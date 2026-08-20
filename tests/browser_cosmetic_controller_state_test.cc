#include "fireball/chromium/browser_cosmetic_controller_state.h"

#include <cassert>
#include <string>
#include <utility>
#include <vector>

namespace {

fireball::browser::DocumentId Parse(const char *value) {
  auto parsed = fireball::browser::DocumentId::Parse(std::string(value));
  assert(parsed.has_value());
  return std::move(*parsed);
}

} // namespace

int main() {
  using fireball::chromium::BrowserCosmeticControllerPhase;
  using fireball::chromium::BrowserCosmeticControllerState;
  using fireball::chromium::IsBoundedCosmeticDomSnapshot;

  assert(IsBoundedCosmeticDomSnapshot({"advert", "sponsor"}, {"hero"}));
  assert(!IsBoundedCosmeticDomSnapshot({}, {""}));
  assert(!IsBoundedCosmeticDomSnapshot(
      {std::string(fireball::chromium::kMaximumCosmeticDomTokenBytes + 1, 'x')},
      {}));
  assert(!IsBoundedCosmeticDomSnapshot(
      std::vector<std::string>(
          fireball::chromium::kMaximumCosmeticDomEntries + 1, "x"),
      {}));
  assert(!IsBoundedCosmeticDomSnapshot({std::string("bad\0token", 9)}, {}));

  const auto first = Parse("40000000-0000-4000-8000-000000000001");
  const auto second = Parse("40000000-0000-4000-8000-000000000002");
  BrowserCosmeticControllerState state;
  assert(state.phase() == BrowserCosmeticControllerPhase::kIdle);

  auto activation = state.BeginActivation(first, 0);
  assert(activation.has_value());
  assert(!state.BeginActivation(first, 0).has_value());
  assert(state.CompleteActivation(*activation, true));
  assert(state.ready());

  auto generic = state.BeginGenericMutation(first, 1);
  assert(generic.has_value());
  assert(!state.BeginGenericMutation(first, 2).has_value());
  assert(state.CompleteGenericMutation(*generic, true));
  assert(state.last_dom_revision() == 1);
  assert(!state.BeginGenericMutation(first, 1).has_value());
  assert(!state.BeginGenericMutation(second, 2).has_value());

  auto late_generic = state.BeginGenericMutation(first, 2);
  assert(late_generic.has_value());
  assert(state.Suspend(first));
  assert(state.phase() == BrowserCosmeticControllerPhase::kSuspended);
  assert(!state.CompleteGenericMutation(*late_generic, true));

  auto restore = state.BeginActivation(first, 1);
  assert(restore.has_value());
  assert(state.CompleteActivation(*restore, true));
  assert(state.ready());
  assert(state.last_dom_revision() == 1);

  auto revoke = state.BeginRevocation(first);
  assert(revoke.has_value());
  assert(state.CompleteRevocation(*revoke, true));
  assert(state.phase() == BrowserCosmeticControllerPhase::kIdle);
  assert(!state.active_document_id().has_value());

  auto next = state.BeginActivation(second, 0);
  assert(next.has_value());
  assert(state.CompleteActivation(*next, false));
  assert(state.phase() == BrowserCosmeticControllerPhase::kFailed);

  assert(state.Reset(second));
  assert(state.phase() == BrowserCosmeticControllerPhase::kIdle);
  assert(!state.active_document_id().has_value());

  auto recovery = state.BeginActivation(second, 0);
  assert(recovery.has_value());
  assert(state.CompleteActivation(*recovery, true));
  assert(state.Dispose(second));
  assert(state.phase() == BrowserCosmeticControllerPhase::kIdle);
  assert(!state.Dispose(second));
  assert(!state.Suspend(first));
  auto retry = state.BeginActivation(second, 0);
  assert(retry.has_value());
  assert(state.Fail(second));
  assert(!state.CompleteActivation(*retry, true));
  assert(state.phase() == BrowserCosmeticControllerPhase::kFailed);

  auto failed_revoke_activation = state.BeginActivation(second, 4);
  assert(failed_revoke_activation.has_value());
  assert(state.CompleteActivation(*failed_revoke_activation, true));
  auto failed_revoke = state.BeginRevocation(second);
  assert(failed_revoke.has_value());
  assert(state.CompleteRevocation(*failed_revoke, false));
  assert(state.phase() == BrowserCosmeticControllerPhase::kFailed);
  assert(state.active_document_id().has_value());
  assert(*state.active_document_id() == second);

  state.ResetAll();
  assert(state.phase() == BrowserCosmeticControllerPhase::kIdle);
  assert(!state.active_document_id().has_value());
  auto cleanup_activation = state.BeginActivation(first, 0);
  assert(cleanup_activation.has_value());
  assert(state.CompleteActivation(*cleanup_activation, true));
  auto synchronous_cleanup_callback = state.BeginGenericMutation(first, 1);
  assert(synchronous_cleanup_callback.has_value());
  state.ResetAll();
  assert(!state.CompleteGenericMutation(*synchronous_cleanup_callback, true));
  assert(state.phase() == BrowserCosmeticControllerPhase::kIdle);
  return 0;
}
