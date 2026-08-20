#include "fireball/chromium/browser_cosmetic_transport_state.h"

#include <cassert>

int main() {
  using fireball::chromium::BrowserCosmeticTransportPhase;
  using fireball::chromium::BrowserCosmeticTransportState;

  BrowserCosmeticTransportState rejected_epoch;
  auto rejected_ticket = rejected_epoch.BeginBinding();
  assert(rejected_ticket.has_value());
  assert(!rejected_epoch.AcceptDocumentEpoch(*rejected_ticket, 0));
  assert(rejected_epoch.phase() == BrowserCosmeticTransportPhase::kFailed);
  assert(rejected_epoch.document_epoch() == 0);
  assert(!rejected_epoch.BeginBinding().has_value());

  BrowserCosmeticTransportState state;
  auto bind_ticket = state.BeginBinding();
  assert(bind_ticket.has_value());
  assert(state.phase() == BrowserCosmeticTransportPhase::kReadingDocumentEpoch);
  assert(!state.BeginBinding().has_value());
  assert(state.AcceptDocumentEpoch(*bind_ticket, 41));
  assert(state.phase() == BrowserCosmeticTransportPhase::kBindingDocument);
  assert(state.document_epoch() == 41);
  assert(state.CompleteBinding(*bind_ticket, true));
  assert(state.ready());

  auto apply_ticket = state.BeginMutation(/*revoke_document=*/false);
  assert(apply_ticket.has_value());
  assert(state.phase() == BrowserCosmeticTransportPhase::kApplyingStylesheet);
  assert(!state.BeginMutation(/*revoke_document=*/false).has_value());
  assert(!state.CompleteMutation(*bind_ticket, true));
  assert(state.CompleteMutation(*apply_ticket, true));
  assert(state.ready());
  assert(state.document_epoch() == 41);

  auto stale_ticket = state.BeginMutation(/*revoke_document=*/false);
  assert(stale_ticket.has_value());
  state.Invalidate();
  assert(state.phase() == BrowserCosmeticTransportPhase::kFailed);
  assert(state.document_epoch() == 0);
  assert(!state.IsCurrent(*stale_ticket));
  assert(!state.CompleteMutation(*stale_ticket, true));

  BrowserCosmeticTransportState revoke_state;
  auto second_bind = revoke_state.BeginBinding();
  assert(second_bind.has_value());
  assert(revoke_state.AcceptDocumentEpoch(*second_bind, 99));
  assert(revoke_state.CompleteBinding(*second_bind, true));
  auto revoke_ticket = revoke_state.BeginMutation(/*revoke_document=*/true);
  assert(revoke_ticket.has_value());
  assert(revoke_state.phase() ==
         BrowserCosmeticTransportPhase::kRevokingDocument);
  assert(revoke_state.CompleteMutation(*revoke_ticket, true));
  assert(revoke_state.phase() == BrowserCosmeticTransportPhase::kRevoked);
  assert(revoke_state.document_epoch() == 0);
  assert(!revoke_state.BeginMutation(/*revoke_document=*/false).has_value());

  BrowserCosmeticTransportState rejected_mutation;
  auto third_bind = rejected_mutation.BeginBinding();
  assert(third_bind.has_value());
  assert(rejected_mutation.AcceptDocumentEpoch(*third_bind, 7));
  assert(rejected_mutation.CompleteBinding(*third_bind, true));
  auto failed_apply =
      rejected_mutation.BeginMutation(/*revoke_document=*/false);
  assert(failed_apply.has_value());
  assert(rejected_mutation.CompleteMutation(*failed_apply, false));
  assert(rejected_mutation.phase() == BrowserCosmeticTransportPhase::kFailed);
  assert(rejected_mutation.document_epoch() == 0);
  return 0;
}
