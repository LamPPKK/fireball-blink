#ifndef FIREBALL_CHROMIUM_PROFILE_POLICY_BINDING_H_
#define FIREBALL_CHROMIUM_PROFILE_POLICY_BINDING_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "fireball/browser/domain_model.h"
#include "fireball/chromium/navigation_adapter_contract.h"

namespace content {
class BrowserContext;
class WebContents;
} // namespace content

namespace fireball::chromium {

// Profile-owned bridge between Chromium's storage boundary and Fireball's
// policy boundary. The binding and evaluator die with BrowserContext, so an
// off-the-record Profile cannot accidentally reuse a regular Profile policy.
class ProfilePolicyBinding final : public base::SupportsUserData::Data {
public:
  static bool Install(content::BrowserContext &browser_context,
                      browser::ProfileId profile_id,
                      browser::StorageMode storage_mode,
                      std::unique_ptr<NavigationPolicyEvaluator> evaluator,
                      std::string applied_proxy_rules);
  static ProfilePolicyBinding *Get(content::BrowserContext &browser_context);
  static const ProfilePolicyBinding *
  Get(const content::BrowserContext &browser_context);

  ProfilePolicyBinding(const ProfilePolicyBinding &) = delete;
  ProfilePolicyBinding &operator=(const ProfilePolicyBinding &) = delete;
  ~ProfilePolicyBinding() override;

  bool UpdateAppliedProxyRules(std::string applied_proxy_rules);
  bool RegisterTabContents(const browser::TabId &tab_id,
                           content::WebContents &web_contents);
  void UnregisterTabContents(const browser::TabId &tab_id,
                             const content::WebContents &web_contents);
  bool OwnsTabContents(const browser::TabId &tab_id,
                       const content::WebContents &web_contents) const;

  const browser::ProfileId &profile_id() const { return profile_id_; }
  NavigationPolicyEvaluator *evaluator() const { return evaluator_.get(); }
  const std::string &applied_proxy_rules() const {
    return applied_proxy_rules_;
  }
  base::WeakPtr<ProfilePolicyBinding> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

private:
  ProfilePolicyBinding(browser::ProfileId profile_id,
                       std::unique_ptr<NavigationPolicyEvaluator> evaluator,
                       std::string applied_proxy_rules);

  browser::ProfileId profile_id_;
  std::unique_ptr<NavigationPolicyEvaluator> evaluator_;
  std::string applied_proxy_rules_;
  std::map<browser::TabId, raw_ptr<content::WebContents>> tab_contents_;
  base::WeakPtrFactory<ProfilePolicyBinding> weak_factory_{this};
};

} // namespace fireball::chromium

#endif // FIREBALL_CHROMIUM_PROFILE_POLICY_BINDING_H_
