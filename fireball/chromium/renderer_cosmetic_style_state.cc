#include "fireball/chromium/renderer_cosmetic_style_state.h"

#include <limits>
#include <string>
#include <utility>

#include "fireball/components/navigation/document_cosmetic_policy.h"

namespace fireball::chromium {
namespace {

RendererStyleMutation Reject(std::string_view error_code) {
  RendererStyleMutation result;
  result.error_code = std::string(error_code);
  return result;
}

std::string LayerName(navigation::CosmeticStyleLayer layer) {
  return layer == navigation::CosmeticStyleLayer::kDocument ? "document"
                                                            : "generic";
}

}  // namespace

bool RendererCosmeticStyleState::BeginDocument() {
  document_id_.reset();
  document_key_.clear();
  generic_key_.clear();
  ++state_revision_;
  if (document_epoch_exhausted_ ||
      document_epoch_ == std::numeric_limits<std::uint64_t>::max()) {
    document_epoch_ = 0;
    document_epoch_exhausted_ = true;
    return false;
  }
  ++document_epoch_;
  return true;
}

void RendererCosmeticStyleState::UnbindDocument() {
  document_id_.reset();
  document_key_.clear();
  generic_key_.clear();
  ++state_revision_;
}

bool RendererCosmeticStyleState::BindDocument(
    browser::DocumentId document_id,
    std::uint64_t expected_document_epoch) {
  if (document_id_.has_value() || expected_document_epoch == 0 ||
      expected_document_epoch != document_epoch()) {
    return false;
  }
  document_id_ = std::move(document_id);
  ++state_revision_;
  return true;
}

RendererStyleMutation RendererCosmeticStyleState::PrepareMutation(
    const browser::DocumentId& document_id,
    std::uint64_t expected_document_epoch,
    navigation::CosmeticStyleLayer layer,
    std::string_view stylesheet) {
  if (expected_document_epoch == 0 ||
      expected_document_epoch != document_epoch()) {
    return Reject("COSMETIC_RENDERER_EPOCH_MISMATCH");
  }
  if (!document_id_.has_value() || *document_id_ != document_id) {
    return Reject("COSMETIC_RENDERER_DOCUMENT_MISMATCH");
  }
  if (!navigation::IsValidCompiledCosmeticStylesheet(stylesheet)) {
    return Reject("COSMETIC_RENDERER_STYLESHEET_INVALID");
  }

  RendererStyleMutation mutation;
  mutation.layer = layer;
  mutation.state_revision = state_revision_;
  mutation.previous_key = CurrentKey(layer);
  if (stylesheet.empty()) {
    mutation.action = mutation.previous_key.empty()
                          ? RendererStyleMutationAction::kNoop
                          : RendererStyleMutationAction::kRemove;
    return mutation;
  }
  if (key_sequence_ == std::numeric_limits<std::uint64_t>::max()) {
    return Reject("COSMETIC_RENDERER_KEY_EXHAUSTED");
  }
  ++key_sequence_;
  mutation.action = RendererStyleMutationAction::kInstall;
  mutation.new_key = "fireball-cosmetic-" + LayerName(layer) + "-" +
                     std::to_string(key_sequence_);
  return mutation;
}

std::uint64_t RendererCosmeticStyleState::document_epoch() const {
  return document_epoch_exhausted_ ? 0 : document_epoch_;
}

bool RendererCosmeticStyleState::CommitMutation(
    const RendererStyleMutation& mutation) {
  if (!document_id_.has_value() ||
      mutation.action == RendererStyleMutationAction::kReject ||
      mutation.state_revision != state_revision_ ||
      mutation.previous_key != CurrentKey(mutation.layer)) {
    return false;
  }
  if (mutation.action == RendererStyleMutationAction::kNoop) {
    return true;
  }
  std::string& key = MutableKey(mutation.layer);
  if (mutation.action == RendererStyleMutationAction::kRemove) {
    if (key.empty() || !mutation.new_key.empty()) {
      return false;
    }
    key.clear();
  } else if (mutation.action == RendererStyleMutationAction::kInstall) {
    if (mutation.new_key.empty()) {
      return false;
    }
    key = mutation.new_key;
  } else {
    return false;
  }
  ++state_revision_;
  return true;
}

const std::string& RendererCosmeticStyleState::CurrentKey(
    navigation::CosmeticStyleLayer layer) const {
  return layer == navigation::CosmeticStyleLayer::kDocument ? document_key_
                                                            : generic_key_;
}

std::string& RendererCosmeticStyleState::MutableKey(
    navigation::CosmeticStyleLayer layer) {
  return layer == navigation::CosmeticStyleLayer::kDocument ? document_key_
                                                            : generic_key_;
}

}  // namespace fireball::chromium
