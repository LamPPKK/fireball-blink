#include "fireball/chromium/fireball_cosmetic_controller_bridge.h"

#include <cstddef>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "fireball/chromium/profile_policy_binding.h"
#include "url/gurl.h"

namespace fireball::chromium {
namespace {

constexpr std::size_t kMaximumTrackedDocuments = 1024;

navigation::CosmeticControllerResult Error(std::string_view error_code) {
  navigation::CosmeticControllerResult result;
  result.error_code = error_code;
  return result;
}

navigation::CosmeticControllerResult Disabled() {
  navigation::CosmeticControllerResult result;
  result.status = navigation::CosmeticControllerStatus::kDisabled;
  return result;
}

navigation::CosmeticControllerResult
Applied(const navigation::DocumentCosmeticPlan &plan,
        std::size_t hidden_selector_count) {
  navigation::CosmeticControllerResult result;
  result.status = navigation::CosmeticControllerStatus::kApplied;
  result.hidden_selector_count = hidden_selector_count;
  result.skipped_procedural_action_count = plan.skipped_procedural_action_count;
  result.skipped_scriptlets = plan.skipped_scriptlets;
  return result;
}

} // namespace

// static
std::unique_ptr<FireballCosmeticControllerBridge>
FireballCosmeticControllerBridge::Create(
    content::WebContents &web_contents,
    const browser::BrowserModel &browser_model,
    const navigation::DocumentCosmeticPolicy &policy,
    browser::ProfileId profile_id, browser::TabId tab_id,
    FireballCosmeticControllerClient &client) {
  ProfilePolicyBinding *binding =
      ProfilePolicyBinding::Get(*web_contents.GetBrowserContext());
  TabWebContentsBinding *tab_binding =
      TabWebContentsBinding::FromWebContents(&web_contents);
  if (binding == nullptr || binding->profile_id() != profile_id ||
      tab_binding == nullptr || !tab_binding->Matches(profile_id, tab_id)) {
    return nullptr;
  }
  std::optional<std::uint64_t> tab_claim =
      tab_binding->AcquireCosmeticController();
  if (!tab_claim.has_value()) {
    return nullptr;
  }
  std::unique_ptr<FireballCosmeticControllerBridge> bridge(
      new FireballCosmeticControllerBridge(
          browser_model, policy, std::move(profile_id), std::move(tab_id),
          client, tab_binding->GetWeakPtr(), *tab_claim));
  if (!bridge->Initialize(web_contents)) {
    return nullptr;
  }
  return bridge;
}

FireballCosmeticControllerBridge::FireballCosmeticControllerBridge(
    const browser::BrowserModel &browser_model,
    const navigation::DocumentCosmeticPolicy &policy,
    browser::ProfileId profile_id, browser::TabId tab_id,
    FireballCosmeticControllerClient &client,
    base::WeakPtr<TabWebContentsBinding> tab_binding, std::uint64_t tab_claim)
    : browser_model_(&browser_model), policy_(&policy), client_(&client),
      tab_binding_(std::move(tab_binding)), tab_claim_(tab_claim),
      profile_id_(std::move(profile_id)), tab_id_(std::move(tab_id)) {}

FireballCosmeticControllerBridge::~FireballCosmeticControllerBridge() {
  weak_factory_.InvalidateWeakPtrs();
  for (const auto &[document_id, tracked] : documents_) {
    content::RenderFrameHost *frame =
        tracked.document.AsRenderFrameHostIfValid();
    if (frame == nullptr) {
      continue;
    }
    FireballCosmeticDocumentHost *host =
        FireballCosmeticDocumentHost::GetForCurrentDocument(frame);
    if (host != nullptr && host->document_id() == document_id) {
      host->ResetForController();
    }
  }
  if (lifecycle_owner_) {
    lifecycle_owner_->Shutdown();
    lifecycle_owner_.reset();
  }
  if (tab_binding_) {
    tab_binding_->ReleaseCosmeticController(tab_claim_);
  }
}

bool FireballCosmeticControllerBridge::Initialize(
    content::WebContents &web_contents) {
  if (!ContextValid()) {
    return false;
  }
  lifecycle_owner_ =
      std::make_unique<FireballCosmeticLifecycleOwner>(web_contents, *this);
  lifecycle_owner_->Start();
  return true;
}

bool FireballCosmeticControllerBridge::RefreshDomSnapshot() {
  return state_.active_document_id().has_value() &&
         StartDomCollection(*state_.active_document_id());
}

bool FireballCosmeticControllerBridge::ApplyDomSnapshot(
    const browser::DocumentId &document_id, CosmeticDomSnapshot snapshot) {
  auto tracked = documents_.find(document_id);
  if (!ContextValid()) {
    ResetAllDocuments();
    Publish(document_id, Error("COSMETIC_SNAPSHOT_CONTEXT_INVALID"));
    return false;
  }
  if (tracked == documents_.end() || snapshot.revision == 0) {
    Publish(document_id, Error("COSMETIC_SNAPSHOT_CONTEXT_INVALID"));
    return false;
  }
  if (!state_.ready()) {
    Publish(document_id, Error("COSMETIC_CONTROLLER_BUSY"));
    return false;
  }
  if (!state_.active_document_id().has_value() ||
      *state_.active_document_id() != document_id) {
    Publish(document_id, Error("COSMETIC_DOCUMENT_INACTIVE"));
    return false;
  }
  FireballCosmeticDocumentHost *host =
      ResolveHost(document_id, tracked->second.document);
  if (host == nullptr) {
    state_.Fail(document_id);
    Publish(document_id, Error("COSMETIC_DOCUMENT_INACTIVE"));
    return false;
  }
  if (!IsBoundedCosmeticDomSnapshot(snapshot.classes, snapshot.ids)) {
    Publish(document_id, Error("COSMETIC_SNAPSHOT_INVALID"));
    return false;
  }
  if (snapshot.revision <= tracked->second.last_dom_revision) {
    Publish(document_id, Error("COSMETIC_SNAPSHOT_STALE"));
    return false;
  }
  if (!tracked->second.plan.generic_scan_allowed) {
    Publish(document_id, Disabled());
    return true;
  }
  navigation::GenericCosmeticPlan generic =
      policy_->MatchGenericSelectors(profile_id_, tracked->second.plan.hostname,
                                     tracked->second.plan, snapshot.classes,
                                     snapshot.ids);
  if (generic.status == navigation::DocumentCosmeticStatus::kDisabled) {
    return RevokeActiveDocument();
  }
  if (generic.status != navigation::DocumentCosmeticStatus::kReady) {
    Publish(document_id,
            Error(generic.error_code.empty() ? "COSMETIC_GENERIC_POLICY_FAILED"
                                             : generic.error_code));
    return false;
  }
  auto ticket = state_.BeginGenericMutation(document_id, snapshot.revision);
  if (!ticket.has_value()) {
    Publish(document_id, Error("COSMETIC_CONTROLLER_BUSY"));
    return false;
  }
  navigation::CosmeticControllerResult result =
      Applied(tracked->second.plan, generic.hidden_selector_count);
  host->SetStylesheet(
      navigation::CosmeticStyleLayer::kGeneric, std::move(generic.stylesheet),
      base::BindOnce(
          &FireballCosmeticControllerBridge::OnGenericStylesheetApplied,
          weak_factory_.GetWeakPtr(), *ticket, tracked->second.document,
          std::move(result)));
  return true;
}

bool FireballCosmeticControllerBridge::RevokeActiveDocument() {
  if (!state_.active_document_id().has_value()) {
    return false;
  }
  const browser::DocumentId document_id = *state_.active_document_id();
  auto tracked = documents_.find(document_id);
  const content::WeakDocumentPtr document =
      tracked != documents_.end()
          ? tracked->second.document
          : lifecycle_owner_->GetActiveDocument(document_id);
  if (!ContextValid()) {
    ResetAllDocuments();
    Publish(document_id, Error("COSMETIC_DOCUMENT_CONTEXT_INVALID"));
    return false;
  }
  if (state_.phase() == BrowserCosmeticControllerPhase::kFailed) {
    ResetDocument(document_id, document);
    Publish(document_id, Disabled());
    return true;
  }
  if (tracked == documents_.end()) {
    return false;
  }
  FireballCosmeticDocumentHost *host =
      ResolveHost(document_id, tracked->second.document);
  if (host == nullptr) {
    state_.Fail(document_id);
    Publish(document_id, Error("COSMETIC_DOCUMENT_INACTIVE"));
    return false;
  }
  if (!CancelDomCollection(document_id, *host, tracked->second.document)) {
    Publish(document_id, Error("COSMETIC_DOM_CANCELLATION_FAILED"));
    return false;
  }
  if (!state_.ready()) {
    return false;
  }
  auto ticket = state_.BeginRevocation(document_id);
  if (!ticket.has_value()) {
    return false;
  }
  host->Revoke(base::BindOnce(
      &FireballCosmeticControllerBridge::OnDocumentRevoked,
      weak_factory_.GetWeakPtr(), *ticket, tracked->second.document));
  return true;
}

bool FireballCosmeticControllerBridge::RefreshActiveDocument() {
  if (!state_.active_document_id().has_value()) {
    return false;
  }
  const browser::DocumentId document_id = *state_.active_document_id();
  auto tracked = documents_.find(document_id);
  const content::WeakDocumentPtr document =
      tracked != documents_.end()
          ? tracked->second.document
          : lifecycle_owner_->GetActiveDocument(document_id);
  if (!ContextValid()) {
    ResetAllDocuments();
    Publish(document_id, Error("COSMETIC_DOCUMENT_CONTEXT_INVALID"));
    return false;
  }
  FireballCosmeticDocumentHost *host = ResolveHost(document_id, document);
  if (host == nullptr) {
    state_.Fail(document_id);
    Publish(document_id, Error("COSMETIC_DOCUMENT_INACTIVE"));
    return false;
  }
  if (state_.phase() == BrowserCosmeticControllerPhase::kFailed) {
    host->ResetForController();
    documents_.erase(document_id);
    if (!state_.Reset(document_id)) {
      Publish(document_id, Error("COSMETIC_DOCUMENT_REFRESH_FAILED"));
      return false;
    }
    auto activation = state_.BeginActivation(document_id, 0);
    if (!activation.has_value()) {
      Publish(document_id, Error("COSMETIC_DOCUMENT_REFRESH_FAILED"));
      return false;
    }
    host->Activate(base::BindOnce(
        &FireballCosmeticControllerBridge::OnHostReactivatedForRefresh,
        weak_factory_.GetWeakPtr(), *activation, document));
    return true;
  }
  if (!CancelDomCollection(document_id, *host, document)) {
    Publish(document_id, Error("COSMETIC_DOM_CANCELLATION_FAILED"));
    return false;
  }
  if (!state_.ready() || tracked == documents_.end()) {
    return false;
  }
  auto ticket = state_.BeginRevocation(document_id);
  if (!ticket.has_value()) {
    return false;
  }
  host->Revoke(base::BindOnce(
      &FireballCosmeticControllerBridge::OnDocumentRevokedForRefresh,
      weak_factory_.GetWeakPtr(), *ticket, document));
  return true;
}

bool FireballCosmeticControllerBridge::CanActivateCosmeticDocument(
    const browser::DocumentId &) {
  if (ContextValid()) {
    return true;
  }
  ResetAllDocuments();
  return false;
}

void FireballCosmeticControllerBridge::OnCosmeticDocumentReady(
    const browser::DocumentId &document_id,
    FireballCosmeticDocumentHost &host) {
  if (!ContextValid()) {
    ResetAllDocuments();
    Publish(document_id, Error("COSMETIC_DOCUMENT_CONTEXT_INVALID"));
    return;
  }
  auto tracked = documents_.find(document_id);
  const std::uint64_t restored_revision =
      tracked == documents_.end() ? 0 : tracked->second.last_dom_revision;
  auto ticket = state_.BeginActivation(document_id, restored_revision);
  if (!ticket.has_value()) {
    Publish(document_id, Error("COSMETIC_CONTROLLER_BUSY"));
    return;
  }
  PrepareReadyDocument(*ticket, host);
}

void FireballCosmeticControllerBridge::PrepareReadyDocument(
    BrowserCosmeticControllerTicket ticket,
    FireballCosmeticDocumentHost &host) {
  const browser::DocumentId &document_id = ticket.document_id;
  auto tracked = documents_.find(document_id);
  content::WeakDocumentPtr document = host.GetWeakDocumentPtr();
  if (!ContextValid()) {
    ResetAllDocuments();
    Publish(document_id, Error("COSMETIC_DOCUMENT_CONTEXT_INVALID"));
    return;
  }
  if (tracked != documents_.end()) {
    tracked->second.document = document;
    if (!state_.CompleteActivation(ticket, true)) {
      return;
    }
    if (tracked->second.plan.generic_scan_allowed) {
      tracked->second.last_result =
          Applied(tracked->second.plan,
                  tracked->second.plan.hidden_selector_count);
      ScheduleDomCollection(document_id);
    }
    Publish(document_id, tracked->second.last_result);
    return;
  }
  if (!ProfileOwnsTab() || documents_.size() >= kMaximumTrackedDocuments) {
    state_.CompleteActivation(ticket, false);
    Publish(document_id, Error(documents_.size() >= kMaximumTrackedDocuments
                                   ? "COSMETIC_DOCUMENT_LIMIT_REACHED"
                                   : "COSMETIC_DOCUMENT_CONTEXT_INVALID"));
    return;
  }
  content::RenderFrameHost *frame = document.AsRenderFrameHostIfValid();
  if (frame == nullptr || !frame->IsInPrimaryMainFrame() ||
      !frame->IsActive()) {
    state_.CompleteActivation(ticket, false);
    Publish(document_id, Error("COSMETIC_DOCUMENT_INACTIVE"));
    return;
  }
  const GURL &url = frame->GetLastCommittedURL();
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS() || url.host().empty()) {
    state_.CompleteActivation(ticket, false);
    Publish(document_id, Error("COSMETIC_DOCUMENT_URL_INVALID"));
    return;
  }
  navigation::DocumentCosmeticPlan plan =
      policy_->BeginDocument(profile_id_, url.spec(), url.host());
  if (plan.status == navigation::DocumentCosmeticStatus::kDisabled) {
    navigation::CosmeticControllerResult result = Disabled();
    if (!state_.CompleteActivation(ticket, true)) {
      return;
    }
    auto [disabled, inserted] = documents_.emplace(
        document_id, TrackedDocument{document, std::move(plan), result, 0});
    if (!inserted) {
      state_.Fail(document_id);
      Publish(document_id, Error("COSMETIC_DOCUMENT_CONTEXT_INVALID"));
      return;
    }
    Publish(disabled->first, disabled->second.last_result);
    return;
  }
  if (plan.status != navigation::DocumentCosmeticStatus::kReady) {
    state_.CompleteActivation(ticket, false);
    Publish(document_id,
            Error(plan.error_code.empty() ? "COSMETIC_POLICY_FAILED"
                                          : plan.error_code));
    return;
  }
  host.SetStylesheet(
      navigation::CosmeticStyleLayer::kDocument, plan.stylesheet,
      base::BindOnce(
          &FireballCosmeticControllerBridge::OnDocumentStylesheetApplied,
          weak_factory_.GetWeakPtr(), ticket, document, std::move(plan)));
}

