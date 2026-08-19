#include "fireball/components/navigation/document_cosmetic_policy.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "fireball/components/navigation/url_cleaner.h"

namespace fireball::navigation {
namespace {

constexpr std::size_t kMaximumStylesheetBytes = 512 * 1024;
constexpr std::size_t kMaximumSelectors = 8192;
constexpr std::size_t kMaximumSelectorBytes = 4096;
constexpr std::string_view kHideDeclaration = "{display:none!important;}\n";

DocumentCosmeticPlan DocumentError(std::string_view code) {
  DocumentCosmeticPlan result;
  result.error_code = code;
  return result;
}

GenericCosmeticPlan GenericError(std::string_view code) {
  GenericCosmeticPlan result;
  result.error_code = code;
  return result;
}

bool IsUtf8Continuation(unsigned char value) {
  return (value & 0xc0U) == 0x80U;
}

bool IsValidUtf8(std::string_view value) {
  for (std::size_t index = 0; index < value.size();) {
    const unsigned char lead = static_cast<unsigned char>(value[index]);
    if (lead <= 0x7fU) {
      ++index;
      continue;
    }
    std::size_t width = 0;
    std::uint32_t codepoint = 0;
    std::uint32_t minimum = 0;
    if ((lead & 0xe0U) == 0xc0U) {
      width = 2;
      codepoint = lead & 0x1fU;
      minimum = 0x80U;
    } else if ((lead & 0xf0U) == 0xe0U) {
      width = 3;
      codepoint = lead & 0x0fU;
      minimum = 0x800U;
    } else if ((lead & 0xf8U) == 0xf0U) {
      width = 4;
      codepoint = lead & 0x07U;
      minimum = 0x10000U;
    } else {
      return false;
    }
    if (index + width > value.size()) {
      return false;
    }
    for (std::size_t offset = 1; offset < width; ++offset) {
      const unsigned char continuation =
          static_cast<unsigned char>(value[index + offset]);
      if (!IsUtf8Continuation(continuation)) {
        return false;
      }
      codepoint = (codepoint << 6U) | (continuation & 0x3fU);
    }
    if (codepoint < minimum || codepoint > 0x10ffffU ||
        (codepoint >= 0xd800U && codepoint <= 0xdfffU)) {
      return false;
    }
    index += width;
  }
  return true;
}

bool IsSafeSelector(std::string_view selector) {
  if (selector.empty() || selector.size() > kMaximumSelectorBytes ||
      !IsValidUtf8(selector) || selector.find("/*") != std::string_view::npos ||
      selector.find("*/") != std::string_view::npos) {
    return false;
  }
  return std::none_of(selector.begin(), selector.end(), [](char character) {
    const unsigned char byte = static_cast<unsigned char>(character);
    return byte < 0x20U || byte == 0x7fU || character == '{' ||
           character == '}' || character == '<' || character == '>' ||
           character == '@' || character == ';' || character == '`';
  });
}

std::optional<std::string> CompileStylesheet(
    const std::vector<std::string>& selectors) {
  if (selectors.size() > kMaximumSelectors) {
    return std::nullopt;
  }
  std::string stylesheet;
  for (const std::string& selector : selectors) {
    if (!IsSafeSelector(selector) ||
        selector.size() > kMaximumStylesheetBytes - stylesheet.size() ||
        kHideDeclaration.size() >
            kMaximumStylesheetBytes - stylesheet.size() - selector.size()) {
      return std::nullopt;
    }
    stylesheet.append(selector);
    stylesheet.append(kHideDeclaration);
  }
  return stylesheet;
}

}  // namespace

DocumentCosmeticPolicy::DocumentCosmeticPolicy(
    const adblock::ProfilePolicy* profile_policy,
    adblock::CosmeticEvaluator* evaluator)
    : profile_policy_(profile_policy), evaluator_(evaluator) {}

DocumentCosmeticPlan DocumentCosmeticPolicy::BeginDocument(
    const browser::ProfileId& profile_id, std::string_view url,
    std::string_view hostname) const {
  if (profile_policy_ == nullptr || !profile_policy_->HasProfile(profile_id) ||
      !adblock::IsCanonicalHostname(hostname) ||
      !IsSafeNavigationUrlForHostname(url, hostname)) {
    return DocumentError("COSMETIC_CONTEXT_INVALID");
  }
  if (!profile_policy_->ShouldEvaluate(profile_id, hostname)) {
    DocumentCosmeticPlan result;
    result.status = DocumentCosmeticStatus::kDisabled;
    return result;
  }
  if (evaluator_ == nullptr) {
    return DocumentError("COSMETIC_ENGINE_UNAVAILABLE");
  }
  adblock::CosmeticEvaluation evaluation = evaluator_->EvaluatePage(url);
  if (evaluation.status != adblock::CosmeticEvaluationStatus::kOk) {
    return DocumentError("COSMETIC_EVALUATION_FAILED");
  }
  auto stylesheet = CompileStylesheet(evaluation.hide_selectors);
  if (!stylesheet.has_value()) {
    return DocumentError("COSMETIC_STYLESHEET_INVALID");
  }

  DocumentCosmeticPlan result;
  result.status = DocumentCosmeticStatus::kReady;
  result.stylesheet = std::move(*stylesheet);
  result.generic_exceptions = std::move(evaluation.exceptions);
  result.generic_scan_allowed = !evaluation.generic_hiding_disabled;
  result.hidden_selector_count = evaluation.hide_selectors.size();
  result.skipped_procedural_action_count = evaluation.procedural_action_count;
  result.skipped_scriptlets = evaluation.has_injected_script;
  result.profile_key = profile_id.value();
  result.hostname = std::string(hostname);
  return result;
}

GenericCosmeticPlan DocumentCosmeticPolicy::MatchGenericSelectors(
    const browser::ProfileId& profile_id, std::string_view hostname,
    const DocumentCosmeticPlan& document,
    const std::vector<std::string>& classes,
    const std::vector<std::string>& ids) const {
  if (profile_policy_ == nullptr || !profile_policy_->HasProfile(profile_id) ||
      !adblock::IsCanonicalHostname(hostname) ||
      document.status != DocumentCosmeticStatus::kReady ||
      document.profile_key != profile_id.value() ||
      document.hostname != hostname) {
    return GenericError("COSMETIC_DOCUMENT_MISMATCH");
  }
  if (!profile_policy_->ShouldEvaluate(profile_id, hostname)) {
    GenericCosmeticPlan result;
    result.status = DocumentCosmeticStatus::kDisabled;
    return result;
  }
  if (!document.generic_scan_allowed) {
    GenericCosmeticPlan result;
    result.status = DocumentCosmeticStatus::kDisabled;
    return result;
  }
  if (evaluator_ == nullptr) {
    return GenericError("COSMETIC_ENGINE_UNAVAILABLE");
  }
  adblock::GenericSelectorEvaluation evaluation =
      evaluator_->EvaluateGenericSelectors(classes, ids,
                                           document.generic_exceptions);
  if (evaluation.status != adblock::CosmeticEvaluationStatus::kOk) {
    return GenericError("COSMETIC_GENERIC_EVALUATION_FAILED");
  }
  auto stylesheet = CompileStylesheet(evaluation.selectors);
  if (!stylesheet.has_value()) {
    return GenericError("COSMETIC_STYLESHEET_INVALID");
  }
  GenericCosmeticPlan result;
  result.status = DocumentCosmeticStatus::kReady;
  result.stylesheet = std::move(*stylesheet);
  result.hidden_selector_count = evaluation.selectors.size();
  return result;
}

}  // namespace fireball::navigation
