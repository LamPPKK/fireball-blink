#include "fireball/components/egress/warp_local_proxy.h"

#include "fireball/components/egress/socks5_probe.h"
#include "fireball/components/privacy/network_audit.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace fireball::egress {

std::optional<EgressRoute> VerifyWarpLocalProxy(
    std::uint16_t port,
    bool has_user_consent,
    std::chrono::milliseconds timeout,
    std::string* error) {
  if (error == nullptr) {
    return std::nullopt;
  }
  error->clear();
  if (!privacy::IsNetworkRequestAllowed(
          "fireball.egress.warp", privacy::NetworkPhase::kPostStartup,
          has_user_consent)) {
    *error = "WARP activation requires an explicit user action";
    return std::nullopt;
  }
  auto route = MakeWarpRoute(port);
  if (!route.has_value()) {
    *error = "WARP local proxy port is invalid";
    return std::nullopt;
  }
  if (!ProbeLoopbackSocks5(port, timeout, error)) {
    return std::nullopt;
  }
  return route;
}

}  // namespace fireball::egress