void FireballCosmeticControllerBridge::OnCosmeticDocumentSuspended(
    const browser::DocumentId &document_id) {
  if (!state_.Suspend(document_id)) {
    return;
  }
  client_->OnCosmeticControllerSuspended(document_id);
}

void FireballCosmeticControllerBridge::OnCosmeticDocumentDisposed(
    const browser::DocumentId &document_id) {
  state_.Dispose(document_id);
  if (documents_.erase(document_id) == 0) {
    return;
  }
  Publish(document_id, Disabled());
}

void FireballCosmeticControllerBridge::OnCosmeticDocumentFailed(
    const browser::DocumentId &document_id, std::string_view error_code) {
  if (!state_.Fail(document_id)) {
    auto ticket = state_.BeginActivation(document_id, 0);
    if (ticket.has_value()) {
      state_.CompleteActivation(*ticket, false);
    }
  }
  documents_.erase(document_id);
  Publish(document_id,
          Error(error_code.empty() ? "COSMETIC_DOCUMENT_FAILED" : error_code));
}

bool FireballCosmeticControllerBridge::ProfileOwnsTab() const {
  const browser::Tab *tab = browser_model_->FindTab(tab_id_);
  if (tab == nullptr) {
    return false;
  }
  const browser::Space *space = browser_model_->FindSpace(tab->space_id);
  return space != nullptr && space->profile_id == profile_id_;
}

