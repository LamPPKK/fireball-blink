#ifndef FIREBALL_COMPONENTS_PRIVACY_NETWORK_AUDIT_H_
#define FIREBALL_COMPONENTS_PRIVACY_NETWORK_AUDIT_H_

#include <string_view>

namespace fireball::privacy {

enum class NetworkPhase {
  kStartup,
  kPostStartup,
};

// Traffic is allowed only when an owner and phase appear in the generated
// policy and the caller supplies any consent required by that rule.
bool IsNetworkRequestAllowed(std::string_view owner,
                             NetworkPhase phase,
                             bool has_user_consent);

inline bool IsStartupRequestAllowed(std::string_view owner) {
  return IsNetworkRequestAllowed(owner, NetworkPhase::kStartup,
                                 /*has_user_consent=*/false);
}

}  // namespace fireball::privacy

#endif  // FIREBALL_COMPONENTS_PRIVACY_NETWORK_AUDIT_H_
