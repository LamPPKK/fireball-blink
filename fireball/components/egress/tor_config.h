#ifndef FIREBALL_COMPONENTS_EGRESS_TOR_CONFIG_H_
#define FIREBALL_COMPONENTS_EGRESS_TOR_CONFIG_H_

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace fireball::egress {

// Produces a client-only, loopback-only Tor configuration. Each profile gets a
// distinct Tor process and listener pair, so Chromium does not need SOCKS auth
// (which it does not support) to preserve profile circuit boundaries.
std::optional<std::string> BuildTorConfiguration(
    const std::filesystem::path& data_directory,
    std::uint16_t socks5_port,
    std::uint16_t http_connect_port,
    std::string* error);

}  // namespace fireball::egress

#endif  // FIREBALL_COMPONENTS_EGRESS_TOR_CONFIG_H_
