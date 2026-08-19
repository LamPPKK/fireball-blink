#include "fireball/components/egress/egress_route.h"

#include <string>

namespace fireball::egress {
namespace {

std::string LoopbackUri(std::string_view scheme, std::uint16_t port) {
  return std::string(scheme) + "://127.0.0.1:" + std::to_string(port);
}

}  // namespace

EgressRoute MakeDirectRoute() {
  return EgressRoute{EgressMode::kDirect, PrivacyClass::kDirect, std::nullopt,
                     false, true};
}

std::optional<EgressRoute> MakeWarpRoute(std::uint16_t local_proxy_port) {
  if (local_proxy_port == 0) {
    return std::nullopt;
  }
  return EgressRoute{
      EgressMode::kWarp,
      PrivacyClass::kEncryptedEgress,
      LoopbackProxyPorts{local_proxy_port, local_proxy_port},
      true,
      false,
  };
}

std::optional<EgressRoute> MakeTorRoute(std::uint16_t socks5_port,
                                       std::uint16_t http_connect_port) {
  if (socks5_port == 0 || http_connect_port == 0 ||
      socks5_port == http_connect_port) {
    return std::nullopt;
  }
  return EgressRoute{
      EgressMode::kTor,
      PrivacyClass::kAnonymityNetwork,
      LoopbackProxyPorts{socks5_port, http_connect_port},
      true,
      false,
  };
}

bool IsValidRoute(const EgressRoute& route) {
  if (route.mode == EgressMode::kDirect) {
    return route.privacy_class == PrivacyClass::kDirect &&
           !route.proxy.has_value() && !route.proxy_resolves_hostnames &&
           route.allows_peer_to_peer;
  }
  if (!route.proxy.has_value() || route.proxy->browser_socks5 == 0 ||
      route.proxy->transfer_http_connect == 0 ||
      !route.proxy_resolves_hostnames || route.allows_peer_to_peer) {
    return false;
  }
  if (route.mode == EgressMode::kWarp) {
    return route.privacy_class == PrivacyClass::kEncryptedEgress &&
           route.proxy->browser_socks5 ==
               route.proxy->transfer_http_connect;
  }
  return route.mode == EgressMode::kTor &&
         route.privacy_class == PrivacyClass::kAnonymityNetwork &&
         route.proxy->browser_socks5 != route.proxy->transfer_http_connect;
}

std::string ChromiumProxyRules(const EgressRoute& route) {
  if (!IsValidRoute(route)) {
    return {};
  }
  if (route.mode == EgressMode::kDirect) {
    return "direct://";
  }
  return LoopbackUri("socks5", route.proxy->browser_socks5);
}

std::optional<std::string> TransferHttpProxy(const EgressRoute& route) {
  if (!IsValidRoute(route) || route.mode == EgressMode::kDirect) {
    return std::nullopt;
  }
  return LoopbackUri("http", route.proxy->transfer_http_connect);
}

std::string_view NetworkPolicyOwner(EgressMode mode) {
  switch (mode) {
    case EgressMode::kWarp:
      return "fireball.egress.warp";
    case EgressMode::kTor:
      return "fireball.egress.tor";
    case EgressMode::kDirect:
      return {};
  }
  return {};
}

}  // namespace fireball::egress
