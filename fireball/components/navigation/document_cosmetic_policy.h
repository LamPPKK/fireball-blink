#ifndef FIREBALL_COMPONENTS_NAVIGATION_DOCUMENT_COSMETIC_POLICY_H_
#define FIREBALL_COMPONENTS_NAVIGATION_DOCUMENT_COSMETIC_POLICY_H_

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "fireball/browser/domain_model.h"
#include "fireball/components/adblock/cosmetic_evaluator.h"
#include "fireball/components/adblock/profile_policy.h"

namespace fireball::navigation {

enum class DocumentCosmeticStatus {
  kReady,
  kDisabled,
  kError,
};

inline constexpr std::size_t kMaximumCosmeticStylesheetBytes = 512 * 1024;

// Revalidates the exact selector-only format at the renderer boundary. Empty
// is the explicit layer-removal value. No page markup or script is accepted.
bool IsValidCompiledCosmeticStylesheet(std::string_view stylesheet);

// Consumed directly by the Chromium renderer adapter. It is not a logging or
// metrics DTO and intentionally retains no page URL.
struct DocumentCosmeticPlan {
  DocumentCosmeticStatus status = DocumentCosmeticStatus::kError;
  std::string stylesheet;
  std::vector<std::string> generic_exceptions;
  bool generic_scan_allowed = false;
  std::size_t hidden_selector_count = 0;
  std::size_t skipped_procedural_action_count = 0;
  bool skipped_scriptlets = false;
  std::string profile_key;
  std::string hostname;
  std::string error_code;
};

struct GenericCosmeticPlan {
  DocumentCosmeticStatus status = DocumentCosmeticStatus::kError;
  std::string stylesheet;
  std::size_t hidden_selector_count = 0;
  std::string error_code;
};

// Produces validated, Profile-bound stylesheet plans. Procedural actions and
// engine-provided scriptlets are reported but never returned for execution.
class DocumentCosmeticPolicy final {
 public:
  DocumentCosmeticPolicy(const adblock::ProfilePolicy* profile_policy,
                         adblock::CosmeticEvaluator* evaluator);

  DocumentCosmeticPlan BeginDocument(const browser::ProfileId& profile_id,
                                     std::string_view url,
                                     std::string_view hostname) const;

  GenericCosmeticPlan MatchGenericSelectors(
      const browser::ProfileId& profile_id,
      std::string_view hostname,
      const DocumentCosmeticPlan& document,
      const std::vector<std::string>& classes,
      const std::vector<std::string>& ids) const;

 private:
  const adblock::ProfilePolicy* profile_policy_;
  adblock::CosmeticEvaluator* evaluator_;
};

}  // namespace fireball::navigation

#endif  // FIREBALL_COMPONENTS_NAVIGATION_DOCUMENT_COSMETIC_POLICY_H_
