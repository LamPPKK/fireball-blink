#ifndef FIREBALL_CHROMIUM_FIREBALL_NAVIGATION_THROTTLE_H_
#define FIREBALL_CHROMIUM_FIREBALL_NAVIGATION_THROTTLE_H_

#include "base/memory/raw_ref.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationThrottleRegistry;
}

namespace fireball::chromium {

class ProfilePolicyBinding;

// First Chromium-facing vertical slice: primary-main-frame HTTP(S)
// navigations. Subresource URLLoader throttles, renderer cosmetic injection and
// lifecycle installation are separate gates and are not implied by this type.
class FireballNavigationThrottle final : public content::NavigationThrottle {
 public:
  // Returns false when the BrowserContext has no explicit Fireball binding or
  // the request is outside this adapter's primary-main-frame HTTP(S) scope.
  static bool MaybeCreateAndAdd(
      content::NavigationThrottleRegistry& registry);

  FireballNavigationThrottle(
      content::NavigationThrottleRegistry& registry,
      const ProfilePolicyBinding& binding);
  FireballNavigationThrottle(const FireballNavigationThrottle&) = delete;
  FireballNavigationThrottle& operator=(const FireballNavigationThrottle&) =
      delete;
  ~FireballNavigationThrottle() override;

  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  const char* GetNameForLogging() override;

 private:
  ThrottleCheckResult EvaluateCurrentUrl();

  const raw_ref<const ProfilePolicyBinding> binding_;
};

}  // namespace fireball::chromium

#endif  // FIREBALL_CHROMIUM_FIREBALL_NAVIGATION_THROTTLE_H_
