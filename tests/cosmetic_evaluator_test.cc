#include "fireball/components/adblock/cosmetic_evaluator.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "fireball/components/navigation/document_cosmetic_policy.h"

extern "C" char* fireball_adblock_cosmetic_resources(
    const FireballAdblockEngine*, const std::uint8_t*, std::size_t) {
  return nullptr;
}

extern "C" char* fireball_adblock_hidden_selectors(
    const FireballAdblockEngine*, const std::uint8_t*, std::size_t,
    const std::uint8_t*, std::size_t, const std::uint8_t*, std::size_t) {
  return nullptr;
}

extern "C" void fireball_adblock_string_destroy(char*) {}

namespace {

class FakeCosmeticEvaluator final
    : public fireball::adblock::CosmeticEvaluator {
 public:
  fireball::adblock::CosmeticEvaluation EvaluatePage(
      std::string_view url) override {
    ++page_calls;
    last_url = std::string(url);
    return page_result;
  }

  fireball::adblock::GenericSelectorEvaluation EvaluateGenericSelectors(
      const std::vector<std::string>& classes,
      const std::vector<std::string>& ids,
      const std::vector<std::string>& exceptions) override {
    ++generic_calls;
    last_classes = classes;
    last_ids = ids;
    last_exceptions = exceptions;
    return generic_result;
  }

  fireball::adblock::CosmeticEvaluation page_result;
  fireball::adblock::GenericSelectorEvaluation generic_result;
  int page_calls = 0;
  int generic_calls = 0;
  std::string last_url;
  std::vector<std::string> last_classes;
  std::vector<std::string> last_ids;
  std::vector<std::string> last_exceptions;
};

}  // namespace

