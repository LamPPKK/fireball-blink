#ifndef FIREBALL_CHROMIUM_FIREBALL_COSMETIC_STYLE_AGENT_H_
#define FIREBALL_CHROMIUM_FIREBALL_COSMETIC_STYLE_AGENT_H_

#include <cstdint>
#include <optional>
#include <string>

#include "base/timer/timer.h"
#include "content/public/renderer/render_frame_observer.h"
#include "fireball/chromium/cosmetic_style_agent.mojom.h"
#include "fireball/chromium/cosmetic_dom_snapshot.h"
#include "fireball/chromium/renderer_cosmetic_style_state.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/web/web_document.h"

namespace content {
class RenderFrame;
}

namespace fireball::chromium {

// Main-frame renderer endpoint for browser-owned cosmetic styles. Registration
// from Fireball's future ContentRendererClient overlay is a separate gate.
class FireballCosmeticStyleAgent final
    : public content::RenderFrameObserver,
      public fireball::mojom::CosmeticStyleAgent {
 public:
  static void Create(content::RenderFrame* render_frame);

  FireballCosmeticStyleAgent(const FireballCosmeticStyleAgent&) = delete;
  FireballCosmeticStyleAgent& operator=(const FireballCosmeticStyleAgent&) =
      delete;

  // content::RenderFrameObserver:
  void DidCreateNewDocument() override;
  void DidFinishLoad() override;
  void OnDestruct() override;

  // fireball::mojom::CosmeticStyleAgent:
  void GetDocumentEpoch(GetDocumentEpochCallback callback) override;
  void BindDocument(const std::string& document_id,
                    std::uint64_t expected_document_epoch,
                    BindDocumentCallback callback) override;
  void SetStylesheet(const std::string& document_id,
                     std::uint64_t expected_document_epoch,
                     std::uint64_t expected_binding_generation,
                     fireball::mojom::CosmeticStyleLayer layer,
                     const std::string& stylesheet,
                     SetStylesheetCallback callback) override;
  void CollectDomSnapshot(const std::string& document_id,
                          std::uint64_t expected_document_epoch,
                          std::uint64_t expected_binding_generation,
                          CollectDomSnapshotCallback callback) override;
  void RemoveDocumentStyles(const std::string& document_id,
                            std::uint64_t expected_document_epoch,
                            std::uint64_t expected_binding_generation,
                            RemoveDocumentStylesCallback callback) override;

 private:
  explicit FireballCosmeticStyleAgent(content::RenderFrame* render_frame);
  ~FireballCosmeticStyleAgent() override;

  void BindReceiver(
      mojo::PendingAssociatedReceiver<fireball::mojom::CosmeticStyleAgent>
          receiver);
  void OnReceiverDisconnected();
  void CompletePendingDomSnapshot();
  void CancelPendingDomSnapshot();
  void CollectDomSnapshotNow(const std::string& document_id,
                             std::uint64_t expected_document_epoch,
                             std::uint64_t expected_binding_generation,
                             CollectDomSnapshotCallback callback);
  void ReplyDomSnapshotRejected(CollectDomSnapshotCallback callback);
  void ReplyDomSnapshotLimited(std::uint64_t revision,
                               CollectDomSnapshotCallback callback);
  void RemoveBoundStylesAndSuspend();
  bool CurrentDocument(blink::WebDocument* document,
                       bool require_bound_token) const;
  static std::optional<navigation::CosmeticStyleLayer> ConvertLayer(
      fireball::mojom::CosmeticStyleLayer layer);

  RendererCosmeticStyleState state_;
  std::optional<blink::DocumentToken> bound_document_token_;
  mojo::AssociatedReceiver<fireball::mojom::CosmeticStyleAgent> receiver_;
  CollectDomSnapshotCallback pending_dom_snapshot_callback_;
  std::string pending_dom_document_id_;
  std::uint64_t pending_dom_document_epoch_ = 0;
  std::uint64_t pending_dom_binding_generation_ = 0;
  base::OneShotTimer dom_collection_timer_;
};

}  // namespace fireball::chromium

#endif  // FIREBALL_CHROMIUM_FIREBALL_COSMETIC_STYLE_AGENT_H_
