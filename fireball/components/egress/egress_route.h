#ifndef FIREBALL_COMPONENTS_EGRESS_EGRESS_ROUTE_H_
#define FIREBALL_COMPONENTS_EGRESS_EGRESS_ROUTE_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace fireball::egress {

enum class EgressMode {
  kDirect,
  kWarp,
  kTor,
};

enum class PrivacyClass {
  kDirect,
  kEncryptedEgress,
  kAnonymityNetwork,
};

struct LoopbackProxyPorts {
  std::uint16_t browser_socks5 = 0;
  std::uint16_t transfer_http_connect = 0;
};

// A route never contains a DIRECT fallback. Chromium uses the SOCKS5 endpoint
// for all supported URL schemes and therefore resolves destination hostnames at
// the proxy. aria2 uses the separate HTTP CONNECT endpoint for HTTP(S) only.
struct EgressRoute {
  EgressMode mode = EgressMode::kDirect;
  PrivacyClass privacy_class = PrivacyClass::kDirect;
  std::optional<LoopbackProxyPorts> proxy;
  bool proxy_resolves_hostnames = false;
  bool allows_peer_to_peer = true;
};

EgressRoute MakeDirectRoute();
std::optional<EgressRoute> MakeWarpRoute(std::uint16_t local_proxy_port);
std::optional<EgressRoute> MakeTorRoute(std::uint16_t socks5_port,
                                       std::uint16_t http_connect_port);

bool IsValidRoute(const EgressRoute& route);
std::string ChromiumProxyRules(const EgressRoute& route);
std::optional<std::string> TransferHttpProxy(const EgressRoute& route);
std::string_view NetworkPolicyOwner(EgressMode mode);

}  // namespace fireball::egress

#endif  // FIREBALL_COMPONENTS_EGRESS_EGRESS_ROUTE_H_