int main() {
  using fireball::adblock::CosmeticEvaluationStatus;
  using fireball::browser::ProfileId;
  using fireball::navigation::DocumentCosmeticPolicy;
  using fireball::navigation::DocumentCosmeticStatus;

  auto parsed = fireball::adblock::internal::ParseCosmeticEvaluationJson(
      R"json({"hide_selectors":[".ad","[data-x=\"v\"]"],"procedural_actions":["proc"],"exceptions":[".allow"],"injected_script":"fire(\ud83d\ude80)","generichide":false})json");
  assert(parsed.has_value());
  assert(parsed->status == CosmeticEvaluationStatus::kOk);
  assert(parsed->hide_selectors.size() == 2);
  assert(parsed->hide_selectors[1] == "[data-x=\"v\"]");
  assert(parsed->exceptions == std::vector<std::string>{".allow"});
  assert(parsed->procedural_action_count == 1);
  assert(parsed->has_injected_script);
  assert(!parsed->generic_hiding_disabled);

  assert(
      !fireball::adblock::internal::ParseCosmeticEvaluationJson(
           R"({"hide_selectors":[".z",".a"],"procedural_actions":[],"exceptions":[],"injected_script":"","generichide":false})")
           .has_value());
  assert(
      !fireball::adblock::internal::ParseCosmeticEvaluationJson(
           R"({"hide_selectors":[".a",".a"],"procedural_actions":[],"exceptions":[],"injected_script":"","generichide":false})")
           .has_value());
  assert(
      !fireball::adblock::internal::ParseCosmeticEvaluationJson(
           R"({"hide_selectors":[],"procedural_actions":[],"exceptions":[],"injected_script":"","generichide":false,"unknown":[]})")
           .has_value());
  assert(
      !fireball::adblock::internal::ParseCosmeticEvaluationJson(
           R"({"hide_selectors":[],"hide_selectors":[],"procedural_actions":[],"exceptions":[],"injected_script":"","generichide":false})")
           .has_value());
  assert(
      !fireball::adblock::internal::ParseCosmeticEvaluationJson(
           R"({"hide_selectors":[],"procedural_actions":[],"exceptions":[],"injected_script":"\ud800x","generichide":false})")
           .has_value());
  assert(
      !fireball::adblock::internal::ParseCosmeticEvaluationJson(
           R"({"hide_selectors":[".ad\u0000tail"],"procedural_actions":[],"exceptions":[],"injected_script":"","generichide":false})")
           .has_value());
  assert(
      !fireball::adblock::internal::ParseCosmeticEvaluationJson(
           R"({"hide_selectors":[],"procedural_actions":[],"exceptions":[],"generichide":false})")
           .has_value());
  assert(fireball::adblock::internal::ParseSelectorArrayJson(
             R"(["#hero",".global-ad"])")
             .has_value());
  assert(!fireball::adblock::internal::ParseSelectorArrayJson(R"([".z",".a"])")
              .has_value());
  assert(!fireball::adblock::internal::ParseSelectorArrayJson(R"([".a",])")
              .has_value());

  const ProfileId profile =
      *ProfileId::Parse("20000000-0000-4000-8000-000000000001");
  const ProfileId other_profile =
      *ProfileId::Parse("20000000-0000-4000-8000-000000000002");
  fireball::adblock::ProfilePolicy profile_policy;
  assert(profile_policy.AddProfile(profile));
  assert(profile_policy.AddProfile(other_profile));
  FakeCosmeticEvaluator evaluator;
  evaluator.page_result = {CosmeticEvaluationStatus::kOk,
                           {".sponsored"},
                           {".except"},
                           2,
                           true,
                           false};
  evaluator.generic_result = {CosmeticEvaluationStatus::kOk, {".global-ad"}};
  DocumentCosmeticPolicy policy(&profile_policy, &evaluator);

  auto document = policy.BeginDocument(
      profile, "https://publisher.example/article", "publisher.example");
  assert(document.status == DocumentCosmeticStatus::kReady);
  assert(document.stylesheet == ".sponsored{display:none!important;}\n");
  assert(document.hidden_selector_count == 1);
  assert(document.skipped_procedural_action_count == 2);
  assert(document.skipped_scriptlets);
  assert(document.generic_scan_allowed);
  assert(document.profile_key == profile.value());
  assert(document.hostname == "publisher.example");
  assert(document.stylesheet.find("https://") == std::string::npos);
  assert(evaluator.last_url == "https://publisher.example/article");

  auto generic = policy.MatchGenericSelectors(
      profile, "publisher.example", document, {"global-ad"}, {"hero"});
  assert(generic.status == DocumentCosmeticStatus::kReady);
  assert(generic.stylesheet == ".global-ad{display:none!important;}\n");
  assert(generic.hidden_selector_count == 1);
  assert(evaluator.last_classes == std::vector<std::string>{"global-ad"});
  assert(evaluator.last_ids == std::vector<std::string>{"hero"});
  assert(evaluator.last_exceptions == std::vector<std::string>{".except"});

  assert(profile_policy.SetSiteExemption(profile, "publisher.example", true));
  const int generic_calls_before_disable = evaluator.generic_calls;
  generic = policy.MatchGenericSelectors(profile, "publisher.example", document,
                                         {"global-ad"}, {});
  assert(generic.status == DocumentCosmeticStatus::kDisabled);
  assert(evaluator.generic_calls == generic_calls_before_disable);
  assert(profile_policy.SetSiteExemption(profile, "publisher.example", false));

  generic = policy.MatchGenericSelectors(other_profile, "publisher.example",
                                         document, {"global-ad"}, {});
  assert(generic.status == DocumentCosmeticStatus::kError);
  assert(generic.error_code == "COSMETIC_DOCUMENT_MISMATCH");

  evaluator.page_result.generic_hiding_disabled = true;
  document = policy.BeginDocument(
      profile, "https://publisher.example/no-generic", "publisher.example");
  assert(document.status == DocumentCosmeticStatus::kReady);
  assert(!document.generic_scan_allowed);
  const int generic_calls = evaluator.generic_calls;
  generic = policy.MatchGenericSelectors(profile, "publisher.example", document,
                                         {"global-ad"}, {});
  assert(generic.status == DocumentCosmeticStatus::kDisabled);
  assert(evaluator.generic_calls == generic_calls);

  assert(profile_policy.SetSiteExemption(profile, "publisher.example", true));
  const int page_calls = evaluator.page_calls;
  document = policy.BeginDocument(profile, "https://publisher.example/exempt",
                                  "publisher.example");
  assert(document.status == DocumentCosmeticStatus::kDisabled);
  assert(evaluator.page_calls == page_calls);
  assert(profile_policy.SetSiteExemption(profile, "publisher.example", false));

  evaluator.page_result.hide_selectors = {"body{color:red}"};
  document = policy.BeginDocument(profile, "https://publisher.example/unsafe",
                                  "publisher.example");
  assert(document.status == DocumentCosmeticStatus::kError);
  assert(document.error_code == "COSMETIC_STYLESHEET_INVALID");

  DocumentCosmeticPolicy missing_engine(&profile_policy, nullptr);
  document = missing_engine.BeginDocument(
      profile, "https://publisher.example/article", "publisher.example");
  assert(document.status == DocumentCosmeticStatus::kError);
  assert(document.error_code == "COSMETIC_ENGINE_UNAVAILABLE");

  document =
      policy.BeginDocument(profile, "file:///etc/passwd", "publisher.example");
  assert(document.status == DocumentCosmeticStatus::kError);
  assert(document.error_code == "COSMETIC_CONTEXT_INVALID");
  return 0;
}
