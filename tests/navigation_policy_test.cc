#include "fireball/components/navigation/request_policy.h"

#include <cassert>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace {

class FakeNetworkEvaluator final
    : public fireball::adblock::NetworkEvaluator {
 public:
  fireball::adblock::NetworkEvaluation Evaluate(
      const fireball::adblock::NetworkRequest& request) override {
    ++calls;
    last_url = std::string(request.url);
    last_hostname = std::string(request.hostname);
    last_source = std::string(request.source_hostname);
    last_type = std::string(request.request_type);
    last_method = std::string(request.method);
    last_third_party = request.third_party;
    return next;
  }

  fireball::adblock::NetworkEvaluation next{
      fireball::adblock::EvaluationStatus::kOk, 0, std::nullopt,
      std::nullopt};
  int calls = 0;
  std::string last_url;
  std::string last_hostname;
  std::string last_source;
  std::string last_type;
  std::string last_method;
  bool last_third_party = false;
};

class FakeEgressBackend final : public fireball::egress::EgressBackend {
 public:
  std::optional<fireball::egress::EgressRoute> Prepare(
      const fireball::browser::ProfileId&,
      fireball::egress::EgressMode mode,
      bool,
      std::string*) override {
    if (mode == fireball::egress::EgressMode::kWarp) {
      return fireball::egress::MakeWarpRoute(41000);
    }
    if (mode == fireball::egress::EgressMode::kTor) {
      return fireball::egress::MakeTorRoute(42000, 42001);
    }
    return fireball::egress::MakeDirectRoute();
  }

  bool Verify(const fireball::browser::ProfileId&,
              const fireball::egress::EgressRoute&,
              std::string*) override {
    return true;
  }

  bool Activate(const fireball::browser::ProfileId&,
                const fireball::egress::EgressRoute&,
                std::string*) override {
    return true;
  }

  void Rollback(const fireball::browser::ProfileId&,
                const fireball::egress::EgressRoute&) override {}
  void Retire(const fireball::browser::ProfileId&,
              const fireball::egress::EgressRoute&) override {}
};

fireball::navigation::RequestContext MainDocument(
    const fireball::browser::ProfileId& profile,
    std::string url,
    std::string hostname) {
  return {profile,
          std::move(url),
          std::move(hostname),
          {},
          "GET",
          fireball::navigation::RequestResourceType::kDocument,
          false,
          true};
}

}  // namespace

