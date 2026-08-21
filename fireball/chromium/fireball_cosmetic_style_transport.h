#ifndef FIREBALL_CHROMIUM_FIREBALL_COSMETIC_STYLE_TRANSPORT_H_
#define FIREBALL_CHROMIUM_FIREBALL_COSMETIC_STYLE_TRANSPORT_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/weak_document_ptr.h"
#include "fireball/browser/domain_model.h"
#include "fireball/chromium/browser_cosmetic_transport_state.h"
#include "fireball/chromium/cosmetic_dom_snapshot.h"
#include "fireball/chromium/cosmetic_style_agent.mojom.h"
#include "fireball/components/navigation/document_cosmetic_controller.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace content {
class RenderFrameHost;
}

namespace fireball::chromium {

enum class CosmeticTransportStatus {
  kBound,
  kCollected,
  kLimited,
  kApplied,
  kRevoked,
  kError,
};

// Safe for controller and diagnostic use: no URL, CSS, selector, document
// epoch or DocumentId crosses this result boundary.
struct CosmeticTransportResult {
  CosmeticTransportStatus status = CosmeticTransportStatus::kError;
  std::string error_code;
};

// Browser-side asynchronous Mojo owner for exactly one committed primary main
// document. The WeakDocumentPtr and renderer epoch are independent freshness
// checks; every callback also carries a local generation ticket. Destruction
// cancels an in-flight completion; the lifecycle owner must call Invalidate()
// first when it needs to deliver an explicit cancellation result.
class FireballCosmeticStyleTransport final {
 public:
  using CompletionCallback = base::OnceCallback<void(CosmeticTransportResult)>;
  using DomSnapshotCallback =
      base::OnceCallback<void(CosmeticTransportResult, CosmeticDomSnapshot)>;

  FireballCosmeticStyleTransport(content::RenderFrameHost& render_frame_host,
                                 browser::DocumentId document_id);
  ~FireballCosmeticStyleTransport();

  FireballCosmeticStyleTransport(const FireballCosmeticStyleTransport&) =
      delete;
  FireballCosmeticStyleTransport& operator=(
      const FireballCosmeticStyleTransport&) = delete;

  void BindDocument(CompletionCallback callback);
  void CollectDomSnapshot(DomSnapshotCallback callback);
  bool CancelDomSnapshot();
  void SetStylesheet(navigation::CosmeticStyleLayer layer,
                     std::string stylesheet,
                     CompletionCallback callback);
  void RemoveDocumentStyles(CompletionCallback callback);

  // Called by the future WebContents lifecycle owner before dropping this
  // document, entering an inactive lifecycle state or tearing down the Tab.
  void Invalidate();

  bool ready() const { return state_.ready(); }
  BrowserCosmeticTransportPhase phase() const { return state_.phase(); }

 private:
  content::RenderFrameHost* ActivePrimaryDocument() const;
  void OnDocumentEpoch(BrowserCosmeticTransportTicket ticket,
                       std::uint64_t document_epoch);
  void OnDocumentBound(BrowserCosmeticTransportTicket ticket,
                       bool bound,
                       std::uint64_t binding_generation);
  void OnDomSnapshotCollected(BrowserCosmeticTransportTicket ticket,
                              bool collected,
                              bool limit_exceeded,
                              std::uint64_t revision,
                              std::uint32_t payload_size,
                              std::uint32_t class_count,
                              std::uint32_t id_count,
                              std::vector<std::uint8_t> payload);
  void OnMutationCompleted(BrowserCosmeticTransportTicket ticket,
                           bool accepted);
  void OnDisconnected();
  void FailCurrent(const BrowserCosmeticTransportTicket& ticket,
                   std::string error_code);
  void Finish(CosmeticTransportStatus status, std::string error_code = {});
  void FinishDomSnapshot(CosmeticTransportStatus status,
                         std::string error_code = {},
                         CosmeticDomSnapshot snapshot = {});
  static fireball::mojom::CosmeticStyleLayer ConvertLayer(
      navigation::CosmeticStyleLayer layer);

  content::WeakDocumentPtr document_;
  browser::DocumentId document_id_;
  BrowserCosmeticTransportState state_;
  mojo::AssociatedRemote<fireball::mojom::CosmeticStyleAgent> remote_;
  CompletionCallback pending_callback_;
  DomSnapshotCallback pending_dom_snapshot_callback_;
  base::WeakPtrFactory<FireballCosmeticStyleTransport> weak_factory_{this};
};

}  // namespace fireball::chromium

#endif  // FIREBALL_CHROMIUM_FIREBALL_COSMETIC_STYLE_TRANSPORT_H_
