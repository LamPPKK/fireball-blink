#ifndef FIREBALL_CHROMIUM_FIREBALL_COSMETIC_LIFECYCLE_OWNER_H_
#define FIREBALL_CHROMIUM_FIREBALL_COSMETIC_LIFECYCLE_OWNER_H_

#include <cstdint>
#include <optional>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "fireball/browser/domain_model.h"
#include "fireball/chromium/fireball_cosmetic_document_host.h"

namespace content {
class Page;
class WebContents;
} // namespace content

namespace fireball::chromium {

class FireballCosmeticLifecycleDelegate {
public:
  virtual ~FireballCosmeticLifecycleDelegate() = default;

  virtual bool
  CanActivateCosmeticDocument(const browser::DocumentId &document_id) = 0;

  // The host reference is valid only for this callback. The async controller
  // bridge retrieves it again from the current document before every operation
  // rather than retaining a raw pointer.
  virtual void OnCosmeticDocumentReady(const browser::DocumentId &document_id,
                                       FireballCosmeticDocumentHost &host) = 0;
  // These callbacks must synchronously cancel controller work for this exact
  // DocumentId and complete its callers. They may destroy the owner.
  virtual void
  OnCosmeticDocumentSuspended(const browser::DocumentId &document_id) = 0;
  virtual void
  OnCosmeticDocumentDisposed(const browser::DocumentId &document_id) = 0;
  virtual void OnCosmeticDocumentFailed(const browser::DocumentId &document_id,
                                        std::string_view error_code) = 0;
};

// Compile-gated WebContents lifecycle owner. PrimaryPageChanged chooses the
// active document, DocumentUserData preserves identity through BFCache, and
// RenderFrameHostStateChanged suspends the remote before stale callbacks can
// publish state. The delegate must outlive this owner. Construction from the
// Chrome tab lifecycle is a later gate.
class FireballCosmeticLifecycleOwner final
    : public content::WebContentsObserver {
public:
  FireballCosmeticLifecycleOwner(content::WebContents &web_contents,
                                 FireballCosmeticLifecycleDelegate &delegate);
  ~FireballCosmeticLifecycleOwner() override;

  FireballCosmeticLifecycleOwner(const FireballCosmeticLifecycleOwner &) =
      delete;
  FireballCosmeticLifecycleOwner &
  operator=(const FireballCosmeticLifecycleOwner &) = delete;

  void Start();
  void Shutdown();
  content::WeakDocumentPtr
  GetActiveDocument(const browser::DocumentId &document_id) const;
  const std::optional<browser::DocumentId> &active_document_id() const {
    return active_document_id_;
  }

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page &page) override;
  void RenderFrameHostStateChanged(
      content::RenderFrameHost *render_frame_host,
      content::RenderFrameHost::LifecycleState old_state,
      content::RenderFrameHost::LifecycleState new_state) override;
  void
  PrimaryMainFrameRenderProcessGone(base::TerminationStatus status) override;
  void RenderFrameDeleted(content::RenderFrameHost *render_frame_host) override;

private:
  void ActivatePrimaryDocument(content::RenderFrameHost &frame);
  std::uint64_t AdvanceLifecycleGeneration();
  void OnDocumentActivated(std::uint64_t lifecycle_generation,
                           content::WeakDocumentPtr expected_document,
                           browser::DocumentId document_id,
                           CosmeticTransportResult result);

  raw_ptr<FireballCosmeticLifecycleDelegate> delegate_;
  std::optional<browser::DocumentId> active_document_id_;
  content::WeakDocumentPtr active_document_;
  std::uint64_t lifecycle_generation_ = 0;
  base::WeakPtrFactory<FireballCosmeticLifecycleOwner> weak_factory_{this};
};

} // namespace fireball::chromium

#endif // FIREBALL_CHROMIUM_FIREBALL_COSMETIC_LIFECYCLE_OWNER_H_
