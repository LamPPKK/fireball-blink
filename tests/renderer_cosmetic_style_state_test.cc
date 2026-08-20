#include "fireball/chromium/renderer_cosmetic_style_state.h"

#include <cassert>
#include <cstdint>
#include <string>
#include <utility>

#include "fireball/components/navigation/document_cosmetic_policy.h"

namespace {

template <typename Id>
Id Parse(const char* value) {
  auto parsed = Id::Parse(std::string(value));
  assert(parsed.has_value());
  return std::move(*parsed);
}

}  // namespace

int main() {
  using fireball::browser::DocumentId;
  using fireball::chromium::RendererCosmeticStyleState;
  using fireball::chromium::RendererStyleMutationAction;
  using fireball::navigation::CosmeticStyleLayer;

  assert(fireball::navigation::IsValidCompiledCosmeticStylesheet(""));
  assert(fireball::navigation::IsValidCompiledCosmeticStylesheet(
      ".advert{display:none!important;}\n"
      "#sponsor{display:none!important;}\n"));
  assert(!fireball::navigation::IsValidCompiledCosmeticStylesheet(
      "body{color:red;}\n"));
  assert(!fireball::navigation::IsValidCompiledCosmeticStylesheet(
      ".advert{display:none!important;}"));
  assert(!fireball::navigation::IsValidCompiledCosmeticStylesheet(
      ".advert<script>{display:none!important;}\n"));
  assert(!fireball::navigation::IsValidCompiledCosmeticStylesheet(
      std::string(".bad") + static_cast<char>(0xff) +
      "{display:none!important;}\n"));
  assert(!fireball::navigation::IsValidCompiledCosmeticStylesheet(std::string(
      fireball::navigation::kMaximumCosmeticStylesheetBytes + 1, 'a')));

  const DocumentId first =
      Parse<DocumentId>("40000000-0000-4000-8000-000000000001");
  const DocumentId second =
      Parse<DocumentId>("40000000-0000-4000-8000-000000000002");
  RendererCosmeticStyleState state;

  assert(state.document_epoch() == 0);
  auto mutation =
      state.PrepareMutation(first, 1, 1, CosmeticStyleLayer::kDocument,
                            ".advert{display:none!important;}\n");
  assert(mutation.action == RendererStyleMutationAction::kReject);
  assert(mutation.error_code == "COSMETIC_RENDERER_EPOCH_MISMATCH");

  assert(state.BeginDocument());
  const std::uint64_t first_epoch = state.document_epoch();
  assert(first_epoch == 1);
  assert(!state.BindDocument(first, first_epoch + 1));
  assert(state.BindDocument(first, first_epoch));
  const std::uint64_t first_binding = state.binding_generation();
  assert(first_binding == 1);
  assert(state.BindDocument(first, first_epoch));
  const std::uint64_t restored_binding = state.binding_generation();
  assert(restored_binding == first_binding + 1);
  assert(!state.BindDocument(second, first_epoch));
  mutation = state.PrepareMutation(first, first_epoch, restored_binding,
                                   CosmeticStyleLayer::kDocument,
                                   "body{color:red;}\n");
  assert(mutation.action == RendererStyleMutationAction::kReject);
  assert(mutation.error_code == "COSMETIC_RENDERER_STYLESHEET_INVALID");

  mutation = state.PrepareMutation(first, first_epoch, restored_binding,
                                   CosmeticStyleLayer::kDocument,
                                   ".advert{display:none!important;}\n");
  assert(mutation.action == RendererStyleMutationAction::kInstall);
  assert(mutation.previous_key.empty());
  assert(mutation.new_key == "fireball-cosmetic-document-1");
  const auto uncommitted = mutation;
  assert(state.CurrentKey(CosmeticStyleLayer::kDocument).empty());
  assert(state.CommitMutation(mutation));
  assert(state.CurrentKey(CosmeticStyleLayer::kDocument) ==
         "fireball-cosmetic-document-1");
  assert(!state.CommitMutation(uncommitted));

  mutation = state.PrepareMutation(first, first_epoch, first_binding,
                                   CosmeticStyleLayer::kDocument,
                                   ".late-binding{display:none!important;}\n");
  assert(mutation.action == RendererStyleMutationAction::kReject);
  assert(mutation.error_code == "COSMETIC_RENDERER_BINDING_MISMATCH");

  mutation = state.PrepareMutation(first, first_epoch, restored_binding,
                                   CosmeticStyleLayer::kDocument,
                                   ".sponsor{display:none!important;}\n");
  assert(mutation.action == RendererStyleMutationAction::kInstall);
  assert(mutation.previous_key == "fireball-cosmetic-document-1");
  assert(mutation.new_key == "fireball-cosmetic-document-2");
  assert(state.CommitMutation(mutation));

  mutation = state.PrepareMutation(first, first_epoch, restored_binding,
                                   CosmeticStyleLayer::kGeneric,
                                   ".global-ad{display:none!important;}\n");
  assert(mutation.action == RendererStyleMutationAction::kInstall);
  assert(mutation.new_key == "fireball-cosmetic-generic-3");
  assert(state.CommitMutation(mutation));

  mutation = state.PrepareMutation(first, first_epoch, restored_binding,
                                   CosmeticStyleLayer::kDocument, "");
  assert(mutation.action == RendererStyleMutationAction::kRemove);
  assert(mutation.previous_key == "fireball-cosmetic-document-2");
  assert(state.CommitMutation(mutation));
  assert(state.CurrentKey(CosmeticStyleLayer::kDocument).empty());
  mutation = state.PrepareMutation(first, first_epoch, restored_binding,
                                   CosmeticStyleLayer::kDocument, "");
  assert(mutation.action == RendererStyleMutationAction::kNoop);
  assert(state.CommitMutation(mutation));

  mutation = state.PrepareMutation(
      first, first_epoch, restored_binding, CosmeticStyleLayer::kDocument,
      ".queued-before-disconnect{display:none!important;}\n");
  assert(mutation.action == RendererStyleMutationAction::kInstall);
  assert(state.CommitMutation(mutation));
  assert(!state.CurrentKey(CosmeticStyleLayer::kDocument).empty());
  state.SuspendBinding();
  assert(!state.HasBoundDocument());
  assert(state.CurrentKey(CosmeticStyleLayer::kDocument).empty());
  assert(state.CurrentKey(CosmeticStyleLayer::kGeneric).empty());
  assert(!state.BindDocument(second, first_epoch));
  assert(state.BindDocument(first, first_epoch));
  const std::uint64_t rebound_generation = state.binding_generation();
  assert(rebound_generation == restored_binding + 1);
  mutation = state.PrepareMutation(
      first, first_epoch, restored_binding, CosmeticStyleLayer::kDocument,
      ".stale-after-rebind{display:none!important;}\n");
  assert(mutation.action == RendererStyleMutationAction::kReject);
  assert(mutation.error_code == "COSMETIC_RENDERER_BINDING_MISMATCH");

  assert(state.BeginDocument());
  const std::uint64_t second_epoch = state.document_epoch();
  assert(second_epoch == first_epoch + 1);
  assert(state.binding_generation() == 0);
  assert(!state.HasBoundDocument());
  mutation = state.PrepareMutation(first, first_epoch, rebound_generation,
                                   CosmeticStyleLayer::kGeneric,
                                   ".late{display:none!important;}\n");
  assert(mutation.action == RendererStyleMutationAction::kReject);
  assert(mutation.error_code == "COSMETIC_RENDERER_EPOCH_MISMATCH");
  assert(!state.BindDocument(first, first_epoch));
  assert(state.BindDocument(second, second_epoch));
  const std::uint64_t second_binding = state.binding_generation();
  assert(second_binding == 1);
  mutation = state.PrepareMutation(
      first, second_epoch, second_binding, CosmeticStyleLayer::kGeneric,
      ".cross-document{display:none!important;}\n");
  assert(mutation.action == RendererStyleMutationAction::kReject);
  mutation = state.PrepareMutation(second, second_epoch, second_binding,
                                   CosmeticStyleLayer::kGeneric,
                                   ".fresh{display:none!important;}\n");
  assert(mutation.action == RendererStyleMutationAction::kInstall);
  assert(mutation.new_key == "fireball-cosmetic-generic-5");
  assert(state.CommitMutation(mutation));

  state.SuspendBinding();
  assert(!state.HasBoundDocument());
  assert(state.document_epoch() == second_epoch);
  assert(!state.BindDocument(first, second_epoch));
  assert(state.BindDocument(second, second_epoch));
  assert(state.binding_generation() == second_binding + 1);
  return 0;
}
