#ifndef FIREBALL_COMPONENTS_EGRESS_WARP_LOCAL_PROXY_H_
#define FIREBALL_COMPONENTS_EGRESS_WARP_LOCAL_PROXY_H_

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

#include "fireball/components/egress/egress_route.h"

namespace fireball::egress {

// WARP is a system service. Fireball deliberately does not disconnect or
// reconfigure a user's machine-wide client. This adapter accepts only a
// preconfigured Local proxy mode endpoint after explicit user action and a real
// SOCKS5 readiness negotiation.
std::optional<EgressRoute> VerifyWarpLocalProxy(
    std::uint16_t port,
    bool has_user_consent,
    std::chrono::milliseconds timeout,
    std::string* error);

}  // namespace fireball::egress

#endif  // FIREBALL_COMPONENTS_EGRESS_WARP_LOCAL_PROXY_H_
