#ifndef FIREBALL_COMPONENTS_ADBLOCK_COSMETIC_EVALUATOR_H_
#define FIREBALL_COMPONENTS_ADBLOCK_COSMETIC_EVALUATOR_H_

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "fireball/components/adblock/include/fireball_adblock_ffi.h"

namespace fireball::adblock {

enum class CosmeticEvaluationStatus {
  kOk,
  kInvalidInput,
  kInternalError,
};

struct CosmeticEvaluation {
  CosmeticEvaluationStatus status = CosmeticEvaluationStatus::kInternalError;
  std::vector<std::string> hide_selectors;
  std::vector<std::string> exceptions;
  std::size_t procedural_action_count = 0;
  bool has_injected_script = false;
  bool generic_hiding_disabled = false;
};

struct GenericSelectorEvaluation {
  CosmeticEvaluationStatus status = CosmeticEvaluationStatus::kInternalError;
  std::vector<std::string> selectors;
};

class CosmeticEvaluator {
 public:
  virtual ~CosmeticEvaluator() = default;

  virtual CosmeticEvaluation EvaluatePage(std::string_view url) = 0;
  virtual GenericSelectorEvaluation EvaluateGenericSelectors(
      const std::vector<std::string>& classes,
      const std::vector<std::string>& ids,
      const std::vector<std::string>& exceptions) = 0;
};

// Borrowed adapter around one single-sequence adblock-rust engine. The owner
// must keep the engine alive and call this object only from its bound sequence.
class FfiCosmeticEvaluator final : public CosmeticEvaluator {
 public:
  explicit FfiCosmeticEvaluator(const FireballAdblockEngine* engine);

  CosmeticEvaluation EvaluatePage(std::string_view url) override;
  GenericSelectorEvaluation EvaluateGenericSelectors(
      const std::vector<std::string>& classes,
      const std::vector<std::string>& ids,
      const std::vector<std::string>& exceptions) override;

 private:
  const FireballAdblockEngine* engine_;
};

namespace internal {

// Strict parser hooks are public only to the standalone policy tests. Product
// code should use FfiCosmeticEvaluator so every Rust allocation is consumed.
std::optional<CosmeticEvaluation> ParseCosmeticEvaluationJson(
    std::string_view json);
std::optional<std::vector<std::string>> ParseSelectorArrayJson(
    std::string_view json);

}  // namespace internal
}  // namespace fireball::adblock

#endif  // FIREBALL_COMPONENTS_ADBLOCK_COSMETIC_EVALUATOR_H_
