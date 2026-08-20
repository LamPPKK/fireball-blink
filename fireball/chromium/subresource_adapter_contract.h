#ifndef FIREBALL_CHROMIUM_SUBRESOURCE_ADAPTER_CONTRACT_H_
#define FIREBALL_CHROMIUM_SUBRESOURCE_ADAPTER_CONTRACT_H_

#include <string>

#include "fireball/browser/domain_model.h"
#include "fireball/chromium/navigation_adapter_contract.h"
#include "fireball/components/navigation/request_policy.h"

namespace fireball::chromium {

// Snapshot of one non-primary-main-frame HTTP(S) request. Chromium converts
// its typed ResourceRequest fields before the single-sequence policy engine is
// called; URLs remain private request data and are never exposed in metrics.
struct SubresourceInput {
  browser::ProfileId profile_id;
  std::string url;
  std::string destination_hostname;
  std::string source_hostname;
  std::string method;
  navigation::RequestResourceType resource_type =
      navigation::RequestResourceType::kOther;
  bool third_party = false;
};

enum class SubresourceAction {
  kAllow,
  kBlock,
  kRedirectToDataUrl,
  kRewriteSameOrigin,
};

struct SubresourceDecision {
  SubresourceAction action = SubresourceAction::kBlock;
  std::string replacement_url;
  std::string error_code;
};

SubresourceDecision EvaluateSubresource(
    const SubresourceInput& input,
    NavigationPolicyEvaluator* evaluator,
    const std::string& applied_proxy_rules);

}  // namespace fireball::chromium

#endif  // FIREBALL_CHROMIUM_SUBRESOURCE_ADAPTER_CONTRACT_H_
