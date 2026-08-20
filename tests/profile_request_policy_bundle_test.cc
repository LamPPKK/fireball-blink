#include "fireball/chromium/profile_request_policy_bundle.h"

#include <cassert>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace {

template <typename Id>
Id Parse(const char* value) {
  auto parsed = Id::Parse(std::string(value));
  assert(parsed.has_value());
  return std::move(*parsed);
}

class FakeNetworkEvaluator final : public fireball::adblock::NetworkEvaluator {
 public:
  fireball::adblock::NetworkEvaluation Evaluate(
      const fireball::adblock::NetworkRequest& request) override {
    ++calls;
    last_hostname = std::string(request.hostname);
    return {fireball::adblock::EvaluationStatus::kOk, flags, std::nullopt,
            std::nullopt};
  }

  int calls = 0;
  std::uint32_t flags = 0;
  std::string last_hostname;
};

struct BackendCounters {
  int prepare_calls = 0;
  int verify_calls = 0;
  int activate_calls = 0;
  int rollback_calls = 0;
  int retire_calls = 0;
};

class FakeEgressBackend final : public fireball::egress::EgressBackend {
 public:
  explicit FakeEgressBackend(std::shared_ptr<BackendCounters> counters)
      : counters_(std::move(counters)) {}

  std::optional<fireball::egress::EgressRoute> Prepare(
      const fireball::browser::ProfileId&,
      fireball::egress::EgressMode mode,
      bool has_user_consent,
      std::string*) override {
    ++counters_->prepare_calls;
    if (!has_user_consent || mode != fireball::egress::EgressMode::kWarp) {
      return std::nullopt;
    }
    return fireball::egress::MakeWarpRoute(41080);
  }

  bool Verify(const fireball::browser::ProfileId&,
              const fireball::egress::EgressRoute&,
              std::string*) override {
    ++counters_->verify_calls;
    return true;
  }

  bool Activate(const fireball::browser::ProfileId&,
                const fireball::egress::EgressRoute&,
                std::string*) override {
    ++counters_->activate_calls;
    return true;
  }

  void Rollback(const fireball::browser::ProfileId&,
                const fireball::egress::EgressRoute&) override {
    ++counters_->rollback_calls;
  }

  void Retire(const fireball::browser::ProfileId&,
              const fireball::egress::EgressRoute&) override {
    ++counters_->retire_calls;
  }

 private:
  std::shared_ptr<BackendCounters> counters_;
};

}  // namespace

int main() {
  using fireball::browser::ProfileId;
  using fireball::chromium::ProfileRequestPolicyBundle;
  using fireball::navigation::RequestAction;
  using fireball::navigation::RequestContext;
  using fireball::navigation::RequestResourceType;

  const ProfileId profile =
      Parse<ProfileId>("10000000-0000-4000-8000-000000000001");
  const ProfileId other =
      Parse<ProfileId>("10000000-0000-4000-8000-000000000002");
  assert(ProfileRequestPolicyBundle::Create(profile, nullptr) == nullptr);

  auto evaluator = std::make_unique<FakeNetworkEvaluator>();
  FakeNetworkEvaluator* evaluator_observer = evaluator.get();
  auto backend_counters = std::make_shared<BackendCounters>();
  auto backend = std::make_unique<FakeEgressBackend>(backend_counters);
  auto bundle = ProfileRequestPolicyBundle::Create(
      profile, std::move(evaluator), std::move(backend));
  assert(bundle != nullptr);
  assert(bundle->profile_id() == profile);
  assert(bundle->ExpectedProxyRules() == "direct://");

  const RequestContext context{
      .profile_id = profile,
      .url = "https://cdn.example.test/app.js",
      .destination_hostname = "cdn.example.test",
      .source_hostname = "example.test",
      .method = "GET",
      .resource_type = RequestResourceType::kScript,
      .third_party = false,
      .main_frame = false,
  };
  auto allowed = bundle->Evaluate(context);
  assert(allowed.action == RequestAction::kAllow);
  assert(allowed.proxy_rules == "direct://");
  assert(evaluator_observer->calls == 1);
  assert(evaluator_observer->last_hostname == "cdn.example.test");

  RequestContext wrong_profile = context;
  wrong_profile.profile_id = other;
  auto rejected = bundle->Evaluate(wrong_profile);
  assert(rejected.action == RequestAction::kError);
  assert(rejected.error_code == "PROFILE_POLICY_BOUNDARY_MISMATCH");
  assert(evaluator_observer->calls == 1);

  assert(bundle->SetBlockingMode(fireball::adblock::BlockingMode::kDisabled));
  auto disabled = bundle->Evaluate(context);
  assert(disabled.action == RequestAction::kAllow);
  assert(!disabled.adblock_evaluated);
  assert(evaluator_observer->calls == 1);
  assert(bundle->SetBlockingMode(fireball::adblock::BlockingMode::kStandard));

  std::string error;
  assert(!bundle->SwitchEgress(fireball::egress::EgressMode::kWarp,
                               false, true, &error));
  assert(bundle->ExpectedProxyRules() == "direct://");
  assert(backend_counters->prepare_calls == 0);
  assert(bundle->SwitchEgress(fireball::egress::EgressMode::kWarp,
                              true, true, &error));
  assert(bundle->ExpectedProxyRules() == "socks5://127.0.0.1:41080");
  assert(backend_counters->prepare_calls == 1);
  assert(backend_counters->verify_calls == 1);
  assert(backend_counters->activate_calls == 1);
  assert(backend_counters->rollback_calls == 0);
  assert(backend_counters->retire_calls == 1);

  auto routed = bundle->Evaluate(context);
  assert(routed.action == RequestAction::kAllow);
  assert(routed.proxy_rules == "socks5://127.0.0.1:41080");
  bundle.reset();
  assert(backend_counters->retire_calls == 2);
  return 0;
}
