#include "fireball/chromium/browser_cosmetic_controller_state.h"

#include <limits>
#include <utility>

namespace fireball::chromium {

std::optional<BrowserCosmeticControllerTicket>
BrowserCosmeticControllerState::BeginActivation(
    browser::DocumentId document_id, std::uint64_t restored_dom_revision) {
  if (phase_ == BrowserCosmeticControllerPhase::kActivating ||
      phase_ == BrowserCosmeticControllerPhase::kCollectingDom ||
      phase_ == BrowserCosmeticControllerPhase::kApplyingGeneric ||
      phase_ == BrowserCosmeticControllerPhase::kRevoking || ready()) {
    return std::nullopt;
  }
  auto generation = AdvanceGeneration();
  if (!generation.has_value()) {
    return std::nullopt;
  }
  active_document_id_ = std::move(document_id);
  last_dom_revision_ = restored_dom_revision;
  phase_ = BrowserCosmeticControllerPhase::kActivating;
  return BrowserCosmeticControllerTicket{*generation, *active_document_id_,
                                         restored_dom_revision};
}

bool BrowserCosmeticControllerState::CompleteActivation(
    const BrowserCosmeticControllerTicket &ticket, bool accepted) {
  if (!IsCurrent(ticket) ||
      phase_ != BrowserCosmeticControllerPhase::kActivating) {
    return false;
  }
  phase_ = accepted ? BrowserCosmeticControllerPhase::kReady
                    : BrowserCosmeticControllerPhase::kFailed;
  return true;
}

std::optional<BrowserCosmeticControllerTicket>
BrowserCosmeticControllerState::BeginDomCollection(
    const browser::DocumentId &document_id) {
  if (!ready() || !active_document_id_.has_value() ||
      *active_document_id_ != document_id) {
    return std::nullopt;
  }
  auto generation = AdvanceGeneration();
  if (!generation.has_value()) {
    return std::nullopt;
  }
  phase_ = BrowserCosmeticControllerPhase::kCollectingDom;
  return BrowserCosmeticControllerTicket{*generation, document_id,
                                         last_dom_revision_};
}

bool BrowserCosmeticControllerState::CompleteDomCollection(
    const BrowserCosmeticControllerTicket &ticket) {
  if (!IsCurrent(ticket) ||
      phase_ != BrowserCosmeticControllerPhase::kCollectingDom) {
    return false;
  }
  phase_ = BrowserCosmeticControllerPhase::kReady;
  return true;
}

bool BrowserCosmeticControllerState::CancelDomCollection(
    const browser::DocumentId &document_id) {
  if (!collecting_dom() || !active_document_id_.has_value() ||
      *active_document_id_ != document_id ||
      !AdvanceGeneration().has_value()) {
    return false;
  }
  phase_ = BrowserCosmeticControllerPhase::kReady;
  return true;
}

std::optional<BrowserCosmeticControllerTicket>
BrowserCosmeticControllerState::BeginGenericMutation(
    const browser::DocumentId &document_id, std::uint64_t dom_revision) {
  if (!ready() || !active_document_id_.has_value() ||
      *active_document_id_ != document_id || dom_revision == 0 ||
      dom_revision <= last_dom_revision_) {
    return std::nullopt;
  }
  auto generation = AdvanceGeneration();
  if (!generation.has_value()) {
    return std::nullopt;
  }
  phase_ = BrowserCosmeticControllerPhase::kApplyingGeneric;
  return BrowserCosmeticControllerTicket{*generation, document_id,
                                         dom_revision};
}

bool BrowserCosmeticControllerState::CompleteGenericMutation(
    const BrowserCosmeticControllerTicket &ticket, bool accepted) {
  if (!IsCurrent(ticket) ||
      phase_ != BrowserCosmeticControllerPhase::kApplyingGeneric) {
    return false;
  }
  if (accepted) {
    last_dom_revision_ = ticket.dom_revision;
    phase_ = BrowserCosmeticControllerPhase::kReady;
  } else {
    phase_ = BrowserCosmeticControllerPhase::kFailed;
  }
  return true;
}

std::optional<BrowserCosmeticControllerTicket>
BrowserCosmeticControllerState::BeginRevocation(
    const browser::DocumentId &document_id) {
  if (!ready() || !active_document_id_.has_value() ||
      *active_document_id_ != document_id) {
    return std::nullopt;
  }
  auto generation = AdvanceGeneration();
  if (!generation.has_value()) {
    return std::nullopt;
  }
  phase_ = BrowserCosmeticControllerPhase::kRevoking;
  return BrowserCosmeticControllerTicket{*generation, document_id,
                                         last_dom_revision_};
}

bool BrowserCosmeticControllerState::CompleteRevocation(
    const BrowserCosmeticControllerTicket &ticket, bool accepted) {
  if (!IsCurrent(ticket) ||
      phase_ != BrowserCosmeticControllerPhase::kRevoking) {
    return false;
  }
  if (accepted) {
    active_document_id_.reset();
    last_dom_revision_ = 0;
    phase_ = BrowserCosmeticControllerPhase::kIdle;
  } else {
    phase_ = BrowserCosmeticControllerPhase::kFailed;
  }
  return true;
}

bool BrowserCosmeticControllerState::Suspend(
    const browser::DocumentId &document_id) {
  if (!active_document_id_.has_value() || *active_document_id_ != document_id) {
    return false;
  }
  if (!AdvanceGeneration().has_value()) {
    return false;
  }
  phase_ = BrowserCosmeticControllerPhase::kSuspended;
  return true;
}

bool BrowserCosmeticControllerState::Fail(
    const browser::DocumentId &document_id) {
  if (!active_document_id_.has_value() || *active_document_id_ != document_id) {
    return false;
  }
  AdvanceGeneration();
  phase_ = BrowserCosmeticControllerPhase::kFailed;
  return true;
}

bool BrowserCosmeticControllerState::Reset(
    const browser::DocumentId &document_id) {
  if (!active_document_id_.has_value() || *active_document_id_ != document_id ||
      !AdvanceGeneration().has_value()) {
    return false;
  }
  active_document_id_.reset();
  last_dom_revision_ = 0;
  phase_ = BrowserCosmeticControllerPhase::kIdle;
  return true;
}

bool BrowserCosmeticControllerState::Dispose(
    const browser::DocumentId &document_id) {
  if (!active_document_id_.has_value() || *active_document_id_ != document_id) {
    return false;
  }
  return Reset(document_id);
}

void BrowserCosmeticControllerState::ResetAll() {
  AdvanceGeneration();
  active_document_id_.reset();
  last_dom_revision_ = 0;
  if (!generation_exhausted_) {
    phase_ = BrowserCosmeticControllerPhase::kIdle;
  }
}

bool BrowserCosmeticControllerState::IsCurrent(
    const BrowserCosmeticControllerTicket &ticket) const {
  return ticket.generation != 0 && ticket.generation == generation_ &&
         !generation_exhausted_ && active_document_id_.has_value() &&
         *active_document_id_ == ticket.document_id;
}

std::optional<std::uint64_t>
BrowserCosmeticControllerState::AdvanceGeneration() {
  if (generation_exhausted_ ||
      generation_ == std::numeric_limits<std::uint64_t>::max()) {
    generation_exhausted_ = true;
    generation_ = 0;
    phase_ = BrowserCosmeticControllerPhase::kFailed;
    return std::nullopt;
  }
  return ++generation_;
}

} // namespace fireball::chromium
