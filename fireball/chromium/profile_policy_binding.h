#ifndef FIREBALL_CHROMIUM_PROFILE_POLICY_BINDING_H_
#define FIREBALL_CHROMIUM_PROFILE_POLICY_BINDING_H_

#include <memory>
#include <string>

#include "base/supports_user_data.h"
#include "fireball/browser/domain_model.h"
#include "fireball/chromium/navigation_adapter_contract.h"

namespace content {
class BrowserContext;
}

namespace fireball::chromium {

// Profile-owned bridge between Chromium's storage boundary and Fireball's
// policy boundary. The binding and evaluator die with BrowserContext, so an
// off-the-record Profile cannot accidentally reuse a regular Profile policy.
class ProfilePolicyBinding final : public base::SupportsUserData::Data {
 public:
  static bool Install(
      content::BrowserContext& browser_context,
      browser::ProfileId profile_id,
      browser::StorageMode storage_mode,
      std::unique_ptr<NavigationPolicyEvaluator> evaluator,
      std::string applied_proxy_rules);
  static ProfilePolicyBinding* Get(content::BrowserContext& browser_context);
  static const ProfilePolicyBinding* Get(
      const content::BrowserContext& browser_context);

  ProfilePolicyBinding(const ProfilePolicyBinding&) = delete;
  ProfilePolicyBinding& operator=(const ProfilePolicyBinding&) = delete;
  ~ProfilePolicyBinding() override;

  bool UpdateAppliedProxyRules(std::string applied_proxy_rules);

  const browser::ProfileId& profile_id() const { return profile_id_; }
  NavigationPolicyEvaluator* evaluator() const { return evaluator_.get(); }
  const std::string& applied_proxy_rules() const {
    return applied_proxy_rules_;
  }

 private:
  ProfilePolicyBinding(browser::ProfileId profile_id,
                       std::unique_ptr<NavigationPolicyEvaluator> evaluator,
                       std::string applied_proxy_rules);

  browser::ProfileId profile_id_;
  std::unique_ptr<NavigationPolicyEvaluator> evaluator_;
  std::string applied_proxy_rules_;
};

}  // namespace fireball::chromium

#endif  // FIREBALL_CHROMIUM_PROFILE_POLICY_BINDING_H_
