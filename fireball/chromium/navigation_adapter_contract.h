#ifndef FIREBALL_CHROMIUM_NAVIGATION_ADAPTER_CONTRACT_H_
#define FIREBALL_CHROMIUM_NAVIGATION_ADAPTER_CONTRACT_H_

#include <string>
#include <string_view>

#include "fireball/browser/domain_model.h"
#include "fireball/components/navigation/request_policy.h"

namespace fireball::chromium {

// Bounded, Chromium-independent snapshot of one primary-main-frame request.
// The API-facing adapter constructs it from trusted GURL/NavigationHandle
// fields. It is never persisted or emitted to logs.
struct NavigationInput {
  browser::ProfileId profile_id;
  std::string url;
  std::string destination_hostname;
  std::string source_hostname;
  std::string method;
  bool same_document = false;
};

enum class NavigationAction {
  kProceed,
  kBlock,
  kRestartWithCleanedUrl,
};

struct NavigationDecision {
  NavigationAction action = NavigationAction::kBlock;
  std::string replacement_url;
  std::string error_code;
};

// Owned by the BrowserContext binding. Production implementations adapt the
// immutable RequestPolicy bundle associated with exactly one Chromium Profile.
class NavigationPolicyEvaluator {
 public:
  virtual ~NavigationPolicyEvaluator() = default;
  virtual navigation::RequestPolicyDecision Evaluate(
      const navigation::RequestContext& context) = 0;
};

bool IsValidAppliedProxyRules(std::string_view value);

// Converts a Fireball policy result into the small action set supported by a
// primary-main-frame NavigationThrottle. Route equality is mandatory: policy
// evaluation cannot proceed through a proxy configuration that the owning
// Chromium Profile has not confirmed as applied.
NavigationDecision EvaluatePrimaryMainFrame(
    const NavigationInput& input,
    NavigationPolicyEvaluator* evaluator,
    const std::string& applied_proxy_rules);

}  // namespace fireball::chromium

#endif  // FIREBALL_CHROMIUM_NAVIGATION_ADAPTER_CONTRACT_H_
