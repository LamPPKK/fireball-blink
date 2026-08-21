#ifndef FIREBALL_CHROMIUM_FIREBALL_COSMETIC_CONTROLLER_BRIDGE_H_
#define FIREBALL_CHROMIUM_FIREBALL_COSMETIC_CONTROLLER_BRIDGE_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/weak_document_ptr.h"
#include "fireball/browser/domain_model.h"
#include "fireball/chromium/browser_cosmetic_controller_state.h"
#include "fireball/chromium/fireball_cosmetic_lifecycle_owner.h"
#include "fireball/chromium/tab_web_contents_binding.h"
#include "fireball/components/navigation/document_cosmetic_controller.h"
#include "fireball/components/navigation/document_cosmetic_policy.h"

namespace content {
class WebContents;
}

namespace fireball::chromium {

class FireballCosmeticControllerClient {
public:
  virtual ~FireballCosmeticControllerClient() = default;

  // Results contain counts and stable codes only. These callbacks may destroy
  // the bridge, so the bridge never touches member state after notifying.
  virtual void OnCosmeticControllerResult(
      const browser::DocumentId &document_id,
      const navigation::CosmeticControllerResult &result) = 0;
  virtual void
  OnCosmeticControllerSuspended(const browser::DocumentId &document_id) = 0;
};

// Compile-gated, per-WebContents asynchronous bridge. It is the only owner of
// controller commit timing: policy state advances only after the exact active
// DocumentUserData host acknowledges the renderer mutation.
// The BrowserModel, policy, and client supplied to Create() must outlive the
// bridge.
class FireballCosmeticControllerBridge final
    : public FireballCosmeticLifecycleDelegate {
public:
  static std::unique_ptr<FireballCosmeticControllerBridge>
  Create(content::WebContents &web_contents,
         const browser::BrowserModel &browser_model,
         const navigation::DocumentCosmeticPolicy &policy,
         browser::ProfileId profile_id, browser::TabId tab_id,
         FireballCosmeticControllerClient &client);

  FireballCosmeticControllerBridge(const FireballCosmeticControllerBridge &) =
      delete;
  FireballCosmeticControllerBridge &
  operator=(const FireballCosmeticControllerBridge &) = delete;
  ~FireballCosmeticControllerBridge() override;

  bool RefreshDomSnapshot();
  bool RevokeActiveDocument();
  bool RefreshActiveDocument();

  std::size_t tracked_document_count() const { return documents_.size(); }
  BrowserCosmeticControllerPhase phase() const { return state_.phase(); }

  // FireballCosmeticLifecycleDelegate:
  bool
  CanActivateCosmeticDocument(const browser::DocumentId &document_id) override;
  void OnCosmeticDocumentReady(const browser::DocumentId &document_id,
                               FireballCosmeticDocumentHost &host) override;
  void
  OnCosmeticDocumentSuspended(const browser::DocumentId &document_id) override;
  void
  OnCosmeticDocumentDisposed(const browser::DocumentId &document_id) override;
  void OnCosmeticDocumentFailed(const browser::DocumentId &document_id,
                                std::string_view error_code) override;

private:
  struct TrackedDocument {
    content::WeakDocumentPtr document;
    navigation::DocumentCosmeticPlan plan;
    navigation::CosmeticControllerResult last_result;
    std::uint64_t last_dom_revision = 0;
  };

  FireballCosmeticControllerBridge(
      const browser::BrowserModel &browser_model,
      const navigation::DocumentCosmeticPolicy &policy,
      browser::ProfileId profile_id, browser::TabId tab_id,
      FireballCosmeticControllerClient &client,
      base::WeakPtr<TabWebContentsBinding> tab_binding,
      std::uint64_t tab_claim);

  bool Initialize(content::WebContents &web_contents);
  bool ProfileOwnsTab() const;
  bool ContextValid() const;
  void ScheduleDomCollection(const browser::DocumentId &document_id);
  bool StartDomCollection(const browser::DocumentId &document_id);
  bool CancelDomCollection(const browser::DocumentId &document_id,
                           FireballCosmeticDocumentHost &host,
                           const content::WeakDocumentPtr &document);
  void OnDomSnapshotCollected(BrowserCosmeticControllerTicket ticket,
                              content::WeakDocumentPtr document,
                              CosmeticTransportResult transport_result,
                              CosmeticDomSnapshot snapshot);
  void ClearGenericStylesheetAfterLimit(
      const browser::DocumentId &document_id,
      const content::WeakDocumentPtr &document,
      std::uint64_t revision,
      std::string error_code);
  void OnGenericStylesheetClearedAfterLimit(
      BrowserCosmeticControllerTicket ticket,
      content::WeakDocumentPtr document,
      std::string error_code,
      CosmeticTransportResult transport_result);
  bool ApplyDomSnapshot(const browser::DocumentId &document_id,
                        CosmeticDomSnapshot snapshot);
  void ResetDocument(const browser::DocumentId &document_id,
                     const content::WeakDocumentPtr &document);
  void ResetAllDocuments();
  FireballCosmeticDocumentHost *
  ResolveHost(const browser::DocumentId &document_id,
              const content::WeakDocumentPtr &document) const;
  void PrepareReadyDocument(BrowserCosmeticControllerTicket ticket,
                            FireballCosmeticDocumentHost &host);
  void OnDocumentStylesheetApplied(BrowserCosmeticControllerTicket ticket,
                                   content::WeakDocumentPtr document,
                                   navigation::DocumentCosmeticPlan plan,
                                   CosmeticTransportResult transport_result);
  void OnGenericStylesheetApplied(BrowserCosmeticControllerTicket ticket,
                                  content::WeakDocumentPtr document,
                                  navigation::CosmeticControllerResult result,
                                  CosmeticTransportResult transport_result);
  void OnDocumentRevoked(BrowserCosmeticControllerTicket ticket,
                         content::WeakDocumentPtr document,
                         CosmeticTransportResult transport_result);
  void OnDocumentRevokedForRefresh(BrowserCosmeticControllerTicket ticket,
                                   content::WeakDocumentPtr document,
                                   CosmeticTransportResult transport_result);
  void OnHostReactivatedForRefresh(BrowserCosmeticControllerTicket ticket,
                                   content::WeakDocumentPtr document,
                                   CosmeticTransportResult transport_result);
  void Publish(const browser::DocumentId &document_id,
               const navigation::CosmeticControllerResult &result);

  raw_ptr<const browser::BrowserModel> browser_model_;
  raw_ptr<const navigation::DocumentCosmeticPolicy> policy_;
  raw_ptr<FireballCosmeticControllerClient> client_;
  base::WeakPtr<TabWebContentsBinding> tab_binding_;
  std::uint64_t tab_claim_ = 0;
  browser::ProfileId profile_id_;
  browser::TabId tab_id_;
  BrowserCosmeticControllerState state_;
  std::map<browser::DocumentId, TrackedDocument> documents_;
  std::unique_ptr<FireballCosmeticLifecycleOwner> lifecycle_owner_;
  base::WeakPtrFactory<FireballCosmeticControllerBridge> weak_factory_{this};
};

} // namespace fireball::chromium

#endif // FIREBALL_CHROMIUM_FIREBALL_COSMETIC_CONTROLLER_BRIDGE_H_