bool FireballCosmeticControllerBridge::ContextValid() const {
  return tab_binding_ && tab_binding_->Matches(profile_id_, tab_id_) &&
         tab_binding_->OwnsCosmeticController(tab_claim_) && ProfileOwnsTab();
}

void FireballCosmeticControllerBridge::ScheduleDomCollection(
    const browser::DocumentId &document_id) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FireballCosmeticControllerBridge::StartDomCollection,
                     weak_factory_.GetWeakPtr(), document_id));
}

bool FireballCosmeticControllerBridge::StartDomCollection(
    const browser::DocumentId &document_id) {
  if (!ContextValid()) {
    ResetAllDocuments();
    return false;
  }
  if (!state_.ready() || !state_.active_document_id().has_value() ||
      *state_.active_document_id() != document_id) {
    return false;
  }
  auto tracked = documents_.find(document_id);
  if (tracked == documents_.end() ||
      !tracked->second.plan.generic_scan_allowed) {
    return false;
  }
  FireballCosmeticDocumentHost *host =
      ResolveHost(document_id, tracked->second.document);
  if (host == nullptr || !host->ready()) {
    return false;
  }
  auto ticket = state_.BeginDomCollection(document_id);
  if (!ticket.has_value()) {
    return false;
  }
  host->CollectDomSnapshot(base::BindOnce(
      &FireballCosmeticControllerBridge::OnDomSnapshotCollected,
      weak_factory_.GetWeakPtr(), *ticket, tracked->second.document));
  return true;
}

