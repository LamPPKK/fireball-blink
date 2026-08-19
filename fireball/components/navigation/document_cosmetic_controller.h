#ifndef FIREBALL_COMPONENTS_NAVIGATION_DOCUMENT_COSMETIC_CONTROLLER_H_
#define FIREBALL_COMPONENTS_NAVIGATION_DOCUMENT_COSMETIC_CONTROLLER_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "fireball/browser/domain_model.h"
#include "fireball/components/navigation/document_cosmetic_policy.h"

namespace fireball::navigation {

enum class CosmeticStyleLayer {
  kDocument,
  kGeneric,
};

// Chromium implements this with a browser-owned document stylesheet seam.
// SetStylesheet must synchronously copy |stylesheet| and be atomic: false must
// leave the layer unchanged. An empty value removes only that layer. No
// implementation may concatenate this value into HTML.
class CosmeticStyleSink {
 public:
  virtual ~CosmeticStyleSink() = default;

  virtual bool SetStylesheet(const browser::DocumentId& document_id,
                             CosmeticStyleLayer layer,
                             std::string_view stylesheet) = 0;
  virtual bool RemoveDocumentStyles(const browser::DocumentId& document_id) = 0;
};

enum class CosmeticControllerStatus {
  kApplied,
  kDisabled,
  kError,
};

// Safe for direct adapter/UI consumption: it contains only counts, flags and a
// stable error code, never URLs, hostnames, selectors or DOM tokens.
struct CosmeticControllerResult {
  CosmeticControllerStatus status = CosmeticControllerStatus::kError;
  std::size_t hidden_selector_count = 0;
  std::size_t skipped_procedural_action_count = 0;
  bool skipped_scriptlets = false;
  std::string error_code;
};

struct CosmeticRevocationResult {
  std::size_t revoked_documents = 0;
  std::size_t sink_failures = 0;

  bool complete() const { return sink_failures == 0; }
};

// Single-sequence lifecycle owner for document cosmetic plans. It rejects
// stale DOM revisions, binds one live DocumentId to a Tab and revokes styles
// across navigation, tab teardown and Profile policy changes.
class DocumentCosmeticController final {
 public:
  DocumentCosmeticController(const browser::BrowserModel* browser_model,
                             const DocumentCosmeticPolicy* policy,
                             CosmeticStyleSink* style_sink);

  CosmeticControllerResult CommitDocument(const browser::ProfileId& profile_id,
                                          const browser::TabId& tab_id,
                                          browser::DocumentId document_id,
                                          std::string_view url,
                                          std::string_view hostname);

  CosmeticControllerResult ApplyDomSnapshot(
      const browser::DocumentId& document_id, std::uint64_t revision,
      const std::vector<std::string>& classes,
      const std::vector<std::string>& ids);

  bool RevokeDocument(const browser::DocumentId& document_id);
  CosmeticRevocationResult RevokeTab(const browser::TabId& tab_id);
  CosmeticRevocationResult RevokeProfile(const browser::ProfileId& profile_id);
  CosmeticRevocationResult RetryPendingRevocations();

  std::size_t active_document_count() const { return documents_.size(); }
  std::size_t pending_revocation_count() const {
    return pending_revocations_.size();
  }

 private:
  struct DocumentState {
    browser::ProfileId profile_id;
    browser::TabId tab_id;
    std::string hostname;
    DocumentCosmeticPlan plan;
    std::uint64_t last_dom_revision = 0;
  };

  struct PendingRevocation {
    browser::ProfileId profile_id;
    browser::TabId tab_id;
  };

  bool ProfileOwnsTab(const browser::ProfileId& profile_id,
                      const browser::TabId& tab_id) const;
  bool EraseDocument(const browser::DocumentId& document_id);

  const browser::BrowserModel* browser_model_;
  const DocumentCosmeticPolicy* policy_;
  CosmeticStyleSink* style_sink_;
  std::map<browser::DocumentId, DocumentState> documents_;
  std::map<browser::TabId, browser::DocumentId> tab_documents_;
  std::map<browser::DocumentId, PendingRevocation> pending_revocations_;
};

}  // namespace fireball::navigation

#endif  // FIREBALL_COMPONENTS_NAVIGATION_DOCUMENT_COSMETIC_CONTROLLER_H_
