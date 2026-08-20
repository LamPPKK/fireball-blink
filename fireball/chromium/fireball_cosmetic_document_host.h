#ifndef FIREBALL_CHROMIUM_FIREBALL_COSMETIC_DOCUMENT_HOST_H_
#define FIREBALL_CHROMIUM_FIREBALL_COSMETIC_DOCUMENT_HOST_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/weak_document_ptr.h"
#include "fireball/browser/domain_model.h"
#include "fireball/chromium/browser_cosmetic_document_state.h"
#include "fireball/chromium/fireball_cosmetic_style_transport.h"
#include "fireball/components/navigation/document_cosmetic_controller.h"

namespace content {
class RenderFrameHost;
}

namespace fireball::chromium {

// DocumentUserData owner for one Blink document. It survives BFCache with the
// same DocumentId, but drops the browser Mojo remote while inactive. On restore
// it performs a fresh epoch handshake before accepting another mutation.
class FireballCosmeticDocumentHost final
    : public content::DocumentUserData<FireballCosmeticDocumentHost> {
public:
  using CompletionCallback = FireballCosmeticStyleTransport::CompletionCallback;

  FireballCosmeticDocumentHost(const FireballCosmeticDocumentHost &) = delete;
  FireballCosmeticDocumentHost &
  operator=(const FireballCosmeticDocumentHost &) = delete;
  ~FireballCosmeticDocumentHost() override;

  void Activate(CompletionCallback callback);
  void Suspend();
  void ResetForController();
  void SetStylesheet(navigation::CosmeticStyleLayer layer,
                     std::string stylesheet, CompletionCallback callback);
  void Revoke(CompletionCallback callback);

  bool ready() const;
  const browser::DocumentId &document_id() const { return document_id_; }
  content::WeakDocumentPtr GetWeakDocumentPtr();
  BrowserCosmeticDocumentPhase phase() const { return state_.phase(); }

private:
  friend content::DocumentUserData<FireballCosmeticDocumentHost>;

  FireballCosmeticDocumentHost(content::RenderFrameHost *render_frame_host,
                               browser::DocumentId document_id);

  void OnActivated(BrowserCosmeticDocumentTicket ticket,
                   CompletionCallback callback, CosmeticTransportResult result);
  void RestoreDesiredStyles(BrowserCosmeticDocumentTicket ticket,
                            CompletionCallback callback);
  void RestoreDesiredGenericStyle(BrowserCosmeticDocumentTicket ticket,
                                  CompletionCallback callback);
  void OnDesiredDocumentStyleRestored(BrowserCosmeticDocumentTicket ticket,
                                      CompletionCallback callback,
                                      CosmeticTransportResult result);
  void OnDesiredGenericStyleRestored(BrowserCosmeticDocumentTicket ticket,
                                     CompletionCallback callback,
                                     CosmeticTransportResult result);
  void FinishRestore(BrowserCosmeticDocumentTicket ticket,
                     CompletionCallback callback, bool accepted,
                     std::string_view error_code = {});
  void OnStylesheetApplied(navigation::CosmeticStyleLayer layer,
                           std::string stylesheet, CompletionCallback callback,
                           CosmeticTransportResult result);
  void OnRevoked(BrowserCosmeticDocumentTicket ticket,
                 CompletionCallback callback, CosmeticTransportResult result);
  bool IsActivePrimaryDocument() const;

  browser::DocumentId document_id_;
  BrowserCosmeticDocumentState state_;
  std::unique_ptr<FireballCosmeticStyleTransport> transport_;
  std::string desired_document_stylesheet_;
  std::string desired_generic_stylesheet_;
  base::WeakPtrFactory<FireballCosmeticDocumentHost> weak_factory_{this};

  DOCUMENT_USER_DATA_KEY_DECL();
};

} // namespace fireball::chromium

#endif // FIREBALL_CHROMIUM_FIREBALL_COSMETIC_DOCUMENT_HOST_H_
