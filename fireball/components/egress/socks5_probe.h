#ifndef FIREBALL_COMPONENTS_EGRESS_SOCKS5_PROBE_H_
#define FIREBALL_COMPONENTS_EGRESS_SOCKS5_PROBE_H_

#include <chrono>
#include <cstdint>
#include <string>

namespace fireball::egress {

// Connects only to 127.0.0.1 and performs the SOCKS5 no-auth negotiation. It
// does not send a destination request or create external traffic.
bool ProbeLoopbackSocks5(std::uint16_t port,
                         std::chrono::milliseconds timeout,
                         std::string* error);

}  // namespace fireball::egress

#endif  // FIREBALL_COMPONENTS_EGRESS_SOCKS5_PROBE_H_
