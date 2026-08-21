#include "fireball/chromium/browser_cosmetic_transport_state.h"

#include <limits>

namespace fireball::chromium {

std::optional<BrowserCosmeticTransportTicket>
BrowserCosmeticTransportState::BeginBinding() {
  if (phase_ != BrowserCosmeticTransportPhase::kUnbound) {
    return std::nullopt;
  }
  auto ticket = AdvanceGeneration();
  if (!ticket.has_value()) {
    return std::nullopt;
  }
  phase_ = BrowserCosmeticTransportPhase::kReadingDocumentEpoch;
  return ticket;
}

bool BrowserCosmeticTransportState::AcceptDocumentEpoch(
    const BrowserCosmeticTransportTicket& ticket,
    std::uint64_t document_epoch) {
  if (!IsCurrent(ticket) ||
      phase_ != BrowserCosmeticTransportPhase::kReadingDocumentEpoch ||
      document_epoch == 0) {
    if (IsCurrent(ticket)) {
      phase_ = BrowserCosmeticTransportPhase::kFailed;
      document_epoch_ = 0;
      binding_generation_ = 0;
    }
    return false;
  }
  document_epoch_ = document_epoch;
  phase_ = BrowserCosmeticTransportPhase::kBindingDocument;
  return true;
}

bool BrowserCosmeticTransportState::CompleteBinding(
    const BrowserCosmeticTransportTicket& ticket,
    bool accepted,
    std::uint64_t binding_generation) {
  if (!IsCurrent(ticket) ||
      phase_ != BrowserCosmeticTransportPhase::kBindingDocument) {
    return false;
  }
  const bool valid = accepted && binding_generation != 0;
  phase_ = valid ? BrowserCosmeticTransportPhase::kReady
                 : BrowserCosmeticTransportPhase::kFailed;
  if (!valid) {
    document_epoch_ = 0;
    binding_generation_ = 0;
  } else {
    binding_generation_ = binding_generation;
  }
  return true;
}

std::optional<BrowserCosmeticTransportTicket>
BrowserCosmeticTransportState::BeginDomCollection() {
  if (!ready()) {
    return std::nullopt;
  }
  auto ticket = AdvanceGeneration();
  if (!ticket.has_value()) {
    return std::nullopt;
  }
  phase_ = BrowserCosmeticTransportPhase::kCollectingDom;
  return ticket;
}

bool BrowserCosmeticTransportState::CompleteDomCollection(
    const BrowserCosmeticTransportTicket& ticket,
    bool transport_valid) {
  if (!IsCurrent(ticket) ||
      phase_ != BrowserCosmeticTransportPhase::kCollectingDom) {
    return false;
  }
  phase_ = transport_valid ? BrowserCosmeticTransportPhase::kReady
                           : BrowserCosmeticTransportPhase::kFailed;
  if (!transport_valid) {
    document_epoch_ = 0;
    binding_generation_ = 0;
  }
  return true;
}

bool BrowserCosmeticTransportState::CancelDomCollection() {
  if (phase_ != BrowserCosmeticTransportPhase::kCollectingDom ||
      !AdvanceGeneration().has_value()) {
    return false;
  }
  phase_ = BrowserCosmeticTransportPhase::kReady;
  return true;
}

std::optional<BrowserCosmeticTransportTicket>
BrowserCosmeticTransportState::BeginMutation(bool revoke_document) {
  if (!ready()) {
    return std::nullopt;
  }
  auto ticket = AdvanceGeneration();
  if (!ticket.has_value()) {
    return std::nullopt;
  }
  phase_ = revoke_document ? BrowserCosmeticTransportPhase::kRevokingDocument
                           : BrowserCosmeticTransportPhase::kApplyingStylesheet;
  return ticket;
}

bool BrowserCosmeticTransportState::CompleteMutation(
    const BrowserCosmeticTransportTicket& ticket,
    bool accepted) {
  if (!IsCurrent(ticket) ||
      (phase_ != BrowserCosmeticTransportPhase::kApplyingStylesheet &&
       phase_ != BrowserCosmeticTransportPhase::kRevokingDocument)) {
    return false;
  }
  const bool revoking =
      phase_ == BrowserCosmeticTransportPhase::kRevokingDocument;
  if (!accepted) {
    phase_ = BrowserCosmeticTransportPhase::kFailed;
    document_epoch_ = 0;
    binding_generation_ = 0;
    return true;
  }
  phase_ = revoking ? BrowserCosmeticTransportPhase::kRevoked
                    : BrowserCosmeticTransportPhase::kReady;
  if (revoking) {
    document_epoch_ = 0;
    binding_generation_ = 0;
  }
  return true;
}

void BrowserCosmeticTransportState::Invalidate() {
  AdvanceGeneration();
  phase_ = BrowserCosmeticTransportPhase::kFailed;
  document_epoch_ = 0;
  binding_generation_ = 0;
}

bool BrowserCosmeticTransportState::IsCurrent(
    const BrowserCosmeticTransportTicket& ticket) const {
  return ticket.generation != 0 && ticket.generation == generation_ &&
         !generation_exhausted_;
}

std::optional<BrowserCosmeticTransportTicket>
BrowserCosmeticTransportState::AdvanceGeneration() {
  if (generation_exhausted_ ||
      generation_ == std::numeric_limits<std::uint64_t>::max()) {
    generation_exhausted_ = true;
    generation_ = 0;
    phase_ = BrowserCosmeticTransportPhase::kFailed;
    document_epoch_ = 0;
    binding_generation_ = 0;
    return std::nullopt;
  }
  ++generation_;
  return BrowserCosmeticTransportTicket{generation_};
}

}  // namespace fireball::chromium
