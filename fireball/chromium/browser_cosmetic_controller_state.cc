#include "fireball/chromium/browser_cosmetic_controller_state.h"

#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace fireball::chromium {
namespace {

bool AddBoundedTokens(const std::vector<std::string> &values,
                      std::size_t *entry_count, std::size_t *byte_count) {
  if (entry_count == nullptr || byte_count == nullptr ||
      values.size() > kMaximumCosmeticDomEntries - *entry_count) {
    return false;
  }
  *entry_count += values.size();
  for (const std::string &value : values) {
    if (value.empty() || value.size() > kMaximumCosmeticDomTokenBytes ||
        value.find('\0') != std::string::npos ||
        value.size() > kMaximumCosmeticDomSnapshotBytes - *byte_count) {
      return false;
    }
    *byte_count += value.size();
  }
  return true;
}

} // namespace

bool IsBoundedCosmeticDomSnapshot(const std::vector<std::string> &classes,
                                  const std::vector<std::string> &ids) {
  std::size_t entry_count = 0;
  std::size_t byte_count = 0;
  return AddBoundedTokens(classes, &entry_count, &byte_count) &&
         AddBoundedTokens(ids, &entry_count, &byte_count);
}

std::optional<BrowserCosmeticControllerTicket>
BrowserCosmeticControllerState::BeginActivation(
    browser::DocumentId document_id, std::uint64_t restored_dom_revision) {
  if (phase_ == BrowserCosmeticControllerPhase::kActivating ||
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
