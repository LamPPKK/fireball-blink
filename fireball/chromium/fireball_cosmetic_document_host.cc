#include "fireball/chromium/fireball_cosmetic_document_host.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "content/public/browser/render_frame_host.h"

namespace fireball::chromium {
namespace {

CosmeticTransportResult Error(std::string error_code) {
  return {CosmeticTransportStatus::kError, std::move(error_code)};
}

} // namespace

DOCUMENT_USER_DATA_KEY_IMPL(FireballCosmeticDocumentHost);

FireballCosmeticDocumentHost::FireballCosmeticDocumentHost(
    content::RenderFrameHost *render_frame_host,
    browser::DocumentId document_id)
    : content::DocumentUserData<FireballCosmeticDocumentHost>(
          render_frame_host),
      document_id_(std::move(document_id)) {}

FireballCosmeticDocumentHost::~FireballCosmeticDocumentHost() = default;

void FireballCosmeticDocumentHost::Activate(CompletionCallback callback) {
  if (!IsActivePrimaryDocument()) {
    std::move(callback).Run(Error("COSMETIC_DOCUMENT_INACTIVE"));
    return;
  }
  if (ready()) {
    std::move(callback).Run({CosmeticTransportStatus::kBound, {}});
    return;
  }
  if (state_.phase() == BrowserCosmeticDocumentPhase::kReady) {
    if (transport_ &&
        transport_->phase() != BrowserCosmeticTransportPhase::kFailed &&
        transport_->phase() != BrowserCosmeticTransportPhase::kRevoked) {
      std::move(callback).Run(Error("COSMETIC_DOCUMENT_BUSY"));
      return;
    }
    state_.Fail();
    transport_.reset();
  }
  auto ticket = state_.BeginActivation();
  if (!ticket.has_value()) {
    std::move(callback).Run(Error("COSMETIC_DOCUMENT_BUSY"));
    return;
  }

  transport_ = std::make_unique<FireballCosmeticStyleTransport>(
      render_frame_host(), document_id_);
  transport_->BindDocument(
      base::BindOnce(&FireballCosmeticDocumentHost::OnActivated,
                     weak_factory_.GetWeakPtr(), *ticket, std::move(callback)));
}

void FireballCosmeticDocumentHost::Suspend() {
  state_.Suspend();
  std::unique_ptr<FireballCosmeticStyleTransport> transport =
      std::move(transport_);
  if (transport) {
    transport->Invalidate();
  }
}

void FireballCosmeticDocumentHost::ResetForController() {
  Suspend();
  desired_document_stylesheet_.clear();
  desired_generic_stylesheet_.clear();
}

void FireballCosmeticDocumentHost::SetStylesheet(
    navigation::CosmeticStyleLayer layer, std::string stylesheet,
    CompletionCallback callback) {
  if (!ready() || !IsActivePrimaryDocument()) {
    std::move(callback).Run(Error("COSMETIC_DOCUMENT_NOT_READY"));
    return;
  }
  std::string desired_stylesheet = stylesheet;
  transport_->SetStylesheet(
      layer, std::move(stylesheet),
      base::BindOnce(&FireballCosmeticDocumentHost::OnStylesheetApplied,
                     weak_factory_.GetWeakPtr(), layer,
                     std::move(desired_stylesheet), std::move(callback)));
}

void FireballCosmeticDocumentHost::Revoke(CompletionCallback callback) {
  if (!ready() || !IsActivePrimaryDocument()) {
    std::move(callback).Run(Error("COSMETIC_DOCUMENT_NOT_READY"));
    return;
  }
  auto ticket = state_.BeginRevocation();
  if (!ticket.has_value()) {
    std::move(callback).Run(Error("COSMETIC_DOCUMENT_BUSY"));
    return;
  }
  transport_->RemoveDocumentStyles(
      base::BindOnce(&FireballCosmeticDocumentHost::OnRevoked,
                     weak_factory_.GetWeakPtr(), *ticket, std::move(callback)));
}

bool FireballCosmeticDocumentHost::ready() const {
  return state_.ready() && transport_ && transport_->ready();
}

content::WeakDocumentPtr FireballCosmeticDocumentHost::GetWeakDocumentPtr() {
  return render_frame_host().GetWeakDocumentPtr();
}

void FireballCosmeticDocumentHost::OnActivated(
    BrowserCosmeticDocumentTicket ticket, CompletionCallback callback,
    CosmeticTransportResult result) {
  const bool accepted = result.status == CosmeticTransportStatus::kBound &&
                        IsActivePrimaryDocument();
  if (!state_.CompleteBinding(ticket, accepted)) {
    std::move(callback).Run(Error("COSMETIC_DOCUMENT_ACTIVATION_STALE"));
    return;
  }
  if (!accepted) {
    if (result.status != CosmeticTransportStatus::kError) {
      result = Error("COSMETIC_DOCUMENT_INACTIVE");
    }
    std::move(callback).Run(std::move(result));
    return;
  }
  RestoreDesiredStyles(ticket, std::move(callback));
}

void FireballCosmeticDocumentHost::RestoreDesiredStyles(
    BrowserCosmeticDocumentTicket ticket, CompletionCallback callback) {
  if (!state_.IsCurrent(ticket) || transport_ == nullptr) {
    if (state_.IsCurrent(ticket)) {
      FinishRestore(ticket, std::move(callback), false,
                    "COSMETIC_DOCUMENT_RESTORE_FAILED");
    } else {
      std::move(callback).Run(Error("COSMETIC_DOCUMENT_RESTORE_STALE"));
    }
    return;
  }
  if (desired_document_stylesheet_.empty()) {
    RestoreDesiredGenericStyle(ticket, std::move(callback));
    return;
  }
  transport_->SetStylesheet(
      navigation::CosmeticStyleLayer::kDocument, desired_document_stylesheet_,
      base::BindOnce(
          &FireballCosmeticDocumentHost::OnDesiredDocumentStyleRestored,
          weak_factory_.GetWeakPtr(), ticket, std::move(callback)));
}

void FireballCosmeticDocumentHost::RestoreDesiredGenericStyle(
    BrowserCosmeticDocumentTicket ticket, CompletionCallback callback) {
  if (!state_.IsCurrent(ticket) || transport_ == nullptr) {
    if (state_.IsCurrent(ticket)) {
      FinishRestore(ticket, std::move(callback), false,
                    "COSMETIC_DOCUMENT_RESTORE_FAILED");
    } else {
      std::move(callback).Run(Error("COSMETIC_DOCUMENT_RESTORE_STALE"));
    }
    return;
  }
  if (desired_generic_stylesheet_.empty()) {
    FinishRestore(ticket, std::move(callback), true);
    return;
  }
  transport_->SetStylesheet(
      navigation::CosmeticStyleLayer::kGeneric, desired_generic_stylesheet_,
      base::BindOnce(
          &FireballCosmeticDocumentHost::OnDesiredGenericStyleRestored,
          weak_factory_.GetWeakPtr(), ticket, std::move(callback)));
}

void FireballCosmeticDocumentHost::OnDesiredDocumentStyleRestored(
    BrowserCosmeticDocumentTicket ticket, CompletionCallback callback,
    CosmeticTransportResult result) {
  if (result.status != CosmeticTransportStatus::kApplied) {
    FinishRestore(ticket, std::move(callback), false,
                  result.error_code.empty()
                      ? std::string_view("COSMETIC_DOCUMENT_RESTORE_FAILED")
                      : std::string_view(result.error_code));
    return;
  }
  RestoreDesiredGenericStyle(ticket, std::move(callback));
}

void FireballCosmeticDocumentHost::OnDesiredGenericStyleRestored(
    BrowserCosmeticDocumentTicket ticket, CompletionCallback callback,
    CosmeticTransportResult result) {
  const bool accepted = result.status == CosmeticTransportStatus::kApplied;
  FinishRestore(
      ticket, std::move(callback), accepted,
      accepted ? std::string_view()
               : (result.error_code.empty()
                      ? std::string_view("COSMETIC_DOCUMENT_RESTORE_FAILED")
                      : std::string_view(result.error_code)));
}

void FireballCosmeticDocumentHost::FinishRestore(
    BrowserCosmeticDocumentTicket ticket, CompletionCallback callback,
    bool accepted, std::string_view error_code) {
  accepted = accepted && IsActivePrimaryDocument();
  if (!state_.CompleteRestore(ticket, accepted)) {
    std::move(callback).Run(Error("COSMETIC_DOCUMENT_RESTORE_STALE"));
    return;
  }
  if (!accepted) {
    std::unique_ptr<FireballCosmeticStyleTransport> transport =
        std::move(transport_);
    if (transport) {
      transport->Invalidate();
    }
    std::move(callback).Run(Error(error_code.empty()
                                      ? "COSMETIC_DOCUMENT_INACTIVE"
                                      : std::string(error_code)));
    return;
  }
  std::move(callback).Run({CosmeticTransportStatus::kBound, {}});
}

void FireballCosmeticDocumentHost::OnStylesheetApplied(
    navigation::CosmeticStyleLayer layer, std::string stylesheet,
    CompletionCallback callback, CosmeticTransportResult result) {
  if (result.status == CosmeticTransportStatus::kApplied &&
      IsActivePrimaryDocument()) {
    std::string &desired = layer == navigation::CosmeticStyleLayer::kDocument
                               ? desired_document_stylesheet_
                               : desired_generic_stylesheet_;
    desired = std::move(stylesheet);
  } else if (result.status == CosmeticTransportStatus::kApplied) {
    result = Error("COSMETIC_DOCUMENT_INACTIVE");
    if (state_.phase() != BrowserCosmeticDocumentPhase::kSuspended) {
      state_.Fail();
    }
  } else if (state_.phase() != BrowserCosmeticDocumentPhase::kSuspended) {
    state_.Fail();
  }
  std::move(callback).Run(std::move(result));
}

void FireballCosmeticDocumentHost::OnRevoked(
    BrowserCosmeticDocumentTicket ticket, CompletionCallback callback,
    CosmeticTransportResult result) {
  const bool accepted = result.status == CosmeticTransportStatus::kRevoked;
  if (!state_.CompleteRevocation(ticket, accepted)) {
    std::move(callback).Run(Error("COSMETIC_DOCUMENT_REVOCATION_STALE"));
    return;
  }
  if (accepted) {
    desired_document_stylesheet_.clear();
    desired_generic_stylesheet_.clear();
  }
  std::move(callback).Run(std::move(result));
}

bool FireballCosmeticDocumentHost::IsActivePrimaryDocument() const {
  return render_frame_host().IsInPrimaryMainFrame() &&
         render_frame_host().IsActive();
}

} // namespace fireball::chromium
