#include "fireball/chromium/navigation_adapter_contract.h"

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
          "10000000-0000-4000-8000-000000000099")};
  int calls = 0;

  fireball::navigation::RequestPolicyDecision Evaluate(
      const fireball::navigation::RequestContext& context) override {
    ++calls;
    last_context = context;
    return decision;
  }
};

}  // namespace

int main() {
  using fireball::browser::ProfileId;
  using fireball::chromium::EvaluatePrimaryMainFrame;
  using fireball::chromium::NavigationAction;
  using fireball::chromium::NavigationInput;
  using fireball::navigation::RequestAction;

  const ProfileId profile =
      Parse<ProfileId>("10000000-0000-4000-8000-000000000001");
  const NavigationInput input{
      .profile_id = profile,
      .url = "https://example.test/?utm_source=fireball",
      .destination_hostname = "example.test",
      .source_hostname = "source.test",
      .method = "GET",
  };

  auto missing = EvaluatePrimaryMainFrame(input, nullptr, "direct://");
  assert(missing.action == NavigationAction::kBlock);
  assert(missing.error_code == "PROFILE_POLICY_UNAVAILABLE");

  FakeEvaluator evaluator;
  auto invalid_route =
      EvaluatePrimaryMainFrame(input, &evaluator, "direct://\nleak");
  assert(invalid_route.action == NavigationAction::kBlock);
  assert(invalid_route.error_code == "EGRESS_BINDING_UNAVAILABLE");

  evaluator.decision.action = RequestAction::kAllow;
  evaluator.decision.request_url = input.url;
  evaluator.decision.proxy_rules = "direct://";
  auto allowed = EvaluatePrimaryMainFrame(input, &evaluator, "direct://");
  assert(allowed.action == NavigationAction::kProceed);
  assert(evaluator.calls == 1);
  assert(evaluator.last_context.profile_id == profile);
  assert(evaluator.last_context.main_frame);
  assert(!evaluator.last_context.third_party);

  auto mismatch =
      EvaluatePrimaryMainFrame(input, &evaluator, "socks5://127.0.0.1:19050");
  assert(mismatch.action == NavigationAction::kBlock);
  assert(mismatch.error_code == "EGRESS_BINDING_MISMATCH");

  evaluator.decision.request_url = "https://example.test/";
  evaluator.decision.url_cleaned = true;
  auto cleaned = EvaluatePrimaryMainFrame(input, &evaluator, "direct://");
  assert(cleaned.action == NavigationAction::kRestartWithCleanedUrl);
  assert(cleaned.replacement_url == "https://example.test/");

  evaluator.decision.request_url = "https://attacker.test/";
  auto cross_host =
      EvaluatePrimaryMainFrame(input, &evaluator, "direct://");
  assert(cross_host.action == NavigationAction::kBlock);
  assert(cross_host.error_code == "REQUEST_POLICY_URL_INVALID");

  evaluator.decision.request_url = "https://example.test/";

  NavigationInput post = input;
  post.method = "POST";
  auto unsafe_restart =
      EvaluatePrimaryMainFrame(post, &evaluator, "direct://");
  assert(unsafe_restart.action == NavigationAction::kBlock);
  assert(unsafe_restart.error_code == "MAIN_FRAME_REWRITE_UNSUPPORTED");

  evaluator.decision.action = RequestAction::kBlock;
  evaluator.decision.request_url = input.url;
  evaluator.decision.url_cleaned = false;
  auto blocked = EvaluatePrimaryMainFrame(input, &evaluator, "direct://");
  assert(blocked.action == NavigationAction::kBlock);
  assert(blocked.error_code == "REQUEST_BLOCKED_BY_POLICY");

  NavigationInput same_document = input;
  same_document.same_document = true;
  const int calls_before_same_document = evaluator.calls;
  auto same =
      EvaluatePrimaryMainFrame(same_document, &evaluator, "direct://");
  assert(same.action == NavigationAction::kProceed);
  assert(evaluator.calls == calls_before_same_document);

  evaluator.decision.action = RequestAction::kError;
  evaluator.decision.error_code = "ADBLOCK_ENGINE_UNAVAILABLE";
  auto failed = EvaluatePrimaryMainFrame(input, &evaluator, "direct://");
  assert(failed.action == NavigationAction::kBlock);
  assert(failed.error_code == "ADBLOCK_ENGINE_UNAVAILABLE");
  return 0;
}
