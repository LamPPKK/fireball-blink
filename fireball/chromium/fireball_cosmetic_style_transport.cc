#include "fireball/chromium/fireball_cosmetic_style_transport.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "content/public/browser/render_frame_host.h"
#include "fireball/components/navigation/document_cosmetic_policy.h"
#include "mojo/public/cpp/bindings/message.h"
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

void FireballCosmeticStyleTransport::CollectDomSnapshot(
    DomSnapshotCallback callback) {
  if (pending_callback_ || pending_dom_snapshot_callback_) {
    std::move(callback).Run(Error("COSMETIC_TRANSPORT_NOT_READY"), {});
    return;
  }
  auto ticket = state_.BeginDomCollection();
  if (!ticket.has_value()) {
    std::move(callback).Run(Error("COSMETIC_TRANSPORT_NOT_READY"), {});
    return;
  }
  if (ActivePrimaryDocument() == nullptr || !remote_.is_bound() ||
      !remote_.is_connected()) {
    state_.Invalidate();
    remote_.reset();
    std::move(callback).Run(
        Error("COSMETIC_TRANSPORT_DOCUMENT_INACTIVE"), {});
    return;
  }

  pending_dom_snapshot_callback_ = std::move(callback);
  remote_->CollectDomSnapshot(
      document_id_.value(), state_.document_epoch(),
      state_.binding_generation(),
      base::BindOnce(
          &FireballCosmeticStyleTransport::OnDomSnapshotCollected,
          weak_factory_.GetWeakPtr(), *ticket));
}

bool FireballCosmeticStyleTransport::CancelDomSnapshot() {
  if (!pending_dom_snapshot_callback_) {
    return false;
  }
  const bool cancelled = state_.CancelDomCollection();
  pending_dom_snapshot_callback_.Reset();
  return cancelled;
}

void FireballCosmeticStyleTransport::SetStylesheet(
    navigation::CosmeticStyleLayer layer,
    std::string stylesheet,
    CompletionCallback callback) {
  if (!navigation::IsValidCompiledCosmeticStylesheet(stylesheet)) {
    std::move(callback).Run(Error("COSMETIC_TRANSPORT_STYLESHEET_INVALID"));
    return;
  }
  if (pending_callback_ || pending_dom_snapshot_callback_) {
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
  if (pending_callback_ || pending_dom_snapshot_callback_) {
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
  } else if (pending_dom_snapshot_callback_) {
    FinishDomSnapshot(CosmeticTransportStatus::kError,
                      "COSMETIC_TRANSPORT_INVALIDATED");
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

void FireballCosmeticStyleTransport::OnDomSnapshotCollected(
    BrowserCosmeticTransportTicket ticket,
    bool collected,
    bool limit_exceeded,
    std::uint64_t revision,
    std::uint32_t payload_size,
    std::uint32_t class_count,
    std::uint32_t id_count,
    std::vector<std::uint8_t> payload) {
  const bool empty_payload = payload_size == 0 && class_count == 0 &&
                             id_count == 0 &&
                             IsZeroedCosmeticDomWirePayload(payload);
  const bool rejected = !collected && !limit_exceeded && revision == 0 &&
                        empty_payload;
  const bool limited = !collected && limit_exceeded && revision != 0 &&
                       empty_payload;
  std::optional<CosmeticDomSnapshot> snapshot;
  if (collected && !limit_exceeded) {
    snapshot = DecodeCosmeticDomSnapshot(revision, payload_size, class_count,
                                         id_count, payload);
  }
  if (!rejected && !limited && !snapshot.has_value()) {
    mojo::ReportBadMessage("Malformed Fireball cosmetic DOM snapshot");
    if (state_.IsCurrent(ticket)) {
      FailCurrent(ticket, "COSMETIC_DOM_SNAPSHOT_REJECTED");
    }
    return;
  }
  if (!state_.IsCurrent(ticket)) {
    return;
  }
  if (ActivePrimaryDocument() == nullptr) {
    FailCurrent(ticket, "COSMETIC_TRANSPORT_DOCUMENT_INACTIVE");
    return;
  }
  if (rejected) {
    FailCurrent(ticket, "COSMETIC_DOM_SNAPSHOT_REJECTED");
    return;
  }
  if (limited) {
    if (!state_.CompleteDomCollection(ticket, /*transport_valid=*/true)) {
      return;
    }
    FinishDomSnapshot(CosmeticTransportStatus::kLimited,
                      "COSMETIC_DOM_LIMIT_EXCEEDED",
                      CosmeticDomSnapshot{revision, {}, {}});
    return;
  }
  if (!state_.CompleteDomCollection(ticket, /*transport_valid=*/true)) {
    return;
  }
  FinishDomSnapshot(CosmeticTransportStatus::kCollected, {},
                    std::move(*snapshot));
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
  } else if (pending_dom_snapshot_callback_) {
    FinishDomSnapshot(CosmeticTransportStatus::kError,
                      "COSMETIC_TRANSPORT_DISCONNECTED");
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
  if (pending_dom_snapshot_callback_) {
    FinishDomSnapshot(CosmeticTransportStatus::kError,
                      std::move(error_code));
  } else {
    Finish(CosmeticTransportStatus::kError, std::move(error_code));
  }
}

void FireballCosmeticStyleTransport::Finish(CosmeticTransportStatus status,
                                            std::string error_code) {
  CompletionCallback callback = std::move(pending_callback_);
  if (callback) {
    std::move(callback).Run({status, std::move(error_code)});
  }
}

void FireballCosmeticStyleTransport::FinishDomSnapshot(
    CosmeticTransportStatus status,
    std::string error_code,
    CosmeticDomSnapshot snapshot) {
  DomSnapshotCallback callback = std::move(pending_dom_snapshot_callback_);
  if (callback) {
    std::move(callback).Run({status, std::move(error_code)},
                            std::move(snapshot));
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
