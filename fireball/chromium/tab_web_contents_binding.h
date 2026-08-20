#ifndef FIREBALL_CHROMIUM_TAB_WEB_CONTENTS_BINDING_H_
#define FIREBALL_CHROMIUM_TAB_WEB_CONTENTS_BINDING_H_

#include <cstdint>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_user_data.h"
#include "fireball/browser/domain_model.h"

namespace content {
class WebContents;
}

namespace fireball::chromium {

class ProfilePolicyBinding;

// Authoritative Chrome-tab binding for one WebContents. Installation reserves
// the TabId inside its profile BrowserContext; a generation claim permits only
// one cosmetic controller bridge for that bound tab at a time.
class TabWebContentsBinding final
    : public content::WebContentsUserData<TabWebContentsBinding> {
public:
  static bool Install(content::WebContents &web_contents,
                      browser::ProfileId profile_id, browser::TabId tab_id);

  TabWebContentsBinding(const TabWebContentsBinding &) = delete;
  TabWebContentsBinding &operator=(const TabWebContentsBinding &) = delete;
  ~TabWebContentsBinding() override;

  bool Matches(const browser::ProfileId &profile_id,
               const browser::TabId &tab_id) const;
  std::optional<std::uint64_t> AcquireCosmeticController();
  bool OwnsCosmeticController(std::uint64_t claim) const;
  void ReleaseCosmeticController(std::uint64_t claim);
  base::WeakPtr<TabWebContentsBinding> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

private:
  friend class content::WebContentsUserData<TabWebContentsBinding>;

  TabWebContentsBinding(content::WebContents *web_contents,
                        browser::ProfileId profile_id, browser::TabId tab_id,
                        base::WeakPtr<ProfilePolicyBinding> profile_binding);

  browser::ProfileId profile_id_;
  browser::TabId tab_id_;
  base::WeakPtr<ProfilePolicyBinding> profile_binding_;
  std::uint64_t claim_generation_ = 0;
  bool controller_claimed_ = false;
  base::WeakPtrFactory<TabWebContentsBinding> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

} // namespace fireball::chromium

#endif // FIREBALL_CHROMIUM_TAB_WEB_CONTENTS_BINDING_H_
