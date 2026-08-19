#include "fireball/components/privacy/network_audit.h"

#include <cassert>

int main() {
  using fireball::privacy::IsNetworkRequestAllowed;
  using fireball::privacy::IsStartupRequestAllowed;
  using fireball::privacy::NetworkPhase;

  assert(!IsStartupRequestAllowed(""));
  assert(!IsStartupRequestAllowed("unknown-owner"));
  assert(!IsStartupRequestAllowed("component-updater"));
  assert(!IsStartupRequestAllowed("fireball.transfer.aria2"));
  assert(!IsNetworkRequestAllowed("fireball.transfer.aria2",
                                  NetworkPhase::kPostStartup, false));
  assert(IsNetworkRequestAllowed("fireball.transfer.aria2",
                                 NetworkPhase::kPostStartup, true));
  assert(!IsNetworkRequestAllowed("unknown-owner", NetworkPhase::kPostStartup,
                                  true));
  return 0;
}
