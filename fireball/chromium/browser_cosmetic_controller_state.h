#ifndef FIREBALL_CHROMIUM_BROWSER_COSMETIC_CONTROLLER_STATE_H_
#define FIREBALL_CHROMIUM_BROWSER_COSMETIC_CONTROLLER_STATE_H_

#include <cstdint>
#include <optional>

#include "fireball/browser/domain_model.h"
#include "fireball/chromium/cosmetic_dom_snapshot.h"

namespace fireball::chromium {

enum class BrowserCosmeticControllerPhase {
  kIdle,
  kActivating,
  kReady,
  kCollectingDom,
  kApplyingGeneric,
  kRevoking,
  kSuspended,
  kFailed,
};

struct BrowserCosmeticControllerTicket {
  std::uint64_t generation;
  browser::DocumentId document_id;
  std::uint64_t dom_revision;
};

// Chromium-independent state for the asynchronous policy -> renderer bridge.
// Every operation advances a generation, so a late renderer acknowledgement
// cannot commit policy state after navigation, BFCache suspension or crash.
class BrowserCosmeticControllerState final {
public:
  std::optional<BrowserCosmeticControllerTicket>
  BeginActivation(browser::DocumentId document_id,
                  std::uint64_t restored_dom_revision);
  bool CompleteActivation(const BrowserCosmeticControllerTicket &ticket,
                          bool accepted);

  std::optional<BrowserCosmeticControllerTicket>
  BeginDomCollection(const browser::DocumentId &document_id);
  bool CompleteDomCollection(const BrowserCosmeticControllerTicket &ticket);
  bool CancelDomCollection(const browser::DocumentId &document_id);

  std::optional<BrowserCosmeticControllerTicket>
  BeginGenericMutation(const browser::DocumentId &document_id,
                       std::uint64_t dom_revision);
  bool CompleteGenericMutation(const BrowserCosmeticControllerTicket &ticket,
                               bool accepted);

  std::optional<BrowserCosmeticControllerTicket>
  BeginRevocation(const browser::DocumentId &document_id);
  bool CompleteRevocation(const BrowserCosmeticControllerTicket &ticket,
                          bool accepted);

  bool Suspend(const browser::DocumentId &document_id);
  bool Fail(const browser::DocumentId &document_id);
  bool Reset(const browser::DocumentId &document_id);
  bool Dispose(const browser::DocumentId &document_id);
  void ResetAll();

  bool IsCurrent(const BrowserCosmeticControllerTicket &ticket) const;
  bool ready() const {
    return phase_ == BrowserCosmeticControllerPhase::kReady;
  }
  bool collecting_dom() const {
    return phase_ == BrowserCosmeticControllerPhase::kCollectingDom;
  }
  BrowserCosmeticControllerPhase phase() const { return phase_; }
  const std::optional<browser::DocumentId> &active_document_id() const {
    return active_document_id_;
  }
  std::uint64_t last_dom_revision() const { return last_dom_revision_; }

private:
  std::optional<std::uint64_t> AdvanceGeneration();

  BrowserCosmeticControllerPhase phase_ = BrowserCosmeticControllerPhase::kIdle;
  std::optional<browser::DocumentId> active_document_id_;
  std::uint64_t last_dom_revision_ = 0;
  std::uint64_t generation_ = 0;
  bool generation_exhausted_ = false;
};

} // namespace fireball::chromium

#endif // FIREBALL_CHROMIUM_BROWSER_COSMETIC_CONTROLLER_STATE_H_
