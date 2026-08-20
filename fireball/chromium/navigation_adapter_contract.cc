#include "fireball/chromium/navigation_adapter_contract.h"

#include <string_view>
#include <utility>

namespace fireball::chromium {
namespace {

NavigationDecision Block(std::string_view error_code) {
  NavigationDecision decision;
  decision.error_code = std::string(error_code);
  return decision;
}

}  // namespace

bool IsValidAppliedProxyRules(std::string_view value) {
  if (value.empty() || value.size() > 4096) {
    return false;
  }
  for (const char character : value) {
    const unsigned char byte = static_cast<unsigned char>(character);
    if (byte < 0x20 || byte == 0x7f) {
      return false;
    }
  }
  return true;
}

NavigationDecision EvaluatePrimaryMainFrame(
    const NavigationInput& input,
    NavigationPolicyEvaluator* evaluator,
    const std::string& applied_proxy_rules) {
  if (input.same_document) {
    return {NavigationAction::kProceed, {}, {}};
  }
  if (evaluator == nullptr) {
    return Block("PROFILE_POLICY_UNAVAILABLE");
  }
  if (!IsValidAppliedProxyRules(applied_proxy_rules)) {
    return Block("EGRESS_BINDING_UNAVAILABLE");
  }

  navigation::RequestContext context{
      .profile_id = input.profile_id,
      .url = input.url,
      .destination_hostname = input.destination_hostname,
      .source_hostname = input.source_hostname,
      .method = input.method,
      .resource_type = navigation::RequestResourceType::kDocument,
      .third_party = false,
      .main_frame = true,
  };
  navigation::RequestPolicyDecision policy = evaluator->Evaluate(context);
  if (policy.action == navigation::RequestAction::kError) {
    return Block(policy.error_code.empty() ? "REQUEST_POLICY_FAILED"
                                           : policy.error_code);
  }
  if (!IsValidAppliedProxyRules(policy.proxy_rules) ||
      policy.proxy_rules != applied_proxy_rules) {
    return Block("EGRESS_BINDING_MISMATCH");
  }
  if (policy.action == navigation::RequestAction::kBlock) {
    return Block("REQUEST_BLOCKED_BY_POLICY");
  }
  if (policy.action == navigation::RequestAction::kRedirect ||
      policy.redirect_payload.has_value()) {
    return Block("MAIN_FRAME_REDIRECT_UNSUPPORTED");
  }
  if (policy.request_url.empty() ||
      !navigation::IsSafeNavigationUrlForHostname(
          policy.request_url, input.destination_hostname)) {
    return Block("REQUEST_POLICY_URL_INVALID");
  }
  if (policy.request_url != input.url) {
    if (input.method != "GET" || !policy.url_cleaned) {
      return Block("MAIN_FRAME_REWRITE_UNSUPPORTED");
    }
    return {NavigationAction::kRestartWithCleanedUrl,
            std::move(policy.request_url), {}};
  }
  if (policy.action != navigation::RequestAction::kAllow) {
    return Block("REQUEST_POLICY_ACTION_INVALID");
  }
  return {NavigationAction::kProceed, {}, {}};
}

}  // namespace fireball::chromium
