#include "fireball/components/egress/egress_controller.h"
#include "fireball/components/egress/egress_route.h"
#include "fireball/components/egress/runtime_backend.h"
#include "fireball/components/egress/tor_config.h"
#include "fireball/components/egress/warp_local_proxy.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

fireball::browser::ProfileId Profile(const char* value) {
  auto parsed = fireball::browser::ProfileId::Parse(std::string(value));
  assert(parsed.has_value());
  return std::move(*parsed);
}

class FakeBackend final : public fireball::egress::EgressBackend {
 public:
  std::optional<fireball::egress::EgressRoute> Prepare(
      const fireball::browser::ProfileId&,
      fireball::egress::EgressMode mode,
      bool,
      std::string*) override {
    events.push_back("prepare");
    if (mode == fireball::egress::EgressMode::kDirect) {
      return fireball::egress::MakeDirectRoute();
    }
    if (mode == fireball::egress::EgressMode::kWarp) {
      return fireball::egress::MakeWarpRoute(40000);
    }
    return fireball::egress::MakeTorRoute(39050, 39051);
  }

  bool Verify(const fireball::browser::ProfileId&,
              const fireball::egress::EgressRoute&,
              std::string* error) override {
    events.push_back("verify");
    if (fail_verify) {
      *error = "leak test failed";
      return false;
    }
    return true;
  }

  bool Activate(const fireball::browser::ProfileId&,
                const fireball::egress::EgressRoute&,
                std::string* error) override {
    events.push_back("activate");
    if (fail_activate) {
      *error = "Chromium route activation failed";
      return false;
    }
    return true;
  }

  void Rollback(const fireball::browser::ProfileId&,
                const fireball::egress::EgressRoute&) override {
    events.push_back("rollback");
  }

  void Retire(const fireball::browser::ProfileId&,
              const fireball::egress::EgressRoute&) override {
    events.push_back("retire");
  }

  bool fail_verify = false;
  bool fail_activate = false;
  std::vector<std::string> events;
};

class OneShotSocksServer final {
 public:
  OneShotSocksServer() {
    descriptor_ = socket(AF_INET, SOCK_STREAM, 0);
    assert(descriptor_ >= 0);
    int enabled = 1;
    assert(setsockopt(descriptor_, SOL_SOCKET, SO_REUSEADDR, &enabled,
                      sizeof(enabled)) == 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = 0;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    assert(bind(descriptor_, reinterpret_cast<sockaddr*>(&address),
                sizeof(address)) == 0);
    assert(listen(descriptor_, 1) == 0);
    socklen_t length = sizeof(address);
    assert(getsockname(descriptor_, reinterpret_cast<sockaddr*>(&address),
                       &length) == 0);
    port_ = ntohs(address.sin_port);
    thread_ = std::thread([this] {
      const int client = accept(descriptor_, nullptr, nullptr);
      assert(client >= 0);
      std::array<unsigned char, 3> greeting{};
      assert(recv(client, greeting.data(), greeting.size(), MSG_WAITALL) == 3);
      assert((greeting ==
              std::array<unsigned char, 3>{0x05, 0x01, 0x00}));
      constexpr std::array<unsigned char, 2> response = {0x05, 0x00};
      assert(send(client, response.data(), response.size(), 0) == 2);
      close(client);
    });
  }

  ~OneShotSocksServer() {
    if (thread_.joinable()) {
      thread_.join();
    }
    close(descriptor_);
  }

  std::uint16_t port() const { return port_; }

 private:
  int descriptor_ = -1;
  std::uint16_t port_ = 0;
  std::thread thread_;
};

class FakeRuntimeDelegate final
    : public fireball::egress::RuntimeEgressDelegate {
 public:
  std::optional<fireball::egress::LoopbackProxyPorts> AllocateTorPorts(
      const fireball::browser::ProfileId&,
      std::string*) override {
    return fireball::egress::LoopbackProxyPorts{39060, 39061};
  }

  std::optional<fireball::egress::EgressVerificationEvidence>
  CollectEgressVerificationEvidence(
      const fireball::browser::ProfileId&,
      const fireball::egress::EgressRoute& candidate,
      std::string* error) override {
    ++verify_count;
    if (fail_collection) {
      *error = "raw provider response must not escape";
      return std::nullopt;
    }
    fireball::egress::EgressVerificationEvidence evidence;
    evidence.observed_mode = candidate.mode;
    evidence.public_ip_probe_used_candidate_route = true;
    evidence.public_ip = "1.1.1.1";
    evidence.successful_probe_count = 1;
    if (candidate.proxy.has_value()) {
      evidence.observed_proxy_port = candidate.proxy->browser_socks5;
      evidence.dns_probe_used_candidate_route = true;
      evidence.remote_dns_confirmed = true;
      evidence.successful_probe_count = 2;
      evidence.provider_attestation =
          candidate.mode == fireball::egress::EgressMode::kWarp
              ? fireball::egress::ProviderAttestation::kWarp
              : fireball::egress::ProviderAttestation::kTor;
    }
    return evidence;
  }

  bool ApplyChromiumProxyRules(const fireball::browser::ProfileId&,
                               std::string proxy_rules,
                               std::string*) override {
    applied_rules.push_back(std::move(proxy_rules));
    return true;
  }

  int verify_count = 0;
  bool fail_collection = false;
  std::vector<std::string> applied_rules;
};

}  // namespace

