#include "fireball/components/privacy/network_audit.h"

namespace fireball::privacy {

bool IsStartupRequestAllowed(std::string_view owner) {
  return owner == "component-updater-after-user-consent";
}

}  // namespace fireball::privacy
