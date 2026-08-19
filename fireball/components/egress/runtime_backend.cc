#include "fireball/components/egress/runtime_backend.h"

#include "fireball/components/egress/socks5_probe.h"
#include "fireball/components/privacy/network_audit.h"

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace fireball::egress {

RuntimeEgressBackend::RuntimeEgressBackend(RuntimeEgressConfig config)
    : config_(std::move(config)) {}

RuntimeEgressBackend::~RuntimeEgressBackend() = default;

std::optional<EgressRoute> RuntimeEgressBackend::Prepare(
    const browser::ProfileId& profile_id,
    EgressMode mode,
    bool has_user_consent,
    std::string* error) {
  if (error == nullptr) {
    return std::nullopt;
  }
  error->clear();
  if (config_.delegate == nullptr) {
    *error = "runtime egress delegate is unavailable";
    return std::nullopt;
  }
  if (mode == EgressMode::kDirect) {
    return MakeDirectRoute();
  }
  if (!privacy::IsNetworkRequestAllowed(
          NetworkPolicyOwner(mode), privacy::NetworkPhase::kPostStartup,
          has_user_consent)) {
    *error = "egress preparation requires an explicit user action";
    return std::nullopt;
  }
  if (mode == EgressMode::kWarp) {
    return MakeWarpRoute(config_.warp_local_proxy_port);
  }
  if (prepared_tor_.contains(profile_id) || active_tor_.contains(profile_id)) {
    *error = "profile already owns a Tor process";
    return std::nullopt;
  }
  auto ports = config_.delegate->AllocateTorPorts(profile_id, error);
  if (!ports.has_value()) {
    if (error->empty()) {
      *error = "could not allocate isolated Tor proxy ports";
    }
    return std::nullopt;
  }
  TorSidecarConfig sidecar_config{
      config_.tor_executable,
      config_.tor_runtime_directory,
      profile_id,
      ports->browser_socks5,
      ports->transfer_http_connect,
      has_user_consent,
  };
  auto sidecar = TorSidecar::Launch(sidecar_config, error);
  if (sidecar == nullptr) {
    return std::nullopt;
  }
  const EgressRoute route = sidecar->route();
  prepared_tor_.emplace(profile_id, std::move(sidecar));
  return route;
}

bool RuntimeEgressBackend::Verify(const browser::ProfileId& profile_id,
                                  const EgressRoute& candidate,
                                  std::string* error) {
  if (error == nullptr) {
    return false;
  }
  error->clear();
  if (config_.delegate == nullptr || !IsValidRoute(candidate)) {
    *error = "runtime egress verification is unavailable";
    return false;
  }
  if (candidate.proxy.has_value() &&
      !ProbeLoopbackSocks5(candidate.proxy->browser_socks5,
                           config_.readiness_timeout, error)) {
    return false;
  }
  auto evidence = config_.delegate->CollectEgressVerificationEvidence(
      profile_id, candidate, error);
  if (!evidence.has_value()) {
    *error = EgressVerificationCodeName(
        EgressVerificationCode::kCollectionFailed);
    return false;
  }
  const EgressVerificationResult verification =
      ValidateEgressEvidence(candidate, *evidence);
  if (!verification.accepted()) {
    *error = EgressVerificationCodeName(verification.code);
    return false;
  }
  return true;
}

bool RuntimeEgressBackend::Activate(const browser::ProfileId& profile_id,
                                    const EgressRoute& candidate,
                                    std::string* error) {
  if (error == nullptr) {
    return false;
  }
  error->clear();
  if (config_.delegate == nullptr || !IsValidRoute(candidate)) {
    *error = "runtime egress activation is unavailable";
    return false;
  }
  auto prepared = prepared_tor_.end();
  if (candidate.mode == EgressMode::kTor) {
    prepared = prepared_tor_.find(profile_id);
    if (prepared == prepared_tor_.end() || active_tor_.contains(profile_id)) {
      *error = "prepared Tor process is missing or conflicts with an active process";
      return false;
    }
  }
  if (!config_.delegate->ApplyChromiumProxyRules(
          profile_id, ChromiumProxyRules(candidate), error)) {
    return false;
  }
  if (candidate.mode == EgressMode::kTor) {
    active_tor_.emplace(profile_id, std::move(prepared->second));
    prepared_tor_.erase(prepared);
  }
  return true;
}

void RuntimeEgressBackend::Rollback(const browser::ProfileId& profile_id,
                                    const EgressRoute& candidate) {
  if (candidate.mode == EgressMode::kTor) {
    prepared_tor_.erase(profile_id);
  }
}

void RuntimeEgressBackend::Retire(const browser::ProfileId& profile_id,
                                  const EgressRoute& previous) {
  if (previous.mode == EgressMode::kTor) {
    active_tor_.erase(profile_id);
  }
}

}  // namespace fireball::egress
