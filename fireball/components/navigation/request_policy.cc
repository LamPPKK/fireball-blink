#include "fireball/components/navigation/request_policy.h"

#include <algorithm>
#include <string_view>
#include <utility>

namespace fireball::navigation {
namespace {

constexpr std::uint32_t kKnownAdblockFlags =
    FIREBALL_ADBLOCK_FLAG_BLOCK | FIREBALL_ADBLOCK_FLAG_EXCEPTION |
    FIREBALL_ADBLOCK_FLAG_IMPORTANT | FIREBALL_ADBLOCK_FLAG_REDIRECT |
    FIREBALL_ADBLOCK_FLAG_REWRITE;

bool IsMethod(std::string_view method) {
  return method == "GET" || method == "POST" || method == "PUT" ||
         method == "DELETE" || method == "HEAD" || method == "OPTIONS" ||
         method == "PATCH";
}

bool IsSafeRedirectPayload(std::string_view value) {
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

RequestPolicyDecision Error(std::string_view code) {
  RequestPolicyDecision result;
  result.error_code = code;
  return result;
}

}  // namespace

std::string_view AdblockRequestType(RequestResourceType type) {
  switch (type) {
    case RequestResourceType::kDocument:
      return "document";
    case RequestResourceType::kSubdocument:
      return "subdocument";
    case RequestResourceType::kScript:
      return "script";
    case RequestResourceType::kStylesheet:
      return "stylesheet";
    case RequestResourceType::kImage:
      return "image";
    case RequestResourceType::kMedia:
      return "media";
    case RequestResourceType::kFont:
      return "font";
    case RequestResourceType::kFetch:
      return "fetch";
    case RequestResourceType::kXmlHttpRequest:
      return "xmlhttprequest";
    case RequestResourceType::kWebSocket:
      return "websocket";
    case RequestResourceType::kOther:
      return "other";
  }
  return "other";
}

RequestPolicy::RequestPolicy(
    const adblock::ProfilePolicy* adblock_policy,
    adblock::NetworkEvaluator* adblock_evaluator,
    const UrlCleaner* url_cleaner,
    const egress::EgressController* egress_controller)
    : adblock_policy_(adblock_policy),
      adblock_evaluator_(adblock_evaluator),
      url_cleaner_(url_cleaner),
      egress_controller_(egress_controller) {}

RequestPolicyDecision RequestPolicy::Evaluate(
    const RequestContext& context) const {
  if (adblock_policy_ == nullptr || url_cleaner_ == nullptr ||
      egress_controller_ == nullptr ||
      !adblock_policy_->HasProfile(context.profile_id) ||
      !url_cleaner_->HasProfile(context.profile_id) ||
      !IsMethod(context.method) ||
      (!context.main_frame && context.source_hostname.empty()) ||
      (!context.source_hostname.empty() &&
       !IsCanonicalNavigationHostname(context.source_hostname)) ||
      !IsSafeNavigationUrlForHostname(context.url,
                                      context.destination_hostname)) {
    return Error("REQUEST_CONTEXT_INVALID");
  }
  const egress::EgressRoute* route =
      egress_controller_->GetRoute(context.profile_id);
  if (route == nullptr || !egress::IsValidRoute(*route)) {
    return Error("EGRESS_ROUTE_UNAVAILABLE");
  }
  const std::string proxy_rules = egress::ChromiumProxyRules(*route);
  if (proxy_rules.empty()) {
    return Error("EGRESS_ROUTE_INVALID");
  }

  RequestPolicyDecision result;
  result.action = RequestAction::kAllow;
  result.request_url = context.url;
  result.proxy_rules = proxy_rules;
  if (context.main_frame && context.method == "GET") {
    UrlCleanResult cleaned = url_cleaner_->Clean(
        context.profile_id, result.request_url, context.destination_hostname);
    if (!cleaned.valid()) {
      return Error("URL_CLEANER_FAILED");
    }
    result.request_url = std::move(cleaned.url);
    result.url_cleaned = cleaned.status == UrlCleanStatus::kCleaned;
    result.removed_parameters = cleaned.removed_parameters;
  }

  std::string source_hostname = context.source_hostname;
  if (source_hostname.empty() && context.main_frame) {
    source_hostname = context.destination_hostname;
  }
  const bool can_evaluate_hosts =
      adblock::IsCanonicalHostname(context.destination_hostname) &&
      adblock::IsCanonicalHostname(source_hostname);
  if (!can_evaluate_hosts ||
      !adblock_policy_->ShouldEvaluate(context.profile_id, source_hostname)) {
    return result;
  }
  if (adblock_evaluator_ == nullptr) {
    return Error("ADBLOCK_ENGINE_UNAVAILABLE");
  }

  adblock::NetworkEvaluation evaluation = adblock_evaluator_->Evaluate(
      {result.request_url, context.destination_hostname, source_hostname,
       AdblockRequestType(context.resource_type), context.method,
       context.third_party});
  if (evaluation.status != adblock::EvaluationStatus::kOk ||
      (evaluation.flags & ~kKnownAdblockFlags) != 0) {
    return Error("ADBLOCK_EVALUATION_FAILED");
  }
  result.adblock_evaluated = true;
  result.adblock_flags = evaluation.flags;

  const bool has_redirect_flag =
      (evaluation.flags & FIREBALL_ADBLOCK_FLAG_REDIRECT) != 0;
  const bool has_rewrite_flag =
      (evaluation.flags & FIREBALL_ADBLOCK_FLAG_REWRITE) != 0;
  if (has_redirect_flag != evaluation.redirect.has_value() ||
      has_rewrite_flag != evaluation.rewritten_url.has_value() ||
      (has_redirect_flag && has_rewrite_flag)) {
    return Error("ADBLOCK_RESPONSE_INVALID");
  }
  if ((evaluation.flags & FIREBALL_ADBLOCK_FLAG_EXCEPTION) != 0) {
    if (has_redirect_flag || has_rewrite_flag) {
      return Error("ADBLOCK_RESPONSE_INVALID");
    }
    return result;
  }
  if (has_redirect_flag) {
    if (context.main_frame || !IsSafeRedirectPayload(*evaluation.redirect)) {
      return Error("ADBLOCK_REDIRECT_INVALID");
    }
    result.action = RequestAction::kRedirect;
    result.redirect_payload = std::move(evaluation.redirect);
    return result;
  }
  if (has_rewrite_flag) {
    if (!IsSafeNavigationUrlForHostname(*evaluation.rewritten_url,
                                        context.destination_hostname) ||
        !UsesSameTransportScheme(result.request_url,
                                 *evaluation.rewritten_url)) {
      return Error("ADBLOCK_REWRITE_INVALID");
    }
    result.request_url = std::move(*evaluation.rewritten_url);
    return result;
  }
  if ((evaluation.flags & FIREBALL_ADBLOCK_FLAG_BLOCK) != 0) {
    result.action = RequestAction::kBlock;
  }
  return result;
}

}  // namespace fireball::navigation
