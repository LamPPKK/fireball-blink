#ifndef FIREBALL_COMPONENTS_TRANSFER_EGRESS_TRANSFER_POLICY_H_
#define FIREBALL_COMPONENTS_TRANSFER_EGRESS_TRANSFER_POLICY_H_

#include <string>

#include "fireball/components/egress/egress_route.h"
#include "fireball/components/transfer/aria2_sidecar.h"

namespace fireball::transfer {

// Applies the already-verified profile route to a not-yet-launched sidecar.
// Proxied routes use HTTP CONNECT for HTTP(S) and disable peer-to-peer because
// aria2 cannot prove that every BitTorrent peer socket follows that proxy.
bool ApplyEgressRoute(const egress::EgressRoute& route,
                      Aria2SidecarConfig* config,
                      std::string* error);

}  // namespace fireball::transfer

#endif  // FIREBALL_COMPONENTS_TRANSFER_EGRESS_TRANSFER_POLICY_H_
