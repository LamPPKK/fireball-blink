#ifndef FIREBALL_CHROMIUM_BROWSER_COSMETIC_TRANSPORT_STATE_H_
#define FIREBALL_CHROMIUM_BROWSER_COSMETIC_TRANSPORT_STATE_H_

#include <cstdint>
#include <optional>

namespace fireball::chromium {

enum class BrowserCosmeticTransportPhase {
  kUnbound,
  kReadingDocumentEpoch,
  kBindingDocument,
  kReady,
  kApplyingStylesheet,
  kRevokingDocument,
  kRevoked,
  kFailed,
};

struct BrowserCosmeticTransportTicket {
  std::uint64_t generation = 0;

  friend bool operator==(const BrowserCosmeticTransportTicket&,
                         const BrowserCosmeticTransportTicket&) = default;
};

// Chromium-independent state for one browser-side remote bound to one
// document-scoped WeakDocumentPtr. Every asynchronous operation or handshake
// carries a generation so late callbacks cannot complete a newer operation.
class BrowserCosmeticTransportState final {
 public:
  std::optional<BrowserCosmeticTransportTicket> BeginBinding();
  bool AcceptDocumentEpoch(const BrowserCosmeticTransportTicket& ticket,
                           std::uint64_t document_epoch);
  bool CompleteBinding(const BrowserCosmeticTransportTicket& ticket,
                       bool accepted,
                       std::uint64_t binding_generation);

  std::optional<BrowserCosmeticTransportTicket> BeginMutation(
      bool revoke_document);
  bool CompleteMutation(const BrowserCosmeticTransportTicket& ticket,
                        bool accepted);

  void Invalidate();

  bool IsCurrent(const BrowserCosmeticTransportTicket& ticket) const;
  bool ready() const { return phase_ == BrowserCosmeticTransportPhase::kReady; }
  BrowserCosmeticTransportPhase phase() const { return phase_; }
  std::uint64_t document_epoch() const { return document_epoch_; }
  std::uint64_t binding_generation() const { return binding_generation_; }

 private:
  std::optional<BrowserCosmeticTransportTicket> AdvanceGeneration();

  BrowserCosmeticTransportPhase phase_ =
      BrowserCosmeticTransportPhase::kUnbound;
  std::uint64_t generation_ = 0;
  std::uint64_t document_epoch_ = 0;
  std::uint64_t binding_generation_ = 0;
  bool generation_exhausted_ = false;
};

}  // namespace fireball::chromium

#endif  // FIREBALL_CHROMIUM_BROWSER_COSMETIC_TRANSPORT_STATE_H_
