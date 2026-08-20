#ifndef FIREBALL_CHROMIUM_FIREBALL_COSMETIC_STYLE_AGENT_H_
#define FIREBALL_CHROMIUM_FIREBALL_COSMETIC_STYLE_AGENT_H_

#include <cstdint>
#include <optional>
#include <string>

#include "content/public/renderer/render_frame_observer.h"
#include "fireball/chromium/cosmetic_style_agent.mojom.h"
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
  void OnDestruct() override;

  // fireball::mojom::CosmeticStyleAgent:
  void GetDocumentEpoch(GetDocumentEpochCallback callback) override;
  void BindDocument(const std::string& document_id,
                    std::uint64_t expected_document_epoch,
                    BindDocumentCallback callback) override;
  void SetStylesheet(const std::string& document_id,
                     std::uint64_t expected_document_epoch,
                     fireball::mojom::CosmeticStyleLayer layer,
                     const std::string& stylesheet,
                     SetStylesheetCallback callback) override;
  void RemoveDocumentStyles(const std::string& document_id,
                            std::uint64_t expected_document_epoch,
                            RemoveDocumentStylesCallback callback) override;

 private:
  explicit FireballCosmeticStyleAgent(content::RenderFrame* render_frame);
  ~FireballCosmeticStyleAgent() override;

  void BindReceiver(
      mojo::PendingAssociatedReceiver<fireball::mojom::CosmeticStyleAgent>
          receiver);
  void OnReceiverDisconnected();
  bool CurrentDocument(blink::WebDocument* document,
                       bool require_bound_token) const;
  static std::optional<navigation::CosmeticStyleLayer> ConvertLayer(
      fireball::mojom::CosmeticStyleLayer layer);

  RendererCosmeticStyleState state_;
  std::optional<blink::DocumentToken> bound_document_token_;
  mojo::AssociatedReceiver<fireball::mojom::CosmeticStyleAgent> receiver_;
};

}  // namespace fireball::chromium

#endif  // FIREBALL_CHROMIUM_FIREBALL_COSMETIC_STYLE_AGENT_H_
