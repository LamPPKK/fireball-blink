#ifndef FIREBALL_COMPONENTS_PRIVACY_NETWORK_AUDIT_H_
#define FIREBALL_COMPONENTS_PRIVACY_NETWORK_AUDIT_H_

#include <string_view>

namespace fireball::privacy {

// Returns true only for startup traffic with an explicit Fireball owner.
// F0 deliberately defaults to deny until the network audit is populated.
bool IsStartupRequestAllowed(std::string_view owner);

}  // namespace fireball::privacy

#endif  // FIREBALL_COMPONENTS_PRIVACY_NETWORK_AUDIT_H_
