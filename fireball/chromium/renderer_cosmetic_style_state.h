#ifndef FIREBALL_CHROMIUM_RENDERER_COSMETIC_STYLE_STATE_H_
#define FIREBALL_CHROMIUM_RENDERER_COSMETIC_STYLE_STATE_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "fireball/browser/domain_model.h"
#include "fireball/components/navigation/document_cosmetic_controller.h"

namespace fireball::chromium {

enum class RendererStyleMutationAction {
  kReject,
  kNoop,
  kInstall,
  kRemove,
};

// A selector-free mutation DTO. Stylesheets stay in the typed Mojo call and
// are never retained in renderer state or diagnostic results.
struct RendererStyleMutation {
  RendererStyleMutationAction action = RendererStyleMutationAction::kReject;
  navigation::CosmeticStyleLayer layer =
      navigation::CosmeticStyleLayer::kDocument;
  std::uint64_t state_revision = 0;
  std::string previous_key;
  std::string new_key;
  std::string error_code;
};

// Same-sequence renderer state. A new Blink document must reset this state and
// be explicitly rebound to one Fireball DocumentId before any CSS is accepted.
class RendererCosmeticStyleState final {
 public:
  bool BeginDocument();
  // Drops the live receiver binding and stylesheet keys while retaining the
  // first DocumentId claimed for this Blink document.
  void SuspendBinding();
  bool BindDocument(browser::DocumentId document_id,
                    std::uint64_t expected_document_epoch);

  RendererStyleMutation PrepareMutation(
      const browser::DocumentId& document_id,
      std::uint64_t expected_document_epoch,
      std::uint64_t expected_binding_generation,
      navigation::CosmeticStyleLayer layer,
      std::string_view stylesheet);
  bool CommitMutation(const RendererStyleMutation& mutation);

  bool HasBoundDocument() const {
    return binding_active_ && document_id_.has_value();
  }
  std::uint64_t document_epoch() const;
  std::uint64_t binding_generation() const;
  const std::string& CurrentKey(navigation::CosmeticStyleLayer layer) const;

 private:
  std::string& MutableKey(navigation::CosmeticStyleLayer layer);

  std::optional<browser::DocumentId> document_id_;
  std::string document_key_;
  std::string generic_key_;
  std::uint64_t state_revision_ = 0;
  std::uint64_t key_sequence_ = 0;
  std::uint64_t document_epoch_ = 0;
  std::uint64_t binding_generation_ = 0;
  bool document_epoch_exhausted_ = false;
  bool binding_generation_exhausted_ = false;
  bool binding_active_ = false;
};

}  // namespace fireball::chromium

#endif  // FIREBALL_CHROMIUM_RENDERER_COSMETIC_STYLE_STATE_H_