int main() {
  using fireball::adblock::BlockingMode;
  using fireball::adblock::EvaluationStatus;
  using fireball::browser::ProfileId;
  using fireball::egress::EgressController;
  using fireball::egress::EgressMode;
  using fireball::navigation::ExtractNavigationHostname;
  using fireball::navigation::RequestAction;
  using fireball::navigation::RequestContext;
  using fireball::navigation::RequestPolicy;
  using fireball::navigation::RequestResourceType;
  using fireball::navigation::UrlCleanStatus;
  using fireball::navigation::UrlCleaner;

  const ProfileId profile =
      *ProfileId::Parse("10000000-0000-4000-8000-000000000001");
  const ProfileId unknown =
      *ProfileId::Parse("10000000-0000-4000-8000-000000000099");

  assert(ExtractNavigationHostname("https://Publisher.Example:443/path") ==
         std::optional<std::string>("publisher.example"));
  assert(ExtractNavigationHostname("http://[::1]:8080/path") ==
         std::optional<std::string>("::1"));
  assert(!ExtractNavigationHostname("https://user:secret@example.test/")
              .has_value());
  assert(!ExtractNavigationHostname("file:///etc/passwd").has_value());

  assert(!UrlCleaner::Create("bad-version", {"utm_source"}).has_value());
  assert(!UrlCleaner::Create("1.0", {"utm_source", "UTM_SOURCE"})
              .has_value());
  auto custom = UrlCleaner::Create("1.0.0", {"Track", "utm_source"});
  assert(custom.has_value() && custom->rules_version() == "1.0.0");

  UrlCleaner cleaner = UrlCleaner::CreateBuiltIn();
  assert(cleaner.rules_version() == "2026.8.1");
  assert(cleaner.AddProfile(profile));
  auto cleaned = cleaner.Clean(
      profile,
      "https://publisher.example/article?utm_source=mail&keep=1&%66bclid=x#top",
      "publisher.example");
  assert(cleaned.status == UrlCleanStatus::kCleaned);
  assert(cleaned.removed_parameters == 2);
  assert(cleaned.url == "https://publisher.example/article?keep=1#top");
  assert(cleaner.SetSiteExemption(profile, "publisher.example", true));
  assert(cleaner.Clean(profile,
                       "https://publisher.example/?utm_source=mail",
                       "publisher.example")
             .status == UrlCleanStatus::kUnchanged);
  assert(cleaner.SetSiteExemption(profile, "publisher.example", false));

  fireball::adblock::ProfilePolicy shields;
  assert(shields.AddProfile(profile));
  FakeNetworkEvaluator evaluator;
  FakeEgressBackend egress_backend;
  EgressController egress(&egress_backend);
  assert(egress.AddProfile(profile));
  RequestPolicy policy(&shields, &evaluator, &cleaner, &egress);

  auto decision = policy.Evaluate(MainDocument(
      profile,
      "https://publisher.example/article?utm_campaign=launch&story=1",
      "publisher.example"));
  assert(decision.action == RequestAction::kAllow);
  assert(decision.url_cleaned && decision.removed_parameters == 1);
  assert(decision.request_url ==
         "https://publisher.example/article?story=1");
  assert(decision.proxy_rules == "direct://");
  assert(decision.adblock_evaluated && evaluator.calls == 1);
  assert(evaluator.last_url == decision.request_url);
  assert(evaluator.last_type == "document" && evaluator.last_method == "GET");

  RequestContext post = MainDocument(
      profile, "https://publisher.example/form?utm_source=keep",
      "publisher.example");
  post.method = "POST";
  decision = policy.Evaluate(post);
  assert(decision.action == RequestAction::kAllow && !decision.url_cleaned);
  assert(decision.request_url == post.url);
  RequestContext unsupported_method = post;
  unsupported_method.method = "CONNECT";
  decision = policy.Evaluate(unsupported_method);
  assert(decision.action == RequestAction::kError);
  assert(decision.error_code == "REQUEST_CONTEXT_INVALID");

  RequestContext script{profile,
                        "https://ads.example/banner.js",
                        "ads.example",
                        "publisher.example",
                        "GET",
                        RequestResourceType::kScript,
                        true,
                        false};
  evaluator.next = {EvaluationStatus::kOk, FIREBALL_ADBLOCK_FLAG_BLOCK,
                    std::nullopt, std::nullopt};
  decision = policy.Evaluate(script);
  assert(decision.action == RequestAction::kBlock);
  assert(decision.adblock_evaluated);
  assert(evaluator.last_type == "script" && evaluator.last_third_party);

  evaluator.next = {EvaluationStatus::kOk,
                    FIREBALL_ADBLOCK_FLAG_BLOCK |
                        FIREBALL_ADBLOCK_FLAG_EXCEPTION,
                    std::nullopt, std::nullopt};
  decision = policy.Evaluate(script);
  assert(decision.action == RequestAction::kAllow);

  evaluator.next = {EvaluationStatus::kOk,
                    FIREBALL_ADBLOCK_FLAG_BLOCK |
                        FIREBALL_ADBLOCK_FLAG_REDIRECT,
                    std::string("data:text/plain;base64,ZmlyZWJhbGw="),
                    std::nullopt};
  decision = policy.Evaluate(script);
  assert(decision.action == RequestAction::kRedirect);
  assert(decision.redirect_payload.has_value());
  evaluator.next = {EvaluationStatus::kOk,
                    FIREBALL_ADBLOCK_FLAG_BLOCK |
                        FIREBALL_ADBLOCK_FLAG_REDIRECT,
                    std::string("data:text/plain;base64,ZmlyZWJhbGw="),
                    std::nullopt};
  decision = policy.Evaluate(MainDocument(
      profile, "https://publisher.example/", "publisher.example"));
  assert(decision.action == RequestAction::kError);
  assert(decision.error_code == "ADBLOCK_REDIRECT_INVALID");

  evaluator.next = {EvaluationStatus::kOk, FIREBALL_ADBLOCK_FLAG_REWRITE,
                    std::nullopt,
                    std::string("https://ads.example/banner.js?clean=1")};
  decision = policy.Evaluate(script);
  assert(decision.action == RequestAction::kAllow);
  assert(decision.request_url == "https://ads.example/banner.js?clean=1");
  evaluator.next = {EvaluationStatus::kOk, FIREBALL_ADBLOCK_FLAG_REWRITE,
                    std::nullopt,
                    std::string("https://attacker.example/banner.js")};
  decision = policy.Evaluate(script);
  assert(decision.action == RequestAction::kError);
  assert(decision.error_code == "ADBLOCK_REWRITE_INVALID");
  assert(decision.error_code.find("attacker") == std::string::npos);
  evaluator.next = {EvaluationStatus::kOk, FIREBALL_ADBLOCK_FLAG_REWRITE,
                    std::nullopt,
                    std::string("http://ads.example/banner.js")};
  decision = policy.Evaluate(script);
  assert(decision.action == RequestAction::kError);
  assert(decision.error_code == "ADBLOCK_REWRITE_INVALID");

  evaluator.next = {EvaluationStatus::kOk, FIREBALL_ADBLOCK_FLAG_REDIRECT,
                    std::string("https://attacker.example/payload"),
                    std::nullopt};
  decision = policy.Evaluate(script);
  assert(decision.action == RequestAction::kError);
  assert(decision.error_code == "ADBLOCK_REDIRECT_INVALID");

  assert(shields.SetSiteExemption(profile, "publisher.example", true));
  const int calls_before_exemption = evaluator.calls;
  decision = policy.Evaluate(script);
  assert(decision.action == RequestAction::kAllow);
  assert(!decision.adblock_evaluated &&
         evaluator.calls == calls_before_exemption);
  assert(shields.SetSiteExemption(profile, "publisher.example", false));

  std::string egress_error;
  assert(egress.Switch(profile, EgressMode::kWarp, true, true, &egress_error));
  evaluator.next = {EvaluationStatus::kOk, 0, std::nullopt, std::nullopt};
  decision = policy.Evaluate(script);
  assert(decision.action == RequestAction::kAllow);
  assert(decision.proxy_rules == "socks5://127.0.0.1:41000");
  assert(egress.Switch(profile, EgressMode::kTor, true, true, &egress_error));
  decision = policy.Evaluate(script);
  assert(decision.action == RequestAction::kAllow);
  assert(decision.proxy_rules == "socks5://127.0.0.1:42000");

  RequestContext missing_source = script;
  missing_source.source_hostname.clear();
  decision = policy.Evaluate(missing_source);
  assert(decision.action == RequestAction::kError);
  assert(decision.error_code == "REQUEST_CONTEXT_INVALID");
  decision = policy.Evaluate(MainDocument(
      unknown, "https://publisher.example/", "publisher.example"));
  assert(decision.action == RequestAction::kError);

  decision = policy.Evaluate(MainDocument(
      profile, "https://user:secret@publisher.example/", "publisher.example"));
  assert(decision.action == RequestAction::kError);

  const int calls_before_ip = evaluator.calls;
  decision = policy.Evaluate(
      MainDocument(profile, "http://[::1]:8080/?utm_source=local", "::1"));
  assert(decision.action == RequestAction::kAllow);
  assert(decision.request_url == "http://[::1]:8080/");
  assert(!decision.adblock_evaluated && evaluator.calls == calls_before_ip);

  assert(shields.SetMode(profile, BlockingMode::kDisabled));
  RequestPolicy no_engine(&shields, nullptr, &cleaner, &egress);
  decision = no_engine.Evaluate(script);
  assert(decision.action == RequestAction::kAllow);
  assert(shields.SetMode(profile, BlockingMode::kStandard));
  decision = no_engine.Evaluate(script);
  assert(decision.action == RequestAction::kError);
  assert(decision.error_code == "ADBLOCK_ENGINE_UNAVAILABLE");
  return 0;
}
