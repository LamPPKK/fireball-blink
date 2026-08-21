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
  binding_generation_ = 0;
  dom_snapshot_revision_ = 0;
  binding_generation_exhausted_ = false;
  dom_snapshot_revision_exhausted_ = false;
  binding_active_ = false;
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

void RendererCosmeticStyleState::SuspendBinding() {
  document_key_.clear();
  generic_key_.clear();
  binding_active_ = false;
  ++state_revision_;
}

bool RendererCosmeticStyleState::BindDocument(
    browser::DocumentId document_id,
    std::uint64_t expected_document_epoch) {
  if (expected_document_epoch == 0 ||
      expected_document_epoch != document_epoch()) {
    return false;
  }
  if (binding_generation_exhausted_ ||
      binding_generation_ == std::numeric_limits<std::uint64_t>::max()) {
    binding_generation_exhausted_ = true;
    binding_generation_ = 0;
    return false;
  }
  if (document_id_.has_value()) {
    if (*document_id_ != document_id) {
      return false;
    }
  } else {
    document_id_ = std::move(document_id);
  }
  binding_active_ = true;
  ++state_revision_;
  ++binding_generation_;
  return true;
}

std::optional<std::uint64_t>
RendererCosmeticStyleState::NextDomSnapshotRevision(
    const browser::DocumentId& document_id,
    std::uint64_t expected_document_epoch,
    std::uint64_t expected_binding_generation) {
  if (expected_document_epoch == 0 ||
      expected_document_epoch != document_epoch() ||
      expected_binding_generation == 0 ||
      expected_binding_generation != binding_generation() ||
      !binding_active_ || !document_id_.has_value() ||
      *document_id_ != document_id || dom_snapshot_revision_exhausted_ ||
      dom_snapshot_revision_ == std::numeric_limits<std::uint64_t>::max()) {
    if (dom_snapshot_revision_ == std::numeric_limits<std::uint64_t>::max()) {
      dom_snapshot_revision_ = 0;
      dom_snapshot_revision_exhausted_ = true;
    }
    return std::nullopt;
  }
  ++dom_snapshot_revision_;
  return dom_snapshot_revision_;
}

RendererStyleMutation RendererCosmeticStyleState::PrepareMutation(
    const browser::DocumentId& document_id,
    std::uint64_t expected_document_epoch,
    std::uint64_t expected_binding_generation,
    navigation::CosmeticStyleLayer layer,
    std::string_view stylesheet) {
  if (expected_document_epoch == 0 ||
      expected_document_epoch != document_epoch()) {
    return Reject("COSMETIC_RENDERER_EPOCH_MISMATCH");
  }
  if (expected_binding_generation == 0 ||
      expected_binding_generation != binding_generation()) {
    return Reject("COSMETIC_RENDERER_BINDING_MISMATCH");
  }
  if (!binding_active_ || !document_id_.has_value() ||
      *document_id_ != document_id) {
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

std::uint64_t RendererCosmeticStyleState::binding_generation() const {
  return binding_generation_exhausted_ ? 0 : binding_generation_;
}

bool RendererCosmeticStyleState::CommitMutation(
    const RendererStyleMutation& mutation) {
  if (!binding_active_ || !document_id_.has_value() ||
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
