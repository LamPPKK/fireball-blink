#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "fireball/components/navigation/document_cosmetic_policy.h"
#include "fireball/components/navigation/request_policy.h"

extern "C" FireballAdblockEngine*
fireball_adblock_engine_create_unverified_for_testing(
    const std::uint8_t* rules_data, std::size_t rules_length);

namespace {

bool ResolveRegistrableDomain(const std::uint8_t* hostname_data,
                              std::size_t hostname_length,
                              std::size_t* domain_start,
                              std::size_t* domain_end) {
  if (hostname_data == nullptr || hostname_length == 0 ||
      domain_start == nullptr || domain_end == nullptr) {
    return false;
  }
  const std::string_view hostname(reinterpret_cast<const char*>(hostname_data),
                                  hostname_length);
  const std::size_t last_dot = hostname.rfind('.');
  if (last_dot == std::string_view::npos) {
    *domain_start = 0;
    *domain_end = hostname_length;
    return true;
  }
  const std::size_t previous_dot = hostname.rfind('.', last_dot - 1);
  *domain_start = previous_dot == std::string_view::npos ? 0 : previous_dot + 1;
  *domain_end = hostname_length;
  return true;
}

fireball::navigation::RequestContext ScriptRequest(
    const fireball::browser::ProfileId& profile, std::string url,
    std::string hostname, bool third_party) {
  return {profile,
          std::move(url),
          std::move(hostname),
          "publisher.example",
          "GET",
          fireball::navigation::RequestResourceType::kScript,
          third_party,
          false};
}

}  // namespace

int main() {
  using fireball::adblock::FfiCosmeticEvaluator;
  using fireball::adblock::FfiNetworkEvaluator;
  using fireball::browser::ProfileId;
  using fireball::navigation::DocumentCosmeticPolicy;
  using fireball::navigation::DocumentCosmeticStatus;
  using fireball::navigation::RequestAction;
  using fireball::navigation::RequestPolicy;
  using fireball::navigation::UrlCleaner;

  assert(fireball_adblock_set_domain_resolver(ResolveRegistrableDomain));
  const std::string rules =
      "||ads.example^\n"
      "@@||ads.example/allowed.js$script,domain=publisher.example\n"
      "||tracker.example^$third-party\n"
      "publisher.example##.sponsored\n"
      "##.global-ad\n"
      "@@||nogeneric.example^$generichide\n";
  FireballAdblockEngine* engine =
      fireball_adblock_engine_create_unverified_for_testing(
          reinterpret_cast<const std::uint8_t*>(rules.data()), rules.size());
  assert(engine != nullptr);

  const ProfileId profile =
      *ProfileId::Parse("20000000-0000-4000-8000-000000000001");
  fireball::adblock::ProfilePolicy shields;
  assert(shields.AddProfile(profile));
  UrlCleaner cleaner = UrlCleaner::CreateBuiltIn();
  assert(cleaner.AddProfile(profile));
  fireball::egress::EgressController egress(nullptr);
  assert(egress.AddProfile(profile));
  FfiNetworkEvaluator evaluator(engine);
  RequestPolicy policy(&shields, &evaluator, &cleaner, &egress);
  FfiCosmeticEvaluator cosmetic_evaluator(engine);
  DocumentCosmeticPolicy cosmetic_policy(&shields, &cosmetic_evaluator);

  auto decision = policy.Evaluate(
      {profile,
       "https://publisher.example/article?utm_source=mail&story=1",
       "publisher.example",
       {},
       "GET",
       fireball::navigation::RequestResourceType::kDocument,
       false,
       true});
  assert(decision.action == RequestAction::kAllow);

  auto cosmetic = cosmetic_policy.BeginDocument(
      profile, "https://publisher.example/article", "publisher.example");
  assert(cosmetic.status == DocumentCosmeticStatus::kReady);
  assert(cosmetic.stylesheet == ".sponsored{display:none!important;}\n");
  assert(cosmetic.hidden_selector_count == 1);
  assert(cosmetic.generic_scan_allowed);
  assert(cosmetic.skipped_procedural_action_count == 0);
  assert(!cosmetic.skipped_scriptlets);
  auto generic = cosmetic_policy.MatchGenericSelectors(
      profile, "publisher.example", cosmetic, {"global-ad"}, {});
  assert(generic.status == DocumentCosmeticStatus::kReady);
  assert(generic.stylesheet == ".global-ad{display:none!important;}\n");
  assert(generic.hidden_selector_count == 1);

  cosmetic = cosmetic_policy.BeginDocument(
      profile, "https://nogeneric.example/", "nogeneric.example");
  assert(cosmetic.status == DocumentCosmeticStatus::kReady);
  assert(!cosmetic.generic_scan_allowed);
  assert(decision.url_cleaned && decision.removed_parameters == 1);
  assert(decision.request_url == "https://publisher.example/article?story=1");
  assert(decision.proxy_rules == "direct://");
  assert(decision.adblock_evaluated);

  decision = policy.Evaluate(ScriptRequest(
      profile, "https://ads.example/banner.js", "ads.example", true));
  assert(decision.action == RequestAction::kBlock);
  assert((decision.adblock_flags & FIREBALL_ADBLOCK_FLAG_BLOCK) != 0);

  decision = policy.Evaluate(ScriptRequest(
      profile, "https://ads.example/allowed.js", "ads.example", true));
  assert(decision.action == RequestAction::kAllow);
  assert((decision.adblock_flags & FIREBALL_ADBLOCK_FLAG_EXCEPTION) != 0);

  decision = policy.Evaluate(ScriptRequest(
      profile, "https://tracker.example/pixel.js", "tracker.example", true));
  assert(decision.action == RequestAction::kBlock);
  decision = policy.Evaluate(ScriptRequest(
      profile, "https://tracker.example/pixel.js", "tracker.example", false));
  assert(decision.action == RequestAction::kAllow);

  assert(shields.SetSiteExemption(profile, "publisher.example", true));
  decision = policy.Evaluate(ScriptRequest(
      profile, "https://ads.example/banner.js", "ads.example", true));
  assert(decision.action == RequestAction::kAllow);
  assert(!decision.adblock_evaluated);
  cosmetic = cosmetic_policy.BeginDocument(
      profile, "https://publisher.example/article", "publisher.example");
  assert(cosmetic.status == DocumentCosmeticStatus::kDisabled);

  fireball_adblock_engine_destroy(engine);
  return 0;
}
