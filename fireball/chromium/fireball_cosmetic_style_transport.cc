#include "fireball/chromium/fireball_cosmetic_style_transport.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "content/public/browser/render_frame_host.h"
#include "fireball/components/navigation/document_cosmetic_policy.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace fireball::chromium {
namespace {

CosmeticTransportResult Error(std::string error_code) {
  return {CosmeticTransportStatus::kError, std::move(error_code)};
}

}  // namespace

FireballCosmeticStyleTransport::FireballCosmeticStyleTransport(
    content::RenderFrameHost& render_frame_host,
    browser::DocumentId document_id)
    : document_(render_frame_host.GetWeakDocumentPtr()),
      document_id_(std::move(document_id)) {}

FireballCosmeticStyleTransport::~FireballCosmeticStyleTransport() {
  weak_factory_.InvalidateWeakPtrs();
  remote_.reset();
  state_.Invalidate();
}

void FireballCosmeticStyleTransport::BindDocument(CompletionCallback callback) {
  auto ticket = state_.BeginBinding();
  if (!ticket.has_value()) {
    std::move(callback).Run(Error("COSMETIC_TRANSPORT_BUSY"));
    return;
  }
  content::RenderFrameHost* frame = ActivePrimaryDocument();
  if (frame == nullptr) {
    state_.Invalidate();
    std::move(callback).Run(Error("COSMETIC_TRANSPORT_DOCUMENT_INACTIVE"));
    return;
  }
  blink::AssociatedInterfaceProvider* interfaces =
      frame->GetRemoteAssociatedInterfaces();
  if (interfaces == nullptr) {
    state_.Invalidate();
    std::move(callback).Run(Error("COSMETIC_TRANSPORT_INTERFACE_UNAVAILABLE"));
    return;
  }

  pending_callback_ = std::move(callback);
  interfaces->GetInterface(&remote_);
  remote_.set_disconnect_handler(
      base::BindOnce(&FireballCosmeticStyleTransport::OnDisconnected,
                     weak_factory_.GetWeakPtr()));
  remote_->GetDocumentEpoch(
      base::BindOnce(&FireballCosmeticStyleTransport::OnDocumentEpoch,
                     weak_factory_.GetWeakPtr(), *ticket));
}

void FireballCosmeticStyleTransport::SetStylesheet(
    navigation::CosmeticStyleLayer layer,
    std::string stylesheet,
    CompletionCallback callback) {
  if (!navigation::IsValidCompiledCosmeticStylesheet(stylesheet)) {
    std::move(callback).Run(Error("COSMETIC_TRANSPORT_STYLESHEET_INVALID"));
    return;
  }
  if (pending_callback_) {
    std::move(callback).Run(Error("COSMETIC_TRANSPORT_NOT_READY"));
    return;
  }
  auto ticket = state_.BeginMutation(/*revoke_document=*/false);
  if (!ticket.has_value()) {
    std::move(callback).Run(Error("COSMETIC_TRANSPORT_NOT_READY"));
    return;
  }
  if (ActivePrimaryDocument() == nullptr || !remote_.is_bound() ||
      !remote_.is_connected()) {
    state_.Invalidate();
    remote_.reset();
    std::move(callback).Run(Error("COSMETIC_TRANSPORT_DOCUMENT_INACTIVE"));
    return;
  }

  pending_callback_ = std::move(callback);
  remote_->SetStylesheet(
      document_id_.value(), state_.document_epoch(),
      state_.binding_generation(), ConvertLayer(layer), stylesheet,
      base::BindOnce(&FireballCosmeticStyleTransport::OnMutationCompleted,
                     weak_factory_.GetWeakPtr(), *ticket));
}

void FireballCosmeticStyleTransport::RemoveDocumentStyles(
    CompletionCallback callback) {
  if (pending_callback_) {
    std::move(callback).Run(Error("COSMETIC_TRANSPORT_NOT_READY"));
    return;
  }
  auto ticket = state_.BeginMutation(/*revoke_document=*/true);
  if (!ticket.has_value()) {
    std::move(callback).Run(Error("COSMETIC_TRANSPORT_NOT_READY"));
    return;
  }
  if (ActivePrimaryDocument() == nullptr || !remote_.is_bound() ||
      !remote_.is_connected()) {
    state_.Invalidate();
    remote_.reset();
    std::move(callback).Run(Error("COSMETIC_TRANSPORT_DOCUMENT_INACTIVE"));
    return;
  }

  pending_callback_ = std::move(callback);
  remote_->RemoveDocumentStyles(
      document_id_.value(), state_.document_epoch(),
      state_.binding_generation(),
      base::BindOnce(&FireballCosmeticStyleTransport::OnMutationCompleted,
                     weak_factory_.GetWeakPtr(), *ticket));
}

