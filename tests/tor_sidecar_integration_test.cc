#include "fireball/components/egress/egress_controller.h"
#include "fireball/components/egress/runtime_backend.h"
#include "fireball/components/egress/tor_sidecar.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

volatile sig_atomic_t g_stop = 0;

void HandleStop(int) {
  g_stop = 1;
}

std::optional<std::uint16_t> ParsePort(std::string_view config,
                                       std::string_view directive) {
  const std::string needle = std::string(directive) + " 127.0.0.1:";
  const std::size_t start = config.find(needle);
  if (start == std::string::npos) {
    return std::nullopt;
  }
  std::size_t cursor = start + needle.size();
  unsigned long port = 0;
  while (cursor < config.size() && config[cursor] >= '0' &&
         config[cursor] <= '9') {
    port = port * 10 + static_cast<unsigned long>(config[cursor] - '0');
    ++cursor;
  }
  if (port == 0 || port > 65535) {
    return std::nullopt;
  }
  return static_cast<std::uint16_t>(port);
}

int RunFakeTor(const std::filesystem::path& config_path) {
  std::ifstream input(config_path);
  const std::string config((std::istreambuf_iterator<char>(input)),
                           std::istreambuf_iterator<char>());
  const auto port = ParsePort(config, "SocksPort");
  if (!port.has_value() || config.find("SafeSocks 1") == std::string::npos ||
      config.find("ControlPort 0") == std::string::npos) {
    return 2;
  }

  const int descriptor = socket(AF_INET, SOCK_STREAM, 0);
  if (descriptor < 0) {
    return 3;
  }
  int enabled = 1;
  static_cast<void>(setsockopt(descriptor, SOL_SOCKET, SO_REUSEADDR, &enabled,
                               sizeof(enabled)));
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(*port);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (bind(descriptor, reinterpret_cast<sockaddr*>(&address), sizeof(address)) !=
          0 ||
      listen(descriptor, 1) != 0) {
    close(descriptor);
    return 4;
  }

  struct sigaction action {};
  action.sa_handler = HandleStop;
  sigemptyset(&action.sa_mask);
  action.sa_flags = 0;
  static_cast<void>(sigaction(SIGTERM, &action, nullptr));
  while (!g_stop) {
    fd_set readable;
    FD_ZERO(&readable);
    FD_SET(descriptor, &readable);
    timeval timeout{0, 100000};
    const int selected =
        select(descriptor + 1, &readable, nullptr, nullptr, &timeout);
    if (selected < 0 && errno == EINTR) {
      continue;
    }
    if (selected < 0) {
      close(descriptor);
      return 5;
    }
    if (selected == 0) {
      continue;
    }
    const int client = accept(descriptor, nullptr, nullptr);
    if (client < 0) {
      continue;
    }
    std::array<unsigned char, 3> greeting{};
    const bool valid =
        recv(client, greeting.data(), greeting.size(), MSG_WAITALL) == 3 &&
        greeting == std::array<unsigned char, 3>{0x05, 0x01, 0x00};
    constexpr std::array<unsigned char, 2> response = {0x05, 0x00};
    if (!valid || send(client, response.data(), response.size(), 0) != 2) {
      close(client);
      close(descriptor);
      return 6;
    }
    close(client);
  }
  close(descriptor);
  return 0;
}

std::uint16_t FindAvailableLoopbackPort() {
  const int descriptor = socket(AF_INET, SOCK_STREAM, 0);
  assert(descriptor >= 0);
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = 0;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  assert(bind(descriptor, reinterpret_cast<sockaddr*>(&address),
              sizeof(address)) == 0);
  socklen_t length = sizeof(address);
  assert(getsockname(descriptor, reinterpret_cast<sockaddr*>(&address),
                     &length) == 0);
  const std::uint16_t port = ntohs(address.sin_port);
  close(descriptor);
  return port;
}

fireball::browser::ProfileId Profile() {
  auto parsed = fireball::browser::ProfileId::Parse(
      "30000000-0000-4000-8000-000000000001");
  assert(parsed.has_value());
  return std::move(*parsed);
}

class RuntimeDelegate final : public fireball::egress::RuntimeEgressDelegate {
 public:
  std::optional<fireball::egress::LoopbackProxyPorts> AllocateTorPorts(
      const fireball::browser::ProfileId&,
      std::string*) override {
    return ports;
  }

  std::optional<fireball::egress::EgressVerificationEvidence>
  CollectEgressVerificationEvidence(
      const fireball::browser::ProfileId&,
      const fireball::egress::EgressRoute& candidate,
      std::string*) override {
    ++verify_count;
    fireball::egress::EgressVerificationEvidence evidence;
    evidence.observed_mode = candidate.mode;
    evidence.public_ip_probe_used_candidate_route = true;
    evidence.public_ip = "2606:4700:4700::1111";
    evidence.successful_probe_count = 1;
    if (candidate.proxy.has_value()) {
      evidence.observed_proxy_port = candidate.proxy->browser_socks5;
      evidence.dns_probe_used_candidate_route = true;
      evidence.remote_dns_confirmed = true;
      evidence.local_dns_observed = fail_verify;
      evidence.provider_attestation =
          candidate.mode == fireball::egress::EgressMode::kWarp
              ? fireball::egress::ProviderAttestation::kWarp
              : fireball::egress::ProviderAttestation::kTor;
      evidence.successful_probe_count = 2;
    }
    return evidence;
  }

