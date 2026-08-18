#include "fireball/components/privacy/network_audit.h"

#include <cassert>

int main() {
  using fireball::privacy::IsNetworkRequestAllowed;
  using fireball::privacy::IsStartupRequestAllowed;
  using fireball::privacy::NetworkPhase;

  assert(!IsStartupRequestAllowed(""));
  assert(!IsStartupRequestAllowed("unknown-owner"));
  assert(!IsStartupRequestAllowed("component-updater"));
  assert(!IsNetworkRequestAllowed("unknown-owner", NetworkPhase::kPostStartup,
                                  true));
  return 0;
}
