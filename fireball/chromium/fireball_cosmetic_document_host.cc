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

}  // namespace

DOCUMENT_USER_DATA_KEY_IMPL(FireballCosmeticDocumentHost);

FireballCosmeticDocumentHost::FireballCosmeticDocumentHost(
    content::RenderFrameHost* render_frame_host,
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

void FireballCosmeticDocumentHost::SetStylesheet(
    navigation::CosmeticStyleLayer layer,
    std::string stylesheet,
    CompletionCallback callback) {
  if (!ready() || !IsActivePrimaryDocument()) {
    std::move(callback).Run(Error("COSMETIC_DOCUMENT_NOT_READY"));
    return;
  }
  transport_->SetStylesheet(
      layer, std::move(stylesheet),
      base::BindOnce(&FireballCosmeticDocumentHost::OnStylesheetApplied,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
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

void FireballCosmeticDocumentHost::OnActivated(
    BrowserCosmeticDocumentTicket ticket,
    CompletionCallback callback,
    CosmeticTransportResult result) {
  const bool accepted = result.status == CosmeticTransportStatus::kBound &&
                        IsActivePrimaryDocument();
  if (!state_.CompleteActivation(ticket, accepted)) {
    std::move(callback).Run(Error("COSMETIC_DOCUMENT_ACTIVATION_STALE"));
    return;
  }
  if (!accepted && result.status != CosmeticTransportStatus::kError) {
    result = Error("COSMETIC_DOCUMENT_INACTIVE");
  }
  std::move(callback).Run(std::move(result));
}

void FireballCosmeticDocumentHost::OnStylesheetApplied(
    CompletionCallback callback,
    CosmeticTransportResult result) {
  if (result.status != CosmeticTransportStatus::kApplied &&
      state_.phase() != BrowserCosmeticDocumentPhase::kSuspended) {
    state_.Fail();
  }
  std::move(callback).Run(std::move(result));
}

void FireballCosmeticDocumentHost::OnRevoked(
    BrowserCosmeticDocumentTicket ticket,
    CompletionCallback callback,
    CosmeticTransportResult result) {
  const bool accepted = result.status == CosmeticTransportStatus::kRevoked;
  if (!state_.CompleteRevocation(ticket, accepted)) {
    std::move(callback).Run(Error("COSMETIC_DOCUMENT_REVOCATION_STALE"));
    return;
  }
  std::move(callback).Run(std::move(result));
}

bool FireballCosmeticDocumentHost::IsActivePrimaryDocument() const {
  return render_frame_host().IsInPrimaryMainFrame() &&
         render_frame_host().IsActive();
}

}  // namespace fireball::chromium