void FireballCosmeticStyleTransport::Invalidate() {
  weak_factory_.InvalidateWeakPtrs();
  remote_.reset();
  state_.Invalidate();
  if (pending_callback_) {
    Finish(CosmeticTransportStatus::kError, "COSMETIC_TRANSPORT_INVALIDATED");
  }
}

content::RenderFrameHost*
FireballCosmeticStyleTransport::ActivePrimaryDocument() const {
  content::RenderFrameHost* frame = document_.AsRenderFrameHostIfValid();
  return frame != nullptr && frame->IsInPrimaryMainFrame() && frame->IsActive()
             ? frame
             : nullptr;
}

void FireballCosmeticStyleTransport::OnDocumentEpoch(
    BrowserCosmeticTransportTicket ticket,
    std::uint64_t document_epoch) {
  if (!state_.IsCurrent(ticket)) {
    return;
  }
  if (ActivePrimaryDocument() == nullptr) {
    FailCurrent(ticket, "COSMETIC_TRANSPORT_DOCUMENT_INACTIVE");
    return;
  }
  if (!state_.AcceptDocumentEpoch(ticket, document_epoch)) {
    FailCurrent(ticket, "COSMETIC_TRANSPORT_EPOCH_REJECTED");
    return;
  }
  remote_->BindDocument(
      document_id_.value(), document_epoch,
      base::BindOnce(&FireballCosmeticStyleTransport::OnDocumentBound,
                     weak_factory_.GetWeakPtr(), ticket));
}

void FireballCosmeticStyleTransport::OnDocumentBound(
    BrowserCosmeticTransportTicket ticket,
    bool bound,
    std::uint64_t binding_generation) {
  if (!state_.IsCurrent(ticket)) {
    return;
  }
  if (ActivePrimaryDocument() == nullptr) {
    FailCurrent(ticket, "COSMETIC_TRANSPORT_DOCUMENT_INACTIVE");
    return;
  }
  if (!state_.CompleteBinding(ticket, bound, binding_generation) || !bound ||
      binding_generation == 0) {
    FailCurrent(ticket, "COSMETIC_TRANSPORT_BIND_REJECTED");
    return;
  }
  Finish(CosmeticTransportStatus::kBound);
}

void FireballCosmeticStyleTransport::OnMutationCompleted(
    BrowserCosmeticTransportTicket ticket,
    bool accepted) {
  if (!state_.IsCurrent(ticket)) {
    return;
  }
  if (ActivePrimaryDocument() == nullptr) {
    FailCurrent(ticket, "COSMETIC_TRANSPORT_DOCUMENT_INACTIVE");
    return;
  }
  const bool revoking =
      state_.phase() == BrowserCosmeticTransportPhase::kRevokingDocument;
  if (!state_.CompleteMutation(ticket, accepted) || !accepted) {
    FailCurrent(ticket, "COSMETIC_TRANSPORT_MUTATION_REJECTED");
    return;
  }
  if (revoking) {
    remote_.reset();
    Finish(CosmeticTransportStatus::kRevoked);
    return;
  }
  Finish(CosmeticTransportStatus::kApplied);
}

void FireballCosmeticStyleTransport::OnDisconnected() {
  remote_.reset();
  state_.Invalidate();
  if (pending_callback_) {
    Finish(CosmeticTransportStatus::kError, "COSMETIC_TRANSPORT_DISCONNECTED");
  }
}

void FireballCosmeticStyleTransport::FailCurrent(
    const BrowserCosmeticTransportTicket& ticket,
    std::string error_code) {
  if (!state_.IsCurrent(ticket)) {
    return;
  }
  state_.Invalidate();
  remote_.reset();
  Finish(CosmeticTransportStatus::kError, std::move(error_code));
}

void FireballCosmeticStyleTransport::Finish(CosmeticTransportStatus status,
                                            std::string error_code) {
  CompletionCallback callback = std::move(pending_callback_);
  if (callback) {
    std::move(callback).Run({status, std::move(error_code)});
  }
}

// static
fireball::mojom::CosmeticStyleLayer
FireballCosmeticStyleTransport::ConvertLayer(
    navigation::CosmeticStyleLayer layer) {
  return layer == navigation::CosmeticStyleLayer::kDocument
             ? fireball::mojom::CosmeticStyleLayer::kDocument
             : fireball::mojom::CosmeticStyleLayer::kGeneric;
}

}  // namespace fireball::chromium
