#include "fireball/components/transfer/egress_transfer_policy.h"

#include "fireball/components/egress/egress_route.h"

#include <string>

namespace fireball::transfer {

bool ApplyEgressRoute(const egress::EgressRoute& route,
                      Aria2SidecarConfig* config,
                      std::string* error) {
  if (config == nullptr || error == nullptr) {
    return false;
  }
  error->clear();
  if (!egress::IsValidRoute(route)) {
    *error = "cannot apply an invalid egress route to aria2";
    return false;
  }
  config->outbound_http_proxy = egress::TransferHttpProxy(route);
  config->allow_peer_to_peer = route.allows_peer_to_peer;
  return true;
}

}  // namespace fireball::transfer
