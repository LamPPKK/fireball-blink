#include "fireball/chromium/fireball_cosmetic_lifecycle_owner.h"

#include <cstdint>
#include <limits>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/uuid.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace fireball::chromium {
namespace {

browser::DocumentId GenerateDocumentId() {
  auto parsed = browser::DocumentId::Parse(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  CHECK(parsed.has_value());
  return std::move(*parsed);
}

}  // namespace

FireballCosmeticLifecycleOwner::FireballCosmeticLifecycleOwner(
    content::WebContents& web_contents,
    FireballCosmeticLifecycleDelegate& delegate)
    : content::WebContentsObserver(&web_contents), delegate_(&delegate) {}

FireballCosmeticLifecycleOwner::~FireballCosmeticLifecycleOwner() = default;

void FireballCosmeticLifecycleOwner::PrimaryPageChanged(content::Page& page) {
  content::RenderFrameHost& frame = page.GetMainDocument();
  if (!page.IsPrimary() || !frame.IsInPrimaryMainFrame() || !frame.IsActive()) {
    return;
  }
  content::WeakDocumentPtr primary_document = frame.GetWeakDocumentPtr();

  const std::uint64_t lifecycle_generation = AdvanceLifecycleGeneration();
  if (active_document_id_.has_value()) {
    const browser::DocumentId old_document_id = *active_document_id_;
    content::WeakDocumentPtr old_document = active_document_;
    active_document_id_.reset();
    active_document_ = content::WeakDocumentPtr();

    base::WeakPtr<FireballCosmeticLifecycleOwner> alive =
        weak_factory_.GetWeakPtr();
    content::RenderFrameHost* old_frame =
        old_document.AsRenderFrameHostIfValid();
    if (old_frame != nullptr) {
      FireballCosmeticDocumentHost* old_host =
          FireballCosmeticDocumentHost::GetForCurrentDocument(old_frame);
      if (old_host != nullptr) {
        old_host->Suspend();
      }
    }
    if (!alive || lifecycle_generation_ != lifecycle_generation) {
      return;
    }
    delegate_->OnCosmeticDocumentSuspended(old_document_id);
    if (!alive || lifecycle_generation_ != lifecycle_generation) {
      return;
    }
  }

  content::RenderFrameHost* current_frame =
      primary_document.AsRenderFrameHostIfValid();
  if (current_frame == nullptr || !current_frame->IsInPrimaryMainFrame() ||
      !current_frame->IsActive()) {
    return;
  }
  FireballCosmeticDocumentHost* host =
      FireballCosmeticDocumentHost::GetForCurrentDocument(current_frame);
  if (host == nullptr) {
    FireballCosmeticDocumentHost::CreateForCurrentDocument(
        current_frame, GenerateDocumentId());
    host = FireballCosmeticDocumentHost::GetForCurrentDocument(current_frame);
    CHECK(host != nullptr);
  }
  const browser::DocumentId document_id = host->document_id();
  active_document_id_ = document_id;
  active_document_ = primary_document;
  host->Activate(
      base::BindOnce(&FireballCosmeticLifecycleOwner::OnDocumentActivated,
                     weak_factory_.GetWeakPtr(), lifecycle_generation,
                     primary_document, document_id));
}

void FireballCosmeticLifecycleOwner::RenderFrameHostStateChanged(
    content::RenderFrameHost* render_frame_host,
    content::RenderFrameHost::LifecycleState old_state,
    content::RenderFrameHost::LifecycleState new_state) {
  if (render_frame_host == nullptr ||
      old_state != content::RenderFrameHost::LifecycleState::kActive ||
      new_state == content::RenderFrameHost::LifecycleState::kActive) {
    return;
  }
  FireballCosmeticDocumentHost* host =
      FireballCosmeticDocumentHost::GetForCurrentDocument(render_frame_host);
  if (host == nullptr) {
    return;
  }
  const browser::DocumentId document_id = host->document_id();
  const bool tracked =
      active_document_id_.has_value() && *active_document_id_ == document_id;
  if (tracked) {
    AdvanceLifecycleGeneration();
    active_document_id_.reset();
    active_document_ = content::WeakDocumentPtr();
  } else if (host->phase() == BrowserCosmeticDocumentPhase::kSuspended) {
    return;
  }

  base::WeakPtr<FireballCosmeticLifecycleOwner> alive =
      weak_factory_.GetWeakPtr();
  host->Suspend();
  if (!alive || !tracked) {
    return;
  }
  delegate_->OnCosmeticDocumentSuspended(document_id);
}

void FireballCosmeticLifecycleOwner::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus) {
  content::RenderFrameHost* frame = web_contents()->GetPrimaryMainFrame();
  if (frame == nullptr) {
    return;
  }
  FireballCosmeticDocumentHost* host =
      FireballCosmeticDocumentHost::GetForCurrentDocument(frame);
  if (host == nullptr) {
    return;
  }
  const browser::DocumentId document_id = host->document_id();
  AdvanceLifecycleGeneration();
  active_document_id_.reset();
  active_document_ = content::WeakDocumentPtr();

  content::WeakDocumentPtr crashed_document = frame->GetWeakDocumentPtr();
  base::WeakPtr<FireballCosmeticLifecycleOwner> alive =
      weak_factory_.GetWeakPtr();
  host->Suspend();
  if (!alive) {
    return;
  }
  frame = crashed_document.AsRenderFrameHostIfValid();
  if (frame != nullptr) {
    host = FireballCosmeticDocumentHost::GetForCurrentDocument(frame);
    if (host != nullptr && host->document_id() == document_id) {
      FireballCosmeticDocumentHost::DeleteForCurrentDocument(frame);
    }
  }
  if (!alive) {
    return;
  }
  delegate_->OnCosmeticDocumentFailed(document_id,
                                      "COSMETIC_RENDERER_PROCESS_GONE");
}

std::uint64_t FireballCosmeticLifecycleOwner::AdvanceLifecycleGeneration() {
  CHECK_NE(lifecycle_generation_, std::numeric_limits<std::uint64_t>::max());
  return ++lifecycle_generation_;
}

void FireballCosmeticLifecycleOwner::OnDocumentActivated(
    std::uint64_t lifecycle_generation,
    content::WeakDocumentPtr expected_document,
    browser::DocumentId document_id,
    CosmeticTransportResult result) {
  content::RenderFrameHost* frame =
      expected_document.AsRenderFrameHostIfValid();
  if (lifecycle_generation != lifecycle_generation_ ||
      !active_document_id_.has_value() || *active_document_id_ != document_id ||
      frame == nullptr || !frame->IsInPrimaryMainFrame() ||
      !frame->IsActive()) {
    return;
  }
  FireballCosmeticDocumentHost* host =
      FireballCosmeticDocumentHost::GetForCurrentDocument(frame);
  if (host == nullptr || host->document_id() != document_id) {
    return;
  }
  if (result.status == CosmeticTransportStatus::kBound && host->ready()) {
    delegate_->OnCosmeticDocumentReady(document_id, *host);
    return;
  }
  delegate_->OnCosmeticDocumentFailed(
      document_id, result.error_code.empty()
                       ? std::string_view("COSMETIC_DOCUMENT_BIND_FAILED")
                       : std::string_view(result.error_code));
}

}  // namespace fireball::chromium
