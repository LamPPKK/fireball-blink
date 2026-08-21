#include "fireball/chromium/fireball_cosmetic_style_agent.h"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/time/time.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_css_origin.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/gurl.h"

namespace fireball::chromium {
namespace {

constexpr base::TimeDelta kMaximumDomLoadWait = base::Seconds(5);

bool IsRejected(const RendererStyleMutation& mutation) {
  return mutation.action == RendererStyleMutationAction::kReject;
}

std::string BoundedUtf8(const blink::WebString& value) {
  if (value.IsNull() ||
      value.length() > kMaximumCosmeticDomAttributeCodeUnits) {
    return {};
  }
  return value.Utf8(blink::WebString::Utf8ConversionMode::kStrict);
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
  CancelPendingDomSnapshot();
  receiver_.reset();
  bound_document_token_.reset();
  state_.BeginDocument();
}

void FireballCosmeticStyleAgent::DidFinishLoad() {
  CompletePendingDomSnapshot();
}

void FireballCosmeticStyleAgent::OnDestruct() {
  CancelPendingDomSnapshot();
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

void FireballCosmeticStyleAgent::CollectDomSnapshot(
    const std::string& document_id,
    std::uint64_t expected_document_epoch,
    std::uint64_t expected_binding_generation,
    CollectDomSnapshotCallback callback) {
  auto parsed = browser::DocumentId::Parse(document_id);
  blink::WebDocument document;
  if (pending_dom_snapshot_callback_ || !parsed.has_value() ||
      !CurrentDocument(&document, /*require_bound_token=*/true)) {
    ReplyDomSnapshotRejected(std::move(callback));
    return;
  }
  if (!document.IsLoaded()) {
    pending_dom_snapshot_callback_ = std::move(callback);
    pending_dom_document_id_ = document_id;
    pending_dom_document_epoch_ = expected_document_epoch;
    pending_dom_binding_generation_ = expected_binding_generation;
    dom_collection_timer_.Start(
        FROM_HERE, kMaximumDomLoadWait,
        base::BindOnce(
            &FireballCosmeticStyleAgent::CompletePendingDomSnapshot,
            base::Unretained(this)));
    return;
  }
  CollectDomSnapshotNow(document_id, expected_document_epoch,
                        expected_binding_generation, std::move(callback));
}

void FireballCosmeticStyleAgent::CollectDomSnapshotNow(
    const std::string& document_id,
    std::uint64_t expected_document_epoch,
    std::uint64_t expected_binding_generation,
    CollectDomSnapshotCallback callback) {
  auto parsed = browser::DocumentId::Parse(document_id);
  blink::WebDocument document;
  if (!parsed.has_value() ||
      !CurrentDocument(&document, /*require_bound_token=*/true)) {
    ReplyDomSnapshotRejected(std::move(callback));
    return;
  }
  const std::optional<std::uint64_t> revision =
      state_.NextDomSnapshotRevision(*parsed, expected_document_epoch,
                                     expected_binding_generation);
  if (!revision.has_value()) {
    ReplyDomSnapshotRejected(std::move(callback));
    return;
  }

  blink::WebElementCollection elements = document.All();
  if (!elements) {
    ReplyDomSnapshotRejected(std::move(callback));
    return;
  }
  CosmeticDomSnapshotBuilder builder;
  const blink::WebString class_attribute_name =
      blink::WebString::FromAscii("class");
  std::size_t scanned_element_count = 0;
  for (blink::WebElement element = elements.FirstItem(); !element.IsNull();
       element = elements.NextItem()) {
    if (scanned_element_count == kMaximumCosmeticDomElements) {
      ReplyDomSnapshotLimited(*revision, std::move(callback));
      return;
    }
    ++scanned_element_count;
    if (!builder.AddElement(BoundedUtf8(element.GetIdAttribute()),
                            BoundedUtf8(element.GetAttribute(
                                class_attribute_name)))) {
      ReplyDomSnapshotLimited(*revision, std::move(callback));
      return;
    }
  }

  blink::WebDocument current_document;
  if (!CurrentDocument(&current_document, /*require_bound_token=*/true) ||
      !(current_document.Token() == document.Token())) {
    ReplyDomSnapshotRejected(std::move(callback));
    return;
  }
  std::optional<CosmeticDomSnapshot> snapshot =
      std::move(builder).Finish(*revision);
  if (!snapshot.has_value()) {
    ReplyDomSnapshotLimited(*revision, std::move(callback));
    return;
  }
  std::optional<CosmeticDomWireSnapshot> wire =
      EncodeCosmeticDomSnapshot(*snapshot);
  if (!wire.has_value()) {
    ReplyDomSnapshotLimited(*revision, std::move(callback));
    return;
  }
  std::move(callback).Run(
      true, false, snapshot->revision, wire->payload_size, wire->class_count,
      wire->id_count, std::move(wire->payload));
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
  CancelPendingDomSnapshot();
  RemoveBoundStylesAndSuspend();
  std::move(callback).Run(true);
}

void FireballCosmeticStyleAgent::BindReceiver(
    mojo::PendingAssociatedReceiver<fireball::mojom::CosmeticStyleAgent>
        receiver) {
  if (receiver_.is_bound()) {
    CancelPendingDomSnapshot();
    RemoveBoundStylesAndSuspend();
    receiver_.reset();
  }
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(
      base::BindOnce(&FireballCosmeticStyleAgent::OnReceiverDisconnected,
                     base::Unretained(this)));
}

void FireballCosmeticStyleAgent::OnReceiverDisconnected() {
  CancelPendingDomSnapshot();
  receiver_.reset();
  RemoveBoundStylesAndSuspend();
}

void FireballCosmeticStyleAgent::CompletePendingDomSnapshot() {
  if (!pending_dom_snapshot_callback_) {
    return;
  }
  dom_collection_timer_.Stop();
  CollectDomSnapshotCallback callback =
      std::move(pending_dom_snapshot_callback_);
  std::string document_id = std::move(pending_dom_document_id_);
  const std::uint64_t document_epoch = pending_dom_document_epoch_;
  const std::uint64_t binding_generation =
      pending_dom_binding_generation_;
  pending_dom_document_epoch_ = 0;
  pending_dom_binding_generation_ = 0;
  CollectDomSnapshotNow(document_id, document_epoch, binding_generation,
                        std::move(callback));
}

void FireballCosmeticStyleAgent::CancelPendingDomSnapshot() {
  dom_collection_timer_.Stop();
  pending_dom_snapshot_callback_.Reset();
  pending_dom_document_id_.clear();
  pending_dom_document_epoch_ = 0;
  pending_dom_binding_generation_ = 0;
}

void FireballCosmeticStyleAgent::ReplyDomSnapshotRejected(
    CollectDomSnapshotCallback callback) {
  std::move(callback).Run(
      false, false, 0, 0, 0, 0,
      std::vector<std::uint8_t>(kMaximumCosmeticDomWireBytes, 0));
}

void FireballCosmeticStyleAgent::ReplyDomSnapshotLimited(
    std::uint64_t revision,
    CollectDomSnapshotCallback callback) {
  std::move(callback).Run(
      false, true, revision, 0, 0, 0,
      std::vector<std::uint8_t>(kMaximumCosmeticDomWireBytes, 0));
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