int main() {
  using fireball::egress::ChromiumProxyRules;
  using fireball::egress::EgressController;
  using fireball::egress::EgressMode;
  using fireball::egress::IsValidRoute;
  using fireball::egress::MakeDirectRoute;
  using fireball::egress::MakeTorRoute;
  using fireball::egress::MakeWarpRoute;
  using fireball::egress::PrivacyClass;
  using fireball::egress::TransferHttpProxy;

  const auto direct = MakeDirectRoute();
  assert(IsValidRoute(direct));
  assert(ChromiumProxyRules(direct) == "direct://");
  assert(!TransferHttpProxy(direct).has_value());
  assert(direct.allows_peer_to_peer);

  const auto warp = MakeWarpRoute(40000);
  assert(warp.has_value() && IsValidRoute(*warp));
  assert(warp->privacy_class == PrivacyClass::kEncryptedEgress);
  assert(ChromiumProxyRules(*warp) == "socks5://127.0.0.1:40000");
  assert(*TransferHttpProxy(*warp) == "http://127.0.0.1:40000");
  assert(!warp->allows_peer_to_peer);
  assert(!MakeWarpRoute(0).has_value());

  const auto tor = MakeTorRoute(39050, 39051);
  assert(tor.has_value() && IsValidRoute(*tor));
  assert(tor->privacy_class == PrivacyClass::kAnonymityNetwork);
  assert(ChromiumProxyRules(*tor) == "socks5://127.0.0.1:39050");
  assert(*TransferHttpProxy(*tor) == "http://127.0.0.1:39051");
  assert(!tor->allows_peer_to_peer);
  assert(!MakeTorRoute(39050, 39050).has_value());

  std::string error;
  auto tor_config = fireball::egress::BuildTorConfiguration(
      "/tmp/fireball/tor-profile", 39050, 39051, &error);
  assert(tor_config.has_value() && error.empty());
  assert(tor_config->find("ClientOnly 1") != std::string::npos);
  assert(tor_config->find("SafeSocks 1") != std::string::npos);
  assert(tor_config->find("SocksPort 127.0.0.1:39050") != std::string::npos);
  assert(tor_config->find("HTTPTunnelPort 127.0.0.1:39051") !=
         std::string::npos);
  assert(tor_config->find("ControlPort 0") != std::string::npos);
  assert(!fireball::egress::BuildTorConfiguration(
              "/tmp/fireball path", 39050, 39051, &error)
              .has_value());

  const auto work = Profile("20000000-0000-4000-8000-000000000001");
  const auto private_profile =
      Profile("20000000-0000-4000-8000-000000000002");
  FakeBackend backend;
  EgressController controller(&backend);
  assert(controller.AddProfile(work));
  assert(controller.AddProfile(private_profile));
  assert(controller.GetRoute(work)->mode == EgressMode::kDirect);
  assert(!controller.Switch(work, EgressMode::kWarp,
                            /*profile_is_idle=*/false,
                            /*has_user_consent=*/true, &error));
  assert(backend.events.empty());
  assert(!controller.Switch(work, EgressMode::kWarp,
                            /*profile_is_idle=*/true,
                            /*has_user_consent=*/false, &error));
  assert(backend.events.empty());

  error.clear();
  assert(controller.Switch(work, EgressMode::kWarp,
                           /*profile_is_idle=*/true,
                           /*has_user_consent=*/true, &error));
  assert((backend.events ==
          std::vector<std::string>{"prepare", "verify", "activate", "retire"}));
  assert(controller.GetRoute(work)->mode == EgressMode::kWarp);
  assert(controller.GetRoute(private_profile)->mode == EgressMode::kDirect);

  backend.events.clear();
  backend.fail_verify = true;
  assert(!controller.Switch(work, EgressMode::kTor,
                            /*profile_is_idle=*/true,
                            /*has_user_consent=*/true, &error));
  assert((backend.events ==
          std::vector<std::string>{"prepare", "verify", "rollback"}));
  assert(controller.GetRoute(work)->mode == EgressMode::kWarp);

  backend.events.clear();
  backend.fail_verify = false;
  backend.fail_activate = true;
  assert(!controller.Switch(work, EgressMode::kTor,
                            /*profile_is_idle=*/true,
                            /*has_user_consent=*/true, &error));
  assert((backend.events == std::vector<std::string>{
                                "prepare", "verify", "activate", "rollback"}));
  assert(controller.GetRoute(work)->mode == EgressMode::kWarp);
  backend.fail_activate = false;

  error.clear();
  assert(!fireball::egress::VerifyWarpLocalProxy(
              40000, false, std::chrono::milliseconds(200), &error)
              .has_value());
  OneShotSocksServer socks;
  error.clear();
  auto verified_warp = fireball::egress::VerifyWarpLocalProxy(
      socks.port(), true, std::chrono::seconds(1), &error);
  assert(verified_warp.has_value() && error.empty());
  assert(verified_warp->mode == EgressMode::kWarp);

  const auto runtime_profile =
      Profile("20000000-0000-4000-8000-000000000003");
  OneShotSocksServer runtime_socks;
  FakeRuntimeDelegate runtime_delegate;
  fireball::egress::RuntimeEgressConfig runtime_config;
  runtime_config.warp_local_proxy_port = runtime_socks.port();
  runtime_config.delegate = &runtime_delegate;
  fireball::egress::RuntimeEgressBackend runtime_backend(runtime_config);
  EgressController runtime_controller(&runtime_backend);
  assert(runtime_controller.AddProfile(runtime_profile));
  assert(runtime_controller.Switch(runtime_profile, EgressMode::kWarp,
                                   /*profile_is_idle=*/true,
                                   /*has_user_consent=*/true, &error));
  assert(runtime_delegate.verify_count == 1);
  assert(runtime_delegate.applied_rules.back() ==
         "socks5://127.0.0.1:" + std::to_string(runtime_socks.port()));
  runtime_delegate.fail_collection = true;
  assert(!runtime_controller.Switch(runtime_profile, EgressMode::kDirect,
                                    /*profile_is_idle=*/true,
                                    /*has_user_consent=*/false, &error));
  assert(error == "egress.verification.collection_failed");
  assert(runtime_controller.GetRoute(runtime_profile)->mode ==
         EgressMode::kWarp);
  assert(runtime_delegate.applied_rules.size() == 1);
  runtime_delegate.fail_collection = false;
  assert(runtime_controller.Switch(runtime_profile, EgressMode::kDirect,
                                   /*profile_is_idle=*/true,
                                   /*has_user_consent=*/false, &error));
  assert(runtime_delegate.verify_count == 3);
  assert(runtime_delegate.applied_rules.back() == "direct://");
  assert(runtime_controller.RemoveProfile(runtime_profile));

  assert(controller.RemoveProfile(work));
  assert(controller.RemoveProfile(private_profile));
  return 0;
}
