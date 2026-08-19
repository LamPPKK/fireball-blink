#include "fireball/components/navigation/document_cosmetic_controller.h"

#include <algorithm>
#include <string_view>
#include <utility>
#include <vector>

namespace fireball::navigation {
namespace {

constexpr std::size_t kMaximumTrackedDocuments = 1024;

CosmeticControllerResult Error(std::string_view code) {
  CosmeticControllerResult result;
  result.error_code = code;
  return result;
}

CosmeticControllerResult Disabled() {
  CosmeticControllerResult result;
  result.status = CosmeticControllerStatus::kDisabled;
  return result;
}

CosmeticControllerResult Applied(const DocumentCosmeticPlan& plan,
                                 std::size_t hidden_selector_count) {
  CosmeticControllerResult result;
  result.status = CosmeticControllerStatus::kApplied;
  result.hidden_selector_count = hidden_selector_count;
  result.skipped_procedural_action_count = plan.skipped_procedural_action_count;
  result.skipped_scriptlets = plan.skipped_scriptlets;
  return result;
}

}  // namespace

DocumentCosmeticController::DocumentCosmeticController(
    const browser::BrowserModel* browser_model,
    const DocumentCosmeticPolicy* policy, CosmeticStyleSink* style_sink)
    : browser_model_(browser_model), policy_(policy), style_sink_(style_sink) {}

CosmeticControllerResult DocumentCosmeticController::CommitDocument(
    const browser::ProfileId& profile_id, const browser::TabId& tab_id,
    browser::DocumentId document_id, std::string_view url,
    std::string_view hostname) {
  if (browser_model_ == nullptr || policy_ == nullptr ||
      style_sink_ == nullptr || !ProfileOwnsTab(profile_id, tab_id) ||
      documents_.contains(document_id) ||
      pending_revocations_.contains(document_id)) {
    return Error("COSMETIC_DOCUMENT_CONTEXT_INVALID");
  }
  const bool replacing_tab =
      tab_documents_.contains(tab_id) ||
      std::any_of(pending_revocations_.begin(), pending_revocations_.end(),
                  [&tab_id](const auto& entry) {
                    return entry.second.tab_id == tab_id;
                  });
  if (!replacing_tab && documents_.size() + pending_revocations_.size() >=
                            kMaximumTrackedDocuments) {
    return Error("COSMETIC_DOCUMENT_LIMIT_REACHED");
  }

  DocumentCosmeticPlan plan = policy_->BeginDocument(profile_id, url, hostname);
  const CosmeticRevocationResult previous = RevokeTab(tab_id);
  if (!previous.complete()) {
    return Error("COSMETIC_STYLE_REVOKE_FAILED");
  }
  if (plan.status == DocumentCosmeticStatus::kDisabled) {
    return Disabled();
  }
  if (plan.status != DocumentCosmeticStatus::kReady) {
    return Error(plan.error_code.empty() ? "COSMETIC_POLICY_FAILED"
                                         : plan.error_code);
  }
  if (!plan.stylesheet.empty() &&
      !style_sink_->SetStylesheet(document_id, CosmeticStyleLayer::kDocument,
                                  plan.stylesheet)) {
    return Error("COSMETIC_STYLE_INSTALL_FAILED");
  }

  const std::size_t selector_count = plan.hidden_selector_count;
  DocumentState state{profile_id, tab_id, std::string(hostname),
                      std::move(plan), 0};
  auto [document, inserted] = documents_.emplace(document_id, std::move(state));
  if (!inserted) {
    style_sink_->RemoveDocumentStyles(document_id);
    return Error("COSMETIC_DOCUMENT_CONTEXT_INVALID");
  }
  if (!tab_documents_.emplace(tab_id, std::move(document_id)).second) {
    const bool removed = RevokeDocument(document->first);
    return Error(removed ? "COSMETIC_DOCUMENT_CONTEXT_INVALID"
                         : "COSMETIC_STYLE_REVOKE_FAILED");
  }
  return Applied(document->second.plan, selector_count);
}

CosmeticControllerResult DocumentCosmeticController::ApplyDomSnapshot(
    const browser::DocumentId& document_id, std::uint64_t revision,
    const std::vector<std::string>& classes,
    const std::vector<std::string>& ids) {
  auto document = documents_.find(document_id);
  if (browser_model_ == nullptr || policy_ == nullptr ||
      style_sink_ == nullptr || document == documents_.end() || revision == 0) {
    return Error("COSMETIC_SNAPSHOT_CONTEXT_INVALID");
  }
  DocumentState& state = document->second;
  const auto tab_document = tab_documents_.find(state.tab_id);
  if (!ProfileOwnsTab(state.profile_id, state.tab_id) ||
      tab_document == tab_documents_.end() ||
      tab_document->second != document_id) {
    const bool removed = RevokeDocument(document_id);
    return Error(removed ? "COSMETIC_SNAPSHOT_CONTEXT_INVALID"
                         : "COSMETIC_STYLE_REVOKE_FAILED");
  }
  if (revision <= state.last_dom_revision) {
    return Error("COSMETIC_SNAPSHOT_STALE");
  }
  if (!state.plan.generic_scan_allowed) {
    return Disabled();
  }

  GenericCosmeticPlan generic = policy_->MatchGenericSelectors(
      state.profile_id, state.hostname, state.plan, classes, ids);
  if (generic.status == DocumentCosmeticStatus::kDisabled) {
    const bool removed = RevokeDocument(document_id);
    return removed ? Disabled() : Error("COSMETIC_STYLE_REVOKE_FAILED");
  }
  if (generic.status != DocumentCosmeticStatus::kReady) {
    return Error(generic.error_code.empty() ? "COSMETIC_GENERIC_POLICY_FAILED"
                                            : generic.error_code);
  }
  if (!style_sink_->SetStylesheet(document_id, CosmeticStyleLayer::kGeneric,
                                  generic.stylesheet)) {
    return Error("COSMETIC_STYLE_INSTALL_FAILED");
  }
  state.last_dom_revision = revision;
  return Applied(state.plan, generic.hidden_selector_count);
}

bool DocumentCosmeticController::RevokeDocument(
    const browser::DocumentId& document_id) {
  if (style_sink_ == nullptr) {
    return false;
  }
  const auto document = documents_.find(document_id);
  if (document != documents_.end()) {
    PendingRevocation pending{document->second.profile_id,
                              document->second.tab_id};
    const bool removed = style_sink_->RemoveDocumentStyles(document_id);
    EraseDocument(document_id);
    if (!removed) {
      pending_revocations_.emplace(document_id, std::move(pending));
    }
    return removed;
  }
  const auto pending = pending_revocations_.find(document_id);
  if (pending == pending_revocations_.end()) {
    return false;
  }
  if (!style_sink_->RemoveDocumentStyles(document_id)) {
    return false;
  }
  pending_revocations_.erase(pending);
  return true;
}

CosmeticRevocationResult DocumentCosmeticController::RevokeTab(
    const browser::TabId& tab_id) {
  CosmeticRevocationResult result;
  std::vector<browser::DocumentId> revoked;
  const auto active = tab_documents_.find(tab_id);
  if (active != tab_documents_.end()) {
    revoked.push_back(active->second);
  }
  for (const auto& [document_id, pending] : pending_revocations_) {
    if (pending.tab_id == tab_id) {
      revoked.push_back(document_id);
    }
  }
  result.revoked_documents = revoked.size();
  for (const browser::DocumentId& document_id : revoked) {
    if (!RevokeDocument(document_id)) {
      ++result.sink_failures;
    }
  }
  return result;
}

CosmeticRevocationResult DocumentCosmeticController::RevokeProfile(
    const browser::ProfileId& profile_id) {
  std::vector<browser::DocumentId> revoked;
  revoked.reserve(documents_.size());
  for (const auto& [document_id, state] : documents_) {
    if (state.profile_id == profile_id) {
      revoked.push_back(document_id);
    }
  }
  for (const auto& [document_id, pending] : pending_revocations_) {
    if (pending.profile_id == profile_id) {
      revoked.push_back(document_id);
    }
  }
  CosmeticRevocationResult result;
  result.revoked_documents = revoked.size();
  for (const browser::DocumentId& document_id : revoked) {
    if (!RevokeDocument(document_id)) {
      ++result.sink_failures;
    }
  }
  return result;
}

CosmeticRevocationResult DocumentCosmeticController::RetryPendingRevocations() {
  std::vector<browser::DocumentId> revoked;
  revoked.reserve(pending_revocations_.size());
  for (const auto& entry : pending_revocations_) {
    revoked.push_back(entry.first);
  }
  CosmeticRevocationResult result;
  result.revoked_documents = revoked.size();
  for (const browser::DocumentId& document_id : revoked) {
    if (!RevokeDocument(document_id)) {
      ++result.sink_failures;
    }
  }
  return result;
}

bool DocumentCosmeticController::ProfileOwnsTab(
    const browser::ProfileId& profile_id, const browser::TabId& tab_id) const {
  if (browser_model_ == nullptr) {
    return false;
  }
  const browser::Tab* tab = browser_model_->FindTab(tab_id);
  if (tab == nullptr) {
    return false;
  }
  const browser::Space* space = browser_model_->FindSpace(tab->space_id);
  return space != nullptr && space->profile_id == profile_id;
}

bool DocumentCosmeticController::EraseDocument(
    const browser::DocumentId& document_id) {
  const auto document = documents_.find(document_id);
  if (document == documents_.end()) {
    return false;
  }
  const auto tab = tab_documents_.find(document->second.tab_id);
  if (tab != tab_documents_.end() && tab->second == document_id) {
    tab_documents_.erase(tab);
  }
  documents_.erase(document);
  return true;
}

}  // namespace fireball::navigation
