#include <cassert>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "fireball/components/navigation/document_cosmetic_controller.h"

namespace {

template <typename Id>
Id Parse(const char* value) {
  auto parsed = Id::Parse(std::string(value));
  assert(parsed.has_value());
  return std::move(*parsed);
}

class FakeCosmeticEvaluator final
    : public fireball::adblock::CosmeticEvaluator {
 public:
  fireball::adblock::CosmeticEvaluation EvaluatePage(
      std::string_view) override {
    ++page_calls;
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
  std::vector<std::string> last_classes;
  std::vector<std::string> last_ids;
  std::vector<std::string> last_exceptions;
};

class FakeStyleSink final : public fireball::navigation::CosmeticStyleSink {
 public:
  bool SetStylesheet(const fireball::browser::DocumentId& document_id,
                     fireball::navigation::CosmeticStyleLayer layer,
                     std::string_view stylesheet) override {
    ++set_calls;
    if (fail_next_set) {
      fail_next_set = false;
      return false;
    }
    if (stylesheet.empty()) {
      const auto document = styles.find(document_id);
      if (document != styles.end()) {
        document->second.erase(layer);
        if (document->second.empty()) {
          styles.erase(document);
        }
      }
    } else {
      styles[document_id][layer] = std::string(stylesheet);
    }
    return true;
  }

  bool RemoveDocumentStyles(
      const fireball::browser::DocumentId& document_id) override {
    ++remove_calls;
    if (fail_next_remove) {
      fail_next_remove = false;
      return false;
    }
    styles.erase(document_id);
    return true;
  }

  std::string Style(const fireball::browser::DocumentId& document_id,
                    fireball::navigation::CosmeticStyleLayer layer) const {
    const auto document = styles.find(document_id);
    if (document == styles.end()) {
      return {};
    }
    const auto style = document->second.find(layer);
    return style == document->second.end() ? std::string{} : style->second;
  }

  std::map<fireball::browser::DocumentId,
           std::map<fireball::navigation::CosmeticStyleLayer, std::string>>
      styles;
  bool fail_next_set = false;
  bool fail_next_remove = false;
  int set_calls = 0;
  int remove_calls = 0;
};

}  // namespace

int main() {
  using fireball::adblock::CosmeticEvaluationStatus;
  using fireball::browser::BrowserModel;
  using fireball::browser::DocumentId;
  using fireball::browser::ProfileId;
  using fireball::browser::SpaceId;
  using fireball::browser::SpaceKind;
  using fireball::browser::StorageMode;
  using fireball::browser::TabId;
  using fireball::navigation::CosmeticControllerStatus;
  using fireball::navigation::CosmeticStyleLayer;
  using fireball::navigation::DocumentCosmeticController;
  using fireball::navigation::DocumentCosmeticPolicy;

  const ProfileId profile =
      Parse<ProfileId>("10000000-0000-4000-8000-000000000001");
  const ProfileId other_profile =
      Parse<ProfileId>("10000000-0000-4000-8000-000000000002");
  const SpaceId space = Parse<SpaceId>("20000000-0000-4000-8000-000000000001");
  const SpaceId other_space =
      Parse<SpaceId>("20000000-0000-4000-8000-000000000002");
  const TabId tab = Parse<TabId>("30000000-0000-4000-8000-000000000001");
  const TabId second_tab = Parse<TabId>("30000000-0000-4000-8000-000000000002");
  const TabId other_tab = Parse<TabId>("30000000-0000-4000-8000-000000000003");
  const DocumentId document_one =
      Parse<DocumentId>("40000000-0000-4000-8000-000000000001");
  const DocumentId document_two =
      Parse<DocumentId>("40000000-0000-4000-8000-000000000002");
  const DocumentId document_three =
      Parse<DocumentId>("40000000-0000-4000-8000-000000000003");
  const DocumentId document_four =
      Parse<DocumentId>("40000000-0000-4000-8000-000000000004");
  const DocumentId document_five =
      Parse<DocumentId>("40000000-0000-4000-8000-000000000005");
  const DocumentId document_six =
      Parse<DocumentId>("40000000-0000-4000-8000-000000000006");
  const DocumentId document_seven =
      Parse<DocumentId>("40000000-0000-4000-8000-000000000007");
  const DocumentId document_eight =
      Parse<DocumentId>("40000000-0000-4000-8000-000000000008");

  BrowserModel browser;
  assert(browser.AddProfile(profile, StorageMode::kPersistent));
  assert(browser.AddProfile(other_profile, StorageMode::kPersistent));
  assert(browser.AddSpace(space, profile, SpaceKind::kRegular));
  assert(browser.AddSpace(other_space, other_profile, SpaceKind::kRegular));
  assert(
      browser.AddTab(tab, space, "https://publisher.example/", "Page", true));
  assert(browser.AddTab(second_tab, space, "https://publisher.example/two",
                        "Page two", false));
  assert(browser.AddTab(other_tab, other_space,
                        "https://publisher.example/other", "Other", true));

  fireball::adblock::ProfilePolicy profile_policy;
  assert(profile_policy.AddProfile(profile));
  assert(profile_policy.AddProfile(other_profile));
  FakeCosmeticEvaluator evaluator;
  evaluator.page_result = {CosmeticEvaluationStatus::kOk,
                           {".sponsored"},
                           {".except"},
                           3,
                           true,
                           false};
  evaluator.generic_result = {CosmeticEvaluationStatus::kOk, {".global-ad"}};
  DocumentCosmeticPolicy policy(&profile_policy, &evaluator);
  FakeStyleSink sink;
  DocumentCosmeticController controller(&browser, &policy, &sink);

  auto result = controller.CommitDocument(profile, tab, document_one,
                                          "https://publisher.example/article",
                                          "publisher.example");
  assert(result.status == CosmeticControllerStatus::kApplied);
  assert(result.hidden_selector_count == 1);
  assert(result.skipped_procedural_action_count == 3);
  assert(result.skipped_scriptlets);
  assert(result.error_code.empty());
  assert(controller.active_document_count() == 1);
  assert(sink.Style(document_one, CosmeticStyleLayer::kDocument) ==
         ".sponsored{display:none!important;}\n");

  result =
      controller.ApplyDomSnapshot(document_one, 1, {"global-ad"}, {"hero"});
  assert(result.status == CosmeticControllerStatus::kApplied);
  assert(result.hidden_selector_count == 1);
  assert(sink.Style(document_one, CosmeticStyleLayer::kGeneric) ==
         ".global-ad{display:none!important;}\n");
  assert(evaluator.last_classes == std::vector<std::string>{"global-ad"});
  assert(evaluator.last_ids == std::vector<std::string>{"hero"});
  assert(evaluator.last_exceptions == std::vector<std::string>{".except"});

  evaluator.generic_result = {CosmeticEvaluationStatus::kOk, {}};
  result = controller.ApplyDomSnapshot(document_one, 2, {"clean"}, {});
  assert(result.status == CosmeticControllerStatus::kApplied);
  assert(result.hidden_selector_count == 0);
  assert(sink.Style(document_one, CosmeticStyleLayer::kGeneric).empty());
  evaluator.generic_result = {CosmeticEvaluationStatus::kOk, {".global-ad"}};

  const int set_calls = sink.set_calls;
  result = controller.ApplyDomSnapshot(document_one, 2, {"late"}, {});
  assert(result.status == CosmeticControllerStatus::kError);
  assert(result.error_code == "COSMETIC_SNAPSHOT_STALE");
  assert(sink.set_calls == set_calls);

  result = controller.CommitDocument(profile, tab, document_two,
                                     "https://publisher.example/next",
                                     "publisher.example");
  assert(result.status == CosmeticControllerStatus::kApplied);
  assert(controller.active_document_count() == 1);
  assert(sink.Style(document_one, CosmeticStyleLayer::kDocument).empty());
  result = controller.ApplyDomSnapshot(document_one, 2, {"global-ad"}, {});
  assert(result.status == CosmeticControllerStatus::kError);
  assert(result.error_code == "COSMETIC_SNAPSHOT_CONTEXT_INVALID");

  result = controller.CommitDocument(other_profile, tab, document_three,
                                     "https://publisher.example/cross-profile",
                                     "publisher.example");
  assert(result.status == CosmeticControllerStatus::kError);
  assert(result.error_code == "COSMETIC_DOCUMENT_CONTEXT_INVALID");
  assert(controller.active_document_count() == 1);

  assert(profile_policy.SetSiteExemption(profile, "publisher.example", true));
  result = controller.ApplyDomSnapshot(document_two, 1, {"global-ad"}, {});
  assert(result.status == CosmeticControllerStatus::kDisabled);
  assert(controller.active_document_count() == 0);
  assert(sink.Style(document_two, CosmeticStyleLayer::kDocument).empty());
  assert(profile_policy.SetSiteExemption(profile, "publisher.example", false));

  evaluator.page_result.generic_hiding_disabled = true;
  result = controller.CommitDocument(profile, tab, document_three,
                                     "https://publisher.example/no-generic",
                                     "publisher.example");
  assert(result.status == CosmeticControllerStatus::kApplied);
  const int generic_calls = evaluator.generic_calls;
  result = controller.ApplyDomSnapshot(document_three, 1, {"global-ad"}, {});
  assert(result.status == CosmeticControllerStatus::kDisabled);
  assert(evaluator.generic_calls == generic_calls);
  assert(controller.active_document_count() == 1);
  assert(!sink.Style(document_three, CosmeticStyleLayer::kDocument).empty());
  const auto tab_revoked = controller.RevokeTab(tab);
  assert(tab_revoked.revoked_documents == 1);
  assert(tab_revoked.complete());
  assert(controller.active_document_count() == 0);

  evaluator.page_result.generic_hiding_disabled = false;
  sink.fail_next_set = true;
  result = controller.CommitDocument(profile, tab, document_four,
                                     "https://publisher.example/install-fail",
                                     "publisher.example");
  assert(result.status == CosmeticControllerStatus::kError);
  assert(result.error_code == "COSMETIC_STYLE_INSTALL_FAILED");
  assert(controller.active_document_count() == 0);

  result = controller.CommitDocument(profile, tab, document_five,
                                     "https://publisher.example/five",
                                     "publisher.example");
  assert(result.status == CosmeticControllerStatus::kApplied);
  result = controller.CommitDocument(profile, second_tab, document_six,
                                     "https://publisher.example/six",
                                     "publisher.example");
  assert(result.status == CosmeticControllerStatus::kApplied);
  auto revoked = controller.RevokeProfile(profile);
  assert(revoked.revoked_documents == 2);
  assert(revoked.complete());
  assert(controller.active_document_count() == 0);

  result = controller.CommitDocument(profile, tab, document_seven,
                                     "https://publisher.example/seven",
                                     "publisher.example");
  assert(result.status == CosmeticControllerStatus::kApplied);
  sink.fail_next_remove = true;
  result = controller.CommitDocument(profile, tab, document_eight,
                                     "https://publisher.example/eight",
                                     "publisher.example");
  assert(result.status == CosmeticControllerStatus::kError);
  assert(result.error_code == "COSMETIC_STYLE_REVOKE_FAILED");
  assert(controller.active_document_count() == 0);
  assert(controller.pending_revocation_count() == 1);
  result = controller.CommitDocument(
      profile, second_tab, document_seven,
      "https://publisher.example/reused-document", "publisher.example");
  assert(result.status == CosmeticControllerStatus::kError);
  assert(result.error_code == "COSMETIC_DOCUMENT_CONTEXT_INVALID");
  const auto retried = controller.RetryPendingRevocations();
  assert(retried.revoked_documents == 1);
  assert(retried.complete());
  assert(controller.pending_revocation_count() == 0);
  assert(sink.Style(document_seven, CosmeticStyleLayer::kDocument).empty());

  result = controller.CommitDocument(profile, second_tab, document_eight,
                                     "https://publisher.example/removed-tab",
                                     "publisher.example");
  assert(result.status == CosmeticControllerStatus::kApplied);
  assert(browser.CloseTab(space, second_tab));
  result = controller.ApplyDomSnapshot(document_eight, 1, {"global-ad"}, {});
  assert(result.status == CosmeticControllerStatus::kError);
  assert(result.error_code == "COSMETIC_SNAPSHOT_CONTEXT_INVALID");
  assert(controller.active_document_count() == 0);
  return 0;
}
