#ifndef FIREBALL_CHROMIUM_PROFILE_REQUEST_POLICY_BUNDLE_H_
#define FIREBALL_CHROMIUM_PROFILE_REQUEST_POLICY_BUNDLE_H_

#include <memory>
#include <string>

#include "fireball/browser/domain_model.h"
#include "fireball/chromium/navigation_adapter_contract.h"
#include "fireball/components/adblock/network_evaluator.h"
#include "fireball/components/adblock/profile_policy.h"
#include "fireball/components/egress/egress_controller.h"
#include "fireball/components/navigation/request_policy.h"
#include "fireball/components/navigation/url_cleaner.h"

namespace fireball::chromium {

// Non-movable, Profile-owned policy graph. Its RequestPolicy borrows the other
// members, so the bundle is heap-created and remains at one stable address for
// the complete BrowserContext lifetime.
class ProfileRequestPolicyBundle final : public NavigationPolicyEvaluator {
 public:
  static std::unique_ptr<ProfileRequestPolicyBundle> Create(
      browser::ProfileId profile_id,
      std::unique_ptr<adblock::NetworkEvaluator> network_evaluator,
      std::unique_ptr<egress::EgressBackend> egress_backend = nullptr);

  ProfileRequestPolicyBundle(const ProfileRequestPolicyBundle&) = delete;
  ProfileRequestPolicyBundle& operator=(const ProfileRequestPolicyBundle&) =
      delete;
  ~ProfileRequestPolicyBundle() override;

  navigation::RequestPolicyDecision Evaluate(
      const navigation::RequestContext& context) override;
  std::string ExpectedProxyRules() const override;

  bool SetBlockingMode(adblock::BlockingMode mode);
  bool SetAdblockSiteExemption(std::string hostname, bool exempt);
  bool SetUrlCleaningEnabled(bool enabled);
  bool SetUrlCleaningSiteExemption(std::string hostname, bool exempt);

  bool SwitchEgress(egress::EgressMode mode,
                    bool profile_is_idle,
                    bool has_user_consent,
                    std::string* error);

  const browser::ProfileId& profile_id() const { return profile_id_; }

 private:
  ProfileRequestPolicyBundle(
      browser::ProfileId profile_id,
      std::unique_ptr<adblock::NetworkEvaluator> network_evaluator,
      std::unique_ptr<egress::EgressBackend> egress_backend);
  bool Initialize();

  browser::ProfileId profile_id_;
  std::unique_ptr<adblock::NetworkEvaluator> network_evaluator_;
  std::unique_ptr<egress::EgressBackend> egress_backend_;
  adblock::ProfilePolicy adblock_policy_;
  navigation::UrlCleaner url_cleaner_;
  egress::EgressController egress_controller_;
  navigation::RequestPolicy request_policy_;
};

}  // namespace fireball::chromium

#endif  // FIREBALL_CHROMIUM_PROFILE_REQUEST_POLICY_BUNDLE_H_
