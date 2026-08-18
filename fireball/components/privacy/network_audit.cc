#include "fireball/components/privacy/network_audit.h"

#include <array>

namespace fireball::privacy {
namespace {

struct NetworkPolicyEntry {
  std::string_view owner;
  NetworkPhase phase;
  bool requires_user_consent;
};

#include "fireball/components/privacy/startup_network_policy.inc"

}  // namespace

bool IsNetworkRequestAllowed(std::string_view owner,
                             NetworkPhase phase,
                             bool has_user_consent) {
  if (owner.empty()) {
    return false;
  }
  for (const auto& rule : kNetworkPolicy) {
    if (owner == rule.owner && phase == rule.phase &&
        (!rule.requires_user_consent || has_user_consent)) {
      return true;
    }
  }
  return false;
}

}  // namespace fireball::privacy