bool FireballCosmeticControllerBridge::CancelDomCollection(
    const browser::DocumentId &document_id,
    FireballCosmeticDocumentHost &host,
    const content::WeakDocumentPtr &document) {
  if (!state_.collecting_dom()) {
    return true;
  }
  if (!host.CancelDomSnapshot() ||
      !state_.CancelDomCollection(document_id)) {
    ResetDocument(document_id, document);
    return false;
  }
  return true;
}

void FireballCosmeticControllerBridge::OnDomSnapshotCollected(
    BrowserCosmeticControllerTicket ticket,
    content::WeakDocumentPtr document,
    CosmeticTransportResult transport_result,
    CosmeticDomSnapshot snapshot) {
  if (!ContextValid()) {
    ResetAllDocuments();
    return;
  }
  if (!state_.IsCurrent(ticket)) {
    return;
  }
  const browser::DocumentId &document_id = ticket.document_id;
  content::RenderFrameHost *frame = document.AsRenderFrameHostIfValid();
  if (frame == nullptr || !frame->IsActive()) {
    // The lifecycle owner invalidates the transport before it publishes the
    // matching suspend/dispose transition. That transition owns controller
    // cleanup and BFCache retention for this synchronous callback path.
    return;
  }
  if (ResolveHost(document_id, document) == nullptr) {
    ResetDocument(document_id, document);
    Publish(document_id, Error("COSMETIC_DOCUMENT_INACTIVE"));
    return;
  }
  if (transport_result.status == CosmeticTransportStatus::kLimited) {
    if (!state_.CompleteDomCollection(ticket)) {
      return;
    }
    ClearGenericStylesheetAfterLimit(
        document_id, document, snapshot.revision,
        transport_result.error_code.empty()
            ? "COSMETIC_DOM_LIMIT_EXCEEDED"
            : std::move(transport_result.error_code));
    return;
  }
  if (transport_result.status != CosmeticTransportStatus::kCollected) {
    std::string error_code =
        transport_result.error_code.empty()
            ? "COSMETIC_DOM_COLLECTION_FAILED"
            : std::move(transport_result.error_code);
    ResetDocument(document_id, document);
    Publish(document_id, Error(error_code));
    return;
  }
  if (!state_.CompleteDomCollection(ticket)) {
    return;
  }
  ApplyDomSnapshot(document_id, std::move(snapshot));
}

