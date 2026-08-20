#ifndef FIREBALL_CHROMIUM_BROWSER_COSMETIC_CONTROLLER_STATE_H_
#define FIREBALL_CHROMIUM_BROWSER_COSMETIC_CONTROLLER_STATE_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "fireball/browser/domain_model.h"

namespace fireball::chromium {

inline constexpr std::size_t kMaximumCosmeticDomEntries = 4096;
inline constexpr std::size_t kMaximumCosmeticDomTokenBytes = 256;
inline constexpr std::size_t kMaximumCosmeticDomSnapshotBytes = 256 * 1024;

bool IsBoundedCosmeticDomSnapshot(const std::vector<std::string> &classes,
                                  const std::vector<std::string> &ids);

enum class BrowserCosmeticControllerPhase {
  kIdle,
  kActivating,
  kReady,
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
