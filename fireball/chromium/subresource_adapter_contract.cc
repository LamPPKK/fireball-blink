#include "fireball/chromium/subresource_adapter_contract.h"

#include <algorithm>
#include <string_view>
#include <utility>

namespace fireball::chromium {
namespace {

SubresourceDecision Block(std::string_view error_code) {
  SubresourceDecision decision;
  decision.error_code = std::string(error_code);
  return decision;
}

bool IsSafeDataUrl(std::string_view value) {
  return value.size() > 5 && value.size() <= 256 * 1024 &&
         value.starts_with("data:") &&
         std::none_of(value.begin(), value.end(), [](char character) {
           const unsigned char byte = static_cast<unsigned char>(character);
           return byte <= 0x20 || byte == 0x7f;
         });
}

bool UsesSameTransportScheme(std::string_view original,
                             std::string_view rewritten) {
  return original.starts_with("https://") == rewritten.starts_with("https://");
}

}  // namespace

SubresourceDecision EvaluateSubresource(
    const SubresourceInput& input,
    NavigationPolicyEvaluator* evaluator,
    const std::string& applied_proxy_rules) {
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
      .resource_type = input.resource_type,
      .third_party = input.third_party,
      .main_frame = false,
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
  if (policy.action == navigation::RequestAction::kRedirect) {
    if (!policy.redirect_payload.has_value() ||
        !IsSafeDataUrl(*policy.redirect_payload)) {
      return Block("SUBRESOURCE_REDIRECT_INVALID");
    }
    return {SubresourceAction::kRedirectToDataUrl,
            std::move(*policy.redirect_payload), {}};
  }
  if (policy.action != navigation::RequestAction::kAllow ||
      policy.redirect_payload.has_value() || policy.request_url.empty()) {
    return Block("SUBRESOURCE_POLICY_RESPONSE_INVALID");
  }
  if (policy.request_url != input.url) {
    if (!navigation::IsSafeNavigationUrlForHostname(
            policy.request_url, input.destination_hostname) ||
        !UsesSameTransportScheme(input.url, policy.request_url)) {
      return Block("SUBRESOURCE_REWRITE_INVALID");
    }
    return {SubresourceAction::kRewriteSameOrigin,
            std::move(policy.request_url), {}};
  }
  return {SubresourceAction::kAllow, {}, {}};
}

}  // namespace fireball::chromium