void FireballCosmeticControllerBridge::ClearGenericStylesheetAfterLimit(
    const browser::DocumentId &document_id,
    const content::WeakDocumentPtr &document,
    std::uint64_t revision,
    std::string error_code) {
  auto tracked = documents_.find(document_id);
  FireballCosmeticDocumentHost *host = ResolveHost(document_id, document);
  auto ticket = state_.BeginGenericMutation(document_id, revision);
  if (tracked == documents_.end() || host == nullptr ||
      !ticket.has_value()) {
    ResetDocument(document_id, document);
    Publish(document_id, Error("COSMETIC_DOM_LIMIT_CLEANUP_FAILED"));
    return;
  }
  host->SetStylesheet(
      navigation::CosmeticStyleLayer::kGeneric, {},
      base::BindOnce(
          &FireballCosmeticControllerBridge::
              OnGenericStylesheetClearedAfterLimit,
          weak_factory_.GetWeakPtr(), *ticket, document,
          std::move(error_code)));
}

void FireballCosmeticControllerBridge::OnGenericStylesheetClearedAfterLimit(
    BrowserCosmeticControllerTicket ticket,
    content::WeakDocumentPtr document,
    std::string error_code,
    CosmeticTransportResult transport_result) {
  if (!state_.IsCurrent(ticket)) {
    return;
  }
  if (!ContextValid()) {
    ResetAllDocuments();
    return;
  }
  const bool accepted =
      transport_result.status == CosmeticTransportStatus::kApplied &&
      ResolveHost(ticket.document_id, document) != nullptr;
  if (!state_.CompleteGenericMutation(ticket, accepted)) {
    return;
  }
  auto tracked = documents_.find(ticket.document_id);
  if (!accepted || tracked == documents_.end()) {
    ResetDocument(ticket.document_id, document);
    Publish(ticket.document_id,
            Error(transport_result.error_code.empty()
                      ? "COSMETIC_DOM_LIMIT_CLEANUP_FAILED"
                      : transport_result.error_code));
    return;
  }
  tracked->second.last_dom_revision = ticket.dom_revision;
  tracked->second.last_result =
      Applied(tracked->second.plan,
              tracked->second.plan.hidden_selector_count);
  Publish(ticket.document_id, Error(error_code));
}