  bool ApplyChromiumProxyRules(const fireball::browser::ProfileId&,
                               std::string rules,
                               std::string*) override {
    applied_rules.push_back(std::move(rules));
    return true;
  }

  fireball::egress::LoopbackProxyPorts ports;
  int verify_count = 0;
  bool fail_verify = false;
  std::vector<std::string> applied_rules;
};

}  // namespace

int main(int argc, char** argv) {
  if (argc == 3 && std::string_view(argv[1]) == "-f") {
    return RunFakeTor(argv[2]);
  }
  assert(argc == 1);

  std::string pattern = "/tmp/fireball-tor-test-XXXXXX";
  std::vector<char> mutable_pattern(pattern.begin(), pattern.end());
  mutable_pattern.push_back('\0');
  const char* root_value = mkdtemp(mutable_pattern.data());
  assert(root_value != nullptr);
  const std::filesystem::path runtime(root_value);
  assert(chmod(runtime.c_str(), 0700) == 0);

  fireball::egress::TorSidecarConfig config{
      std::filesystem::canonical(argv[0]),
      runtime,
      Profile(),
      FindAvailableLoopbackPort(),
      FindAvailableLoopbackPort(),
      false,
  };
  while (config.http_connect_port == config.socks5_port) {
    config.http_connect_port = FindAvailableLoopbackPort();
  }

  std::string error;
  assert(!fireball::egress::ValidateTorSidecarConfig(config, &error));
  config.has_user_consent = true;
  error.clear();
  assert(fireball::egress::ValidateTorSidecarConfig(config, &error));
  auto sidecar = fireball::egress::TorSidecar::Launch(config, &error);
  assert(sidecar != nullptr && error.empty());
  assert(sidecar->process_id_for_testing() > 0);
  assert(sidecar->route().mode == fireball::egress::EgressMode::kTor);
  const auto data_directory = sidecar->data_directory_for_testing();
  assert(std::filesystem::is_directory(data_directory));
  for (const auto& entry : std::filesystem::directory_iterator(runtime)) {
    assert(!entry.path().filename().string().starts_with(".fireball-torrc-"));
  }

  const int process_id = sidecar->process_id_for_testing();
  sidecar->Stop();
  assert(kill(process_id, 0) == -1 && errno == ESRCH);
  assert(!std::filesystem::exists(data_directory));
  sidecar.reset();
  assert(std::filesystem::is_empty(runtime));

  RuntimeDelegate delegate;
  delegate.ports = {FindAvailableLoopbackPort(), FindAvailableLoopbackPort()};
  while (delegate.ports.transfer_http_connect ==
         delegate.ports.browser_socks5) {
    delegate.ports.transfer_http_connect = FindAvailableLoopbackPort();
  }
  fireball::egress::RuntimeEgressConfig runtime_config;
  runtime_config.tor_executable = std::filesystem::canonical(argv[0]);
  runtime_config.tor_runtime_directory = runtime;
  runtime_config.delegate = &delegate;
  fireball::egress::RuntimeEgressBackend backend(runtime_config);
  fireball::egress::EgressController controller(&backend);
  const auto runtime_profile = Profile();
  assert(controller.AddProfile(runtime_profile));
  delegate.fail_verify = true;
  assert(!controller.Switch(runtime_profile,
                            fireball::egress::EgressMode::kTor,
                            /*profile_is_idle=*/true,
                            /*has_user_consent=*/true, &error));
  assert(delegate.verify_count == 1);
  assert(error == "egress.verification.local_dns_leak");
  assert(delegate.applied_rules.empty());
  assert(std::filesystem::is_empty(runtime));
  delegate.fail_verify = false;
  delegate.ports = {FindAvailableLoopbackPort(), FindAvailableLoopbackPort()};
  while (delegate.ports.transfer_http_connect ==
         delegate.ports.browser_socks5) {
    delegate.ports.transfer_http_connect = FindAvailableLoopbackPort();
  }
  error.clear();
  assert(controller.Switch(runtime_profile, fireball::egress::EgressMode::kTor,
                           /*profile_is_idle=*/true,
                           /*has_user_consent=*/true, &error));
  assert(delegate.verify_count == 2);
  assert(delegate.applied_rules.back() ==
         "socks5://127.0.0.1:" +
             std::to_string(delegate.ports.browser_socks5));
  assert(!std::filesystem::is_empty(runtime));
  assert(controller.Switch(runtime_profile,
                           fireball::egress::EgressMode::kDirect,
                           /*profile_is_idle=*/true,
                           /*has_user_consent=*/false, &error));
  assert(delegate.verify_count == 3);
  assert(delegate.applied_rules.back() == "direct://");
  assert(std::filesystem::is_empty(runtime));
  assert(controller.RemoveProfile(runtime_profile));

  std::filesystem::remove(runtime);
  return 0;
}
