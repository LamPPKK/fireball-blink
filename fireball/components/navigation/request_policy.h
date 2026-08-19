#ifndef FIREBALL_COMPONENTS_NAVIGATION_REQUEST_POLICY_H_
#define FIREBALL_COMPONENTS_NAVIGATION_REQUEST_POLICY_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "fireball/browser/domain_model.h"
#include "fireball/components/adblock/network_evaluator.h"
#include "fireball/components/adblock/profile_policy.h"
#include "fireball/components/egress/egress_controller.h"
#include "fireball/components/navigation/url_cleaner.h"

namespace fireball::navigation {

enum class RequestResourceType {
  kDocument,
  kSubdocument,
  kScript,
  kStylesheet,
  kImage,
  kMedia,
  kFont,
  kFetch,
  kXmlHttpRequest,
  kWebSocket,
  kOther,
};

enum class RequestAction {
  kAllow,
  kBlock,
  kRedirect,
  kError,
};

struct RequestContext {
  browser::ProfileId profile_id;
  std::string url;
  std::string destination_hostname;
  std::string source_hostname;
  std::string method = "GET";
  RequestResourceType resource_type = RequestResourceType::kOther;
  bool third_party = false;
  bool main_frame = false;
};

// This object is passed directly to the Chromium adapter and may contain the
// post-clean/rewrite request URL. It is not a log/metrics DTO.
struct RequestPolicyDecision {
  RequestAction action = RequestAction::kError;
  std::string request_url;
  std::string proxy_rules;
  std::optional<std::string> redirect_payload;
  bool url_cleaned = false;
  std::size_t removed_parameters = 0;
  bool adblock_evaluated = false;
  std::uint32_t adblock_flags = 0;
  std::string error_code;
};

// Ordered request policy: strict URL/context validation, main-frame URL
// cleanup, per-Profile adblock evaluation, then attachment of the already
// committed Direct/WARP/Tor route. It performs no I/O and retains no URL.
class RequestPolicy final {
 public:
  RequestPolicy(const adblock::ProfilePolicy* adblock_policy,
                adblock::NetworkEvaluator* adblock_evaluator,
                const UrlCleaner* url_cleaner,
                const egress::EgressController* egress_controller);

  RequestPolicyDecision Evaluate(const RequestContext& context) const;

 private:
  const adblock::ProfilePolicy* adblock_policy_;
  adblock::NetworkEvaluator* adblock_evaluator_;
  const UrlCleaner* url_cleaner_;
  const egress::EgressController* egress_controller_;
};

std::string_view AdblockRequestType(RequestResourceType type);

}  // namespace fireball::navigation

#endif  // FIREBALL_COMPONENTS_NAVIGATION_REQUEST_POLICY_H_