void FireballCosmeticControllerBridge::ResetDocument(
    const browser::DocumentId &document_id,
    const content::WeakDocumentPtr &document) {
  content::RenderFrameHost *frame = document.AsRenderFrameHostIfValid();
  if (frame != nullptr) {
    FireballCosmeticDocumentHost *host =
        FireballCosmeticDocumentHost::GetForCurrentDocument(frame);
    if (host != nullptr && host->document_id() == document_id) {
      host->ResetForController();
    }
  }
  state_.Reset(document_id);
  documents_.erase(document_id);
}

void FireballCosmeticControllerBridge::ResetAllDocuments() {
  std::optional<browser::DocumentId> lifecycle_document_id;
  content::WeakDocumentPtr lifecycle_document;
  if (lifecycle_owner_ && lifecycle_owner_->active_document_id().has_value()) {
    lifecycle_document_id = *lifecycle_owner_->active_document_id();
    lifecycle_document =
        lifecycle_owner_->GetActiveDocument(*lifecycle_document_id);
  }
  std::map<browser::DocumentId, TrackedDocument> tracked_documents =
      std::move(documents_);
  documents_.clear();
  state_.ResetAll();
  weak_factory_.InvalidateWeakPtrs();

  for (const auto &[document_id, tracked] : tracked_documents) {
    content::RenderFrameHost *frame =
        tracked.document.AsRenderFrameHostIfValid();
    if (frame == nullptr) {
      continue;
    }
    FireballCosmeticDocumentHost *host =
        FireballCosmeticDocumentHost::GetForCurrentDocument(frame);
    if (host != nullptr && host->document_id() == document_id) {
      host->ResetForController();
    }
  }
  if (lifecycle_document_id.has_value() &&
      tracked_documents.find(*lifecycle_document_id) ==
          tracked_documents.end()) {
    content::RenderFrameHost *frame =
        lifecycle_document.AsRenderFrameHostIfValid();
    if (frame != nullptr) {
      FireballCosmeticDocumentHost *host =
          FireballCosmeticDocumentHost::GetForCurrentDocument(frame);
      if (host != nullptr && host->document_id() == *lifecycle_document_id) {
        host->ResetForController();
      }
    }
  }
}

