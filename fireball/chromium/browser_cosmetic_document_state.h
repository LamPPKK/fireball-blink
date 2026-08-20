#ifndef FIREBALL_CHROMIUM_BROWSER_COSMETIC_DOCUMENT_STATE_H_
#define FIREBALL_CHROMIUM_BROWSER_COSMETIC_DOCUMENT_STATE_H_

#include <cstdint>
#include <optional>

namespace fireball::chromium {

enum class BrowserCosmeticDocumentPhase {
  kDormant,
  kBinding,
  kReady,
  kSuspended,
  kRevoking,
  kRevoked,
  kFailed,
};

struct BrowserCosmeticDocumentTicket {
  std::uint64_t generation = 0;

  friend bool operator==(const BrowserCosmeticDocumentTicket&,
                         const BrowserCosmeticDocumentTicket&) = default;
};

// Chromium-independent lifecycle state for one Blink document. A generation
// is advanced before every bind/revoke operation and whenever the document is
// suspended or failed, so an acknowledgement from an older active lifetime
// cannot publish state after BFCache, navigation or renderer failure.
class BrowserCosmeticDocumentState final {
 public:
  std::optional<BrowserCosmeticDocumentTicket> BeginActivation();
  bool CompleteActivation(const BrowserCosmeticDocumentTicket& ticket,
                          bool accepted);

  std::optional<BrowserCosmeticDocumentTicket> BeginRevocation();
  bool CompleteRevocation(const BrowserCosmeticDocumentTicket& ticket,
                          bool accepted);

  void Suspend();
  void Fail();

  bool IsCurrent(const BrowserCosmeticDocumentTicket& ticket) const;
  bool ready() const { return phase_ == BrowserCosmeticDocumentPhase::kReady; }
  BrowserCosmeticDocumentPhase phase() const { return phase_; }

 private:
  std::optional<BrowserCosmeticDocumentTicket> AdvanceGeneration();

  BrowserCosmeticDocumentPhase phase_ = BrowserCosmeticDocumentPhase::kDormant;
  std::uint64_t generation_ = 0;
  bool generation_exhausted_ = false;
};

}  // namespace fireball::chromium

#endif  // FIREBALL_CHROMIUM_BROWSER_COSMETIC_DOCUMENT_STATE_H_
