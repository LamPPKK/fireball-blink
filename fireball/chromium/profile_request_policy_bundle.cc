#include "fireball/chromium/profile_request_policy_bundle.h"

#include <utility>

namespace fireball::chromium {
namespace {

navigation::RequestPolicyDecision Error(std::string code) {
  navigation::RequestPolicyDecision decision;
  decision.error_code = std::move(code);
  return decision;
}

}  // namespace

std::unique_ptr<ProfileRequestPolicyBundle>
ProfileRequestPolicyBundle::Create(
    browser::ProfileId profile_id,
    std::unique_ptr<adblock::NetworkEvaluator> network_evaluator,
    std::unique_ptr<egress::EgressBackend> egress_backend) {
  if (network_evaluator == nullptr) {
    return nullptr;
  }
  std::unique_ptr<ProfileRequestPolicyBundle> bundle(
      new ProfileRequestPolicyBundle(std::move(profile_id),
                                     std::move(network_evaluator),
                                     std::move(egress_backend)));
  if (!bundle->Initialize()) {
    return nullptr;
  }
  return bundle;
}

ProfileRequestPolicyBundle::ProfileRequestPolicyBundle(
    browser::ProfileId profile_id,
    std::unique_ptr<adblock::NetworkEvaluator> network_evaluator,
    std::unique_ptr<egress::EgressBackend> egress_backend)
    : profile_id_(std::move(profile_id)),
      network_evaluator_(std::move(network_evaluator)),
      egress_backend_(std::move(egress_backend)),
      url_cleaner_(navigation::UrlCleaner::CreateBuiltIn()),
      egress_controller_(egress_backend_.get()),
      request_policy_(&adblock_policy_, network_evaluator_.get(),
                      &url_cleaner_, &egress_controller_) {}

ProfileRequestPolicyBundle::~ProfileRequestPolicyBundle() {
  egress_controller_.RemoveProfile(profile_id_);
  url_cleaner_.RemoveProfile(profile_id_);
  adblock_policy_.RemoveProfile(profile_id_);
}

bool ProfileRequestPolicyBundle::Initialize() {
  return adblock_policy_.AddProfile(profile_id_) &&
         url_cleaner_.AddProfile(profile_id_) &&
         egress_controller_.AddProfile(profile_id_);
}

navigation::RequestPolicyDecision ProfileRequestPolicyBundle::Evaluate(
    const navigation::RequestContext& context) {
  if (context.profile_id != profile_id_) {
    return Error("PROFILE_POLICY_BOUNDARY_MISMATCH");
  }
  return request_policy_.Evaluate(context);
}

std::string ProfileRequestPolicyBundle::ExpectedProxyRules() const {
  const egress::EgressRoute* route =
      egress_controller_.GetRoute(profile_id_);
  return route == nullptr ? std::string() : egress::ChromiumProxyRules(*route);
}

bool ProfileRequestPolicyBundle::SetBlockingMode(
    adblock::BlockingMode mode) {
  return adblock_policy_.SetMode(profile_id_, mode);
}

bool ProfileRequestPolicyBundle::SetAdblockSiteExemption(
    std::string hostname,
    bool exempt) {
  return adblock_policy_.SetSiteExemption(profile_id_, std::move(hostname),
                                          exempt);
}

bool ProfileRequestPolicyBundle::SetUrlCleaningEnabled(bool enabled) {
  return url_cleaner_.SetEnabled(profile_id_, enabled);
}

bool ProfileRequestPolicyBundle::SetUrlCleaningSiteExemption(
    std::string hostname,
    bool exempt) {
  return url_cleaner_.SetSiteExemption(profile_id_, std::move(hostname),
                                       exempt);
}

bool ProfileRequestPolicyBundle::SwitchEgress(
    egress::EgressMode mode,
    bool profile_is_idle,
    bool has_user_consent,
    std::string* error) {
  return egress_controller_.Switch(profile_id_, mode, profile_is_idle,
                                   has_user_consent, error);
}

}  // namespace fireball::chromium
