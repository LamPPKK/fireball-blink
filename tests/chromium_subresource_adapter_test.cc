#include "fireball/chromium/subresource_adapter_contract.h"

#include <cassert>
#include <string>
#include <utility>

namespace {

template <typename Id>
Id Parse(const char* value) {
  auto parsed = Id::Parse(std::string(value));
  assert(parsed.has_value());
  return std::move(*parsed);
}

class FakeEvaluator final
    : public fireball::chromium::NavigationPolicyEvaluator {
 public:
  fireball::navigation::RequestPolicyDecision decision;
  fireball::navigation::RequestContext last_context{
      .profile_id = Parse<fireball::browser::ProfileId>(
          "10000000-0000-4000-8000-000000000099"),
      .url = "https://initial.test/",
      .destination_hostname = "initial.test",
      .source_hostname = "source.test",
      .method = "GET",
      .resource_type = fireball::navigation::RequestResourceType::kOther,
      .third_party = false,
      .main_frame = false,
  };

  fireball::navigation::RequestPolicyDecision Evaluate(
      const fireball::navigation::RequestContext& context) override {
    ++calls;
    last_context = context;
    return decision;
  }

  std::string ExpectedProxyRules() const override { return "direct://"; }

  int calls = 0;
};

}  // namespace

int main() {
  using fireball::browser::ProfileId;
  using fireball::chromium::EvaluateSubresource;
  using fireball::chromium::SubresourceAction;
  using fireball::chromium::SubresourceInput;
  using fireball::navigation::RequestAction;
  using fireball::navigation::RequestResourceType;

  const ProfileId profile =
      Parse<ProfileId>("10000000-0000-4000-8000-000000000001");
  const SubresourceInput input{
      .profile_id = profile,
      .url = "https://cdn.third.test/app.js",
      .destination_hostname = "cdn.third.test",
      .source_hostname = "example.test",
      .method = "GET",
      .resource_type = RequestResourceType::kScript,
      .third_party = true,
  };

  auto missing = EvaluateSubresource(input, nullptr, "direct://");
  assert(missing.action == SubresourceAction::kBlock);
  assert(missing.error_code == "PROFILE_POLICY_UNAVAILABLE");

  FakeEvaluator evaluator;
  evaluator.decision.action = RequestAction::kAllow;
  evaluator.decision.request_url = input.url;
  evaluator.decision.proxy_rules = "direct://";
  auto allowed = EvaluateSubresource(input, &evaluator, "direct://");
  assert(allowed.action == SubresourceAction::kAllow);
  assert(evaluator.last_context.profile_id == profile);
  assert(evaluator.last_context.resource_type == RequestResourceType::kScript);
  assert(evaluator.last_context.third_party);
  assert(!evaluator.last_context.main_frame);

  auto mismatch = EvaluateSubresource(
      input, &evaluator, "socks5://127.0.0.1:19050");
  assert(mismatch.action == SubresourceAction::kBlock);
  assert(mismatch.error_code == "EGRESS_BINDING_MISMATCH");

  evaluator.decision.action = RequestAction::kBlock;
  auto blocked = EvaluateSubresource(input, &evaluator, "direct://");
  assert(blocked.action == SubresourceAction::kBlock);
  assert(blocked.error_code == "REQUEST_BLOCKED_BY_POLICY");

  evaluator.decision.action = RequestAction::kRedirect;
  evaluator.decision.redirect_payload =
      "data:application/javascript,console.debug('blocked')";
  auto redirected = EvaluateSubresource(input, &evaluator, "direct://");
  assert(redirected.action == SubresourceAction::kRedirectToDataUrl);
  assert(redirected.replacement_url ==
         "data:application/javascript,console.debug('blocked')");

  evaluator.decision.action = RequestAction::kAllow;
  evaluator.decision.redirect_payload.reset();
  evaluator.decision.request_url = "https://cdn.third.test/clean.js";
  auto rewritten = EvaluateSubresource(input, &evaluator, "direct://");
  assert(rewritten.action == SubresourceAction::kRewriteSameOrigin);
  assert(rewritten.replacement_url == "https://cdn.third.test/clean.js");

  evaluator.decision.request_url = "https://attacker.test/clean.js";
  auto unsafe_rewrite = EvaluateSubresource(input, &evaluator, "direct://");
  assert(unsafe_rewrite.action == SubresourceAction::kBlock);
  assert(unsafe_rewrite.error_code == "SUBRESOURCE_REWRITE_INVALID");

  evaluator.decision.action = RequestAction::kError;
  evaluator.decision.error_code = "ADBLOCK_EVALUATION_FAILED";
  auto failed = EvaluateSubresource(input, &evaluator, "direct://");
  assert(failed.action == SubresourceAction::kBlock);
  assert(failed.error_code == "ADBLOCK_EVALUATION_FAILED");
  return 0;
}