FireballCosmeticDocumentHost *FireballCosmeticControllerBridge::ResolveHost(
    const browser::DocumentId &document_id,
    const content::WeakDocumentPtr &document) const {
  content::RenderFrameHost *frame = document.AsRenderFrameHostIfValid();
  if (frame == nullptr || !frame->IsInPrimaryMainFrame() ||
      !frame->IsActive()) {
    return nullptr;
  }
  FireballCosmeticDocumentHost *host =
      FireballCosmeticDocumentHost::GetForCurrentDocument(frame);
  return host != nullptr && host->document_id() == document_id ? host : nullptr;
}

void FireballCosmeticControllerBridge::OnDocumentStylesheetApplied(
    BrowserCosmeticControllerTicket ticket, content::WeakDocumentPtr document,
    navigation::DocumentCosmeticPlan plan,
    CosmeticTransportResult transport_result) {
  if (!state_.IsCurrent(ticket)) {
    return;
  }
  if (!ContextValid()) {
    ResetAllDocuments();
    Publish(ticket.document_id, Error("COSMETIC_DOCUMENT_CONTEXT_INVALID"));
    return;
  }
  const bool accepted =
      transport_result.status == CosmeticTransportStatus::kApplied &&
      ResolveHost(ticket.document_id, document) != nullptr;
  if (!state_.CompleteActivation(ticket, accepted)) {
    return;
  }
  if (!accepted) {
    Publish(ticket.document_id, Error(transport_result.error_code.empty()
                                          ? "COSMETIC_STYLE_INSTALL_FAILED"
                                          : transport_result.error_code));
    return;
  }
  navigation::CosmeticControllerResult result =
      Applied(plan, plan.hidden_selector_count);
  plan.stylesheet.clear();
  plan.stylesheet.shrink_to_fit();
  auto [tracked, inserted] =
      documents_.emplace(ticket.document_id,
                         TrackedDocument{document, std::move(plan), result, 0});
  if (!inserted) {
    state_.Fail(ticket.document_id);
    Publish(ticket.document_id, Error("COSMETIC_DOCUMENT_CONTEXT_INVALID"));
    return;
  }
  if (tracked->second.plan.generic_scan_allowed) {
    ScheduleDomCollection(tracked->first);
  }
  Publish(tracked->first, tracked->second.last_result);
}

void FireballCosmeticControllerBridge::OnGenericStylesheetApplied(
    BrowserCosmeticControllerTicket ticket, content::WeakDocumentPtr document,
    navigation::CosmeticControllerResult result,
    CosmeticTransportResult transport_result) {
  if (!state_.IsCurrent(ticket)) {
    return;
  }
  if (!ContextValid()) {
    ResetAllDocuments();
    Publish(ticket.document_id, Error("COSMETIC_DOCUMENT_CONTEXT_INVALID"));
    return;
  }
  const bool accepted =
      transport_result.status == CosmeticTransportStatus::kApplied &&
      ResolveHost(ticket.document_id, document) != nullptr;
  if (!state_.CompleteGenericMutation(ticket, accepted)) {
    return;
  }
  auto tracked = documents_.find(ticket.document_id);
  if (!accepted || tracked == documents_.end()) {
    if (accepted) {
      state_.Fail(ticket.document_id);
    }
    Publish(ticket.document_id, Error(transport_result.error_code.empty()
                                          ? "COSMETIC_STYLE_INSTALL_FAILED"
                                          : transport_result.error_code));
    return;
  }
  tracked->second.last_dom_revision = ticket.dom_revision;
  tracked->second.last_result = std::move(result);
  Publish(ticket.document_id, tracked->second.last_result);
}

