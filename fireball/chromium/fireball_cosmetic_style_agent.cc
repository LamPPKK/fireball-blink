#include "fireball/chromium/fireball_cosmetic_style_agent.h"

#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_css_origin.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/gurl.h"

namespace fireball::chromium {
namespace {

bool IsRejected(const RendererStyleMutation& mutation) {
  return mutation.action == RendererStyleMutationAction::kReject;
}

}  // namespace

// static
void FireballCosmeticStyleAgent::Create(content::RenderFrame* render_frame) {
  if (render_frame != nullptr && render_frame->IsMainFrame()) {
    new FireballCosmeticStyleAgent(render_frame);
  }
}

FireballCosmeticStyleAgent::FireballCosmeticStyleAgent(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame), receiver_(this) {
  render_frame->GetAssociatedInterfaceRegistry()
      ->AddInterface<fireball::mojom::CosmeticStyleAgent>(base::BindRepeating(
          &FireballCosmeticStyleAgent::BindReceiver, base::Unretained(this)));
}

FireballCosmeticStyleAgent::~FireballCosmeticStyleAgent() = default;

void FireballCosmeticStyleAgent::DidCreateNewDocument() {
  receiver_.reset();
  bound_document_token_.reset();
  state_.BeginDocument();
}

void FireballCosmeticStyleAgent::OnDestruct() {
  delete this;
}

void FireballCosmeticStyleAgent::GetDocumentEpoch(
    GetDocumentEpochCallback callback) {
  blink::WebDocument document;
  const std::uint64_t epoch = state_.document_epoch();
  std::move(callback).Run(
      epoch != 0 && CurrentDocument(&document, /*require_bound_token=*/false)
          ? epoch
          : 0);
}

void FireballCosmeticStyleAgent::BindDocument(
    const std::string& document_id,
    std::uint64_t expected_document_epoch,
    BindDocumentCallback callback) {
  auto parsed = browser::DocumentId::Parse(document_id);
  blink::WebDocument document;
  if (!parsed.has_value() ||
      !CurrentDocument(&document, /*require_bound_token=*/false) ||
      !state_.BindDocument(std::move(*parsed), expected_document_epoch)) {
    std::move(callback).Run(false, 0);
    return;
  }
  bound_document_token_ = document.Token();
  std::move(callback).Run(true, state_.binding_generation());
}

void FireballCosmeticStyleAgent::SetStylesheet(
    const std::string& document_id,
    std::uint64_t expected_document_epoch,
    std::uint64_t expected_binding_generation,
    fireball::mojom::CosmeticStyleLayer mojo_layer,
    const std::string& stylesheet,
    SetStylesheetCallback callback) {
  auto parsed = browser::DocumentId::Parse(document_id);
  const auto layer = ConvertLayer(mojo_layer);
  blink::WebDocument document;
  if (!parsed.has_value() || !layer.has_value() ||
      !CurrentDocument(&document, /*require_bound_token=*/true)) {
    std::move(callback).Run(false);
    return;
  }
  RendererStyleMutation mutation =
      state_.PrepareMutation(*parsed, expected_document_epoch,
                             expected_binding_generation, *layer, stylesheet);
  if (IsRejected(mutation)) {
    std::move(callback).Run(false);
    return;
  }
  if (mutation.action == RendererStyleMutationAction::kNoop) {
    std::move(callback).Run(state_.CommitMutation(mutation));
    return;
  }

  if (mutation.action == RendererStyleMutationAction::kRemove) {
    if (!state_.CommitMutation(mutation)) {
      std::move(callback).Run(false);
      return;
    }
    document.RemoveInsertedStyleSheet(
        blink::WebString::FromUtf8(mutation.previous_key),
        blink::WebCssOrigin::kUser);
    std::move(callback).Run(true);
    return;
  }

  const blink::WebString source = blink::WebString::FromUtf8(stylesheet);
  const blink::WebString new_key = blink::WebString::FromUtf8(mutation.new_key);
  if (source.IsNull() || new_key.IsNull()) {
    std::move(callback).Run(false);
    return;
  }
  const blink::WebStyleSheetKey inserted = document.InsertStyleSheet(
      source, &new_key, blink::WebCssOrigin::kUser,
      blink::BackForwardCacheAware::kPossiblyDisallow);
  if (inserted.IsNull() || inserted.Utf8() != mutation.new_key ||
      !state_.CommitMutation(mutation)) {
    if (!inserted.IsNull()) {
      document.RemoveInsertedStyleSheet(inserted, blink::WebCssOrigin::kUser);
    }
    std::move(callback).Run(false);
    return;
  }
  if (!mutation.previous_key.empty()) {
    document.RemoveInsertedStyleSheet(
        blink::WebString::FromUtf8(mutation.previous_key),
        blink::WebCssOrigin::kUser);
  }
  std::move(callback).Run(true);
}

void FireballCosmeticStyleAgent::RemoveDocumentStyles(
    const std::string& document_id,
    std::uint64_t expected_document_epoch,
    std::uint64_t expected_binding_generation,
    RemoveDocumentStylesCallback callback) {
  auto parsed = browser::DocumentId::Parse(document_id);
  blink::WebDocument document;
  if (!parsed.has_value() ||
      !CurrentDocument(&document, /*require_bound_token=*/true)) {
    std::move(callback).Run(false);
    return;
  }
  const RendererStyleMutation probe = state_.PrepareMutation(
      *parsed, expected_document_epoch, expected_binding_generation,
      navigation::CosmeticStyleLayer::kDocument, "");
  if (IsRejected(probe)) {
    std::move(callback).Run(false);
    return;
  }
  RemoveBoundStylesAndSuspend();
  std::move(callback).Run(true);
}

void FireballCosmeticStyleAgent::BindReceiver(
    mojo::PendingAssociatedReceiver<fireball::mojom::CosmeticStyleAgent>
        receiver) {
  if (receiver_.is_bound()) {
    RemoveBoundStylesAndSuspend();
    receiver_.reset();
  }
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(
      base::BindOnce(&FireballCosmeticStyleAgent::OnReceiverDisconnected,
                     base::Unretained(this)));
}

void FireballCosmeticStyleAgent::OnReceiverDisconnected() {
  receiver_.reset();
  RemoveBoundStylesAndSuspend();
}

void FireballCosmeticStyleAgent::RemoveBoundStylesAndSuspend() {
  if (render_frame() != nullptr && render_frame()->GetWebFrame() != nullptr) {
    blink::WebDocument document = render_frame()->GetWebFrame()->GetDocument();
    if (!document.IsNull() && bound_document_token_.has_value() &&
        document.Token() == *bound_document_token_) {
      for (const navigation::CosmeticStyleLayer layer :
           {navigation::CosmeticStyleLayer::kDocument,
            navigation::CosmeticStyleLayer::kGeneric}) {
        const std::string key = state_.CurrentKey(layer);
        if (!key.empty()) {
          document.RemoveInsertedStyleSheet(blink::WebString::FromUtf8(key),
                                            blink::WebCssOrigin::kUser);
        }
      }
    }
  }
  bound_document_token_.reset();
  state_.SuspendBinding();
}

bool FireballCosmeticStyleAgent::CurrentDocument(
    blink::WebDocument* document,
    bool require_bound_token) const {
  if (document == nullptr || render_frame() == nullptr ||
      !render_frame()->IsMainFrame() ||
      render_frame()->GetWebFrame() == nullptr) {
    return false;
  }
  *document = render_frame()->GetWebFrame()->GetDocument();
  if (document->IsNull() || !document->IsActive() ||
      (!document->IsHTMLDocument() && !document->IsXHTMLDocument())) {
    return false;
  }
  const GURL url = document->Url();
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }
  if (!require_bound_token) {
    return true;
  }
  return bound_document_token_.has_value() && state_.HasBoundDocument() &&
         document->Token() == *bound_document_token_;
}

// static
std::optional<navigation::CosmeticStyleLayer>
FireballCosmeticStyleAgent::ConvertLayer(
    fireball::mojom::CosmeticStyleLayer layer) {
  switch (layer) {
    case fireball::mojom::CosmeticStyleLayer::kDocument:
      return navigation::CosmeticStyleLayer::kDocument;
    case fireball::mojom::CosmeticStyleLayer::kGeneric:
      return navigation::CosmeticStyleLayer::kGeneric;
  }
  return std::nullopt;
}

}  // namespace fireball::chromium
