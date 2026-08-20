#include "fireball/chromium/browser_cosmetic_document_state.h"

#include <cassert>

int main() {
  using fireball::chromium::BrowserCosmeticDocumentPhase;
  using fireball::chromium::BrowserCosmeticDocumentState;

  BrowserCosmeticDocumentState state;
  assert(state.phase() == BrowserCosmeticDocumentPhase::kDormant);
  auto first = state.BeginActivation();
  assert(first.has_value());
  assert(!state.BeginActivation().has_value());
  assert(state.CompleteBinding(*first, true));
  assert(state.phase() == BrowserCosmeticDocumentPhase::kRestoring);
  assert(!state.ready());
  assert(state.CompleteRestore(*first, true));
  assert(state.ready());

  state.Suspend();
  assert(state.phase() == BrowserCosmeticDocumentPhase::kSuspended);
  assert(!state.IsCurrent(*first));
  assert(!state.CompleteBinding(*first, true));
  assert(!state.CompleteRestore(*first, true));

  auto restored = state.BeginActivation();
  assert(restored.has_value());
  assert(state.CompleteBinding(*restored, true));
  assert(state.CompleteRestore(*restored, true));
  assert(state.ready());
  auto revoke = state.BeginRevocation();
  assert(revoke.has_value());
  assert(!state.BeginRevocation().has_value());
  assert(state.CompleteRevocation(*revoke, true));
  assert(state.phase() == BrowserCosmeticDocumentPhase::kRevoked);

  auto rebound = state.BeginActivation();
  assert(rebound.has_value());
  assert(state.CompleteBinding(*rebound, false));
  assert(state.phase() == BrowserCosmeticDocumentPhase::kFailed);
  auto retry = state.BeginActivation();
  assert(retry.has_value());
  state.Fail();
  assert(!state.IsCurrent(*retry));
  assert(!state.CompleteBinding(*retry, true));
  assert(state.phase() == BrowserCosmeticDocumentPhase::kFailed);
  return 0;
}