void FireballCosmeticControllerBridge::OnDocumentRevoked(
    BrowserCosmeticControllerTicket ticket, content::WeakDocumentPtr document,
    CosmeticTransportResult transport_result) {
  if (!state_.IsCurrent(ticket)) {
    return;
  }
  if (!ContextValid()) {
    ResetAllDocuments();
    Publish(ticket.document_id, Error("COSMETIC_DOCUMENT_CONTEXT_INVALID"));
    return;
  }
  const bool accepted =
      transport_result.status == CosmeticTransportStatus::kRevoked &&
      ResolveHost(ticket.document_id, document) != nullptr;
  if (!state_.CompleteRevocation(ticket, accepted)) {
    return;
  }
  if (!accepted) {
    Publish(ticket.document_id, Error(transport_result.error_code.empty()
                                          ? "COSMETIC_STYLE_REVOKE_FAILED"
                                          : transport_result.error_code));
    return;
  }
  documents_.erase(ticket.document_id);
  Publish(ticket.document_id, Disabled());
}

void FireballCosmeticControllerBridge::OnDocumentRevokedForRefresh(
    BrowserCosmeticControllerTicket ticket, content::WeakDocumentPtr document,
    CosmeticTransportResult transport_result) {
  if (!state_.IsCurrent(ticket)) {
    return;
  }
  if (!ContextValid()) {
    ResetAllDocuments();
    Publish(ticket.document_id, Error("COSMETIC_DOCUMENT_CONTEXT_INVALID"));
    return;
  }
  const bool accepted =
      transport_result.status == CosmeticTransportStatus::kRevoked &&
      ResolveHost(ticket.document_id, document) != nullptr;
  if (!state_.CompleteRevocation(ticket, accepted)) {
    return;
  }
  if (!accepted) {
    Publish(ticket.document_id, Error(transport_result.error_code.empty()
                                          ? "COSMETIC_STYLE_REVOKE_FAILED"
                                          : transport_result.error_code));
    return;
  }
  documents_.erase(ticket.document_id);
  FireballCosmeticDocumentHost *host =
      ResolveHost(ticket.document_id, document);
  if (host == nullptr) {
    Publish(ticket.document_id, Error("COSMETIC_DOCUMENT_INACTIVE"));
    return;
  }
  auto activation = state_.BeginActivation(ticket.document_id, 0);
  if (!activation.has_value()) {
    Publish(ticket.document_id, Error("COSMETIC_CONTROLLER_BUSY"));
    return;
  }
  host->Activate(base::BindOnce(
      &FireballCosmeticControllerBridge::OnHostReactivatedForRefresh,
      weak_factory_.GetWeakPtr(), *activation, document));
}

void FireballCosmeticControllerBridge::OnHostReactivatedForRefresh(
    BrowserCosmeticControllerTicket ticket, content::WeakDocumentPtr document,
    CosmeticTransportResult transport_result) {
  if (!state_.IsCurrent(ticket)) {
    return;
  }
  if (!ContextValid()) {
    ResetAllDocuments();
    Publish(ticket.document_id, Error("COSMETIC_DOCUMENT_CONTEXT_INVALID"));
    return;
  }
  FireballCosmeticDocumentHost *host =
      ResolveHost(ticket.document_id, document);
  if (transport_result.status != CosmeticTransportStatus::kBound ||
      host == nullptr) {
    if (state_.CompleteActivation(ticket, false)) {
      Publish(ticket.document_id, Error(transport_result.error_code.empty()
                                            ? "COSMETIC_DOCUMENT_REFRESH_FAILED"
                                            : transport_result.error_code));
    }
    return;
  }
  PrepareReadyDocument(ticket, *host);
}

void FireballCosmeticControllerBridge::Publish(
    const browser::DocumentId &document_id,
    const navigation::CosmeticControllerResult &result) {
  client_->OnCosmeticControllerResult(document_id, result);
}

} // namespace fireball::chromium
