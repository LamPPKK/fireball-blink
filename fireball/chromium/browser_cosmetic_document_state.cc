#include "fireball/chromium/browser_cosmetic_document_state.h"

#include <limits>

namespace fireball::chromium {

std::optional<BrowserCosmeticDocumentTicket>
BrowserCosmeticDocumentState::BeginActivation() {
  if (phase_ == BrowserCosmeticDocumentPhase::kBinding ||
      phase_ == BrowserCosmeticDocumentPhase::kReady ||
      phase_ == BrowserCosmeticDocumentPhase::kRevoking) {
    return std::nullopt;
  }
  auto ticket = AdvanceGeneration();
  if (!ticket.has_value()) {
    return std::nullopt;
  }
  phase_ = BrowserCosmeticDocumentPhase::kBinding;
  return ticket;
}

bool BrowserCosmeticDocumentState::CompleteActivation(
    const BrowserCosmeticDocumentTicket& ticket,
    bool accepted) {
  if (!IsCurrent(ticket) || phase_ != BrowserCosmeticDocumentPhase::kBinding) {
    return false;
  }
  phase_ = accepted ? BrowserCosmeticDocumentPhase::kReady
                    : BrowserCosmeticDocumentPhase::kFailed;
  return true;
}

std::optional<BrowserCosmeticDocumentTicket>
BrowserCosmeticDocumentState::BeginRevocation() {
  if (!ready()) {
    return std::nullopt;
  }
  auto ticket = AdvanceGeneration();
  if (!ticket.has_value()) {
    return std::nullopt;
  }
  phase_ = BrowserCosmeticDocumentPhase::kRevoking;
  return ticket;
}

bool BrowserCosmeticDocumentState::CompleteRevocation(
    const BrowserCosmeticDocumentTicket& ticket,
    bool accepted) {
  if (!IsCurrent(ticket) || phase_ != BrowserCosmeticDocumentPhase::kRevoking) {
    return false;
  }
  phase_ = accepted ? BrowserCosmeticDocumentPhase::kRevoked
                    : BrowserCosmeticDocumentPhase::kFailed;
  return true;
}

void BrowserCosmeticDocumentState::Suspend() {
  if (phase_ == BrowserCosmeticDocumentPhase::kSuspended) {
    return;
  }
  AdvanceGeneration();
  if (!generation_exhausted_) {
    phase_ = BrowserCosmeticDocumentPhase::kSuspended;
  }
}

void BrowserCosmeticDocumentState::Fail() {
  AdvanceGeneration();
  phase_ = BrowserCosmeticDocumentPhase::kFailed;
}

bool BrowserCosmeticDocumentState::IsCurrent(
    const BrowserCosmeticDocumentTicket& ticket) const {
  return ticket.generation != 0 && ticket.generation == generation_ &&
         !generation_exhausted_;
}

std::optional<BrowserCosmeticDocumentTicket>
BrowserCosmeticDocumentState::AdvanceGeneration() {
  if (generation_exhausted_ ||
      generation_ == std::numeric_limits<std::uint64_t>::max()) {
    generation_exhausted_ = true;
    generation_ = 0;
    phase_ = BrowserCosmeticDocumentPhase::kFailed;
    return std::nullopt;
  }
  ++generation_;
  return BrowserCosmeticDocumentTicket{generation_};
}

}  // namespace fireball::chromium
