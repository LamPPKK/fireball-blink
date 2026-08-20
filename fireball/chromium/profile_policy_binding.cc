#include "fireball/chromium/profile_policy_binding.h"

#include <utility>

#include "content/public/browser/browser_context.h"

namespace fireball::chromium {
namespace {

const void *BindingKey() {
  static const int key = 0;
  return &key;
}

} // namespace

ProfilePolicyBinding::ProfilePolicyBinding(
    browser::ProfileId profile_id,
    std::unique_ptr<NavigationPolicyEvaluator> evaluator,
    std::string applied_proxy_rules)
    : profile_id_(std::move(profile_id)), evaluator_(std::move(evaluator)),
      applied_proxy_rules_(std::move(applied_proxy_rules)) {}

ProfilePolicyBinding::~ProfilePolicyBinding() = default;

bool ProfilePolicyBinding::Install(
    content::BrowserContext &browser_context, browser::ProfileId profile_id,
    browser::StorageMode storage_mode,
    std::unique_ptr<NavigationPolicyEvaluator> evaluator,
    std::string applied_proxy_rules) {
  const bool expects_off_the_record =
      storage_mode == browser::StorageMode::kOffTheRecord;
  if (Get(browser_context) != nullptr || evaluator == nullptr ||
      browser_context.IsOffTheRecord() != expects_off_the_record ||
      !IsValidAppliedProxyRules(applied_proxy_rules) ||
      evaluator->ExpectedProxyRules() != applied_proxy_rules) {
    return false;
  }
  browser_context.SetUserData(
      BindingKey(),
      std::unique_ptr<base::SupportsUserData::Data>(
          new ProfilePolicyBinding(std::move(profile_id), std::move(evaluator),
                                   std::move(applied_proxy_rules))));
  return true;
}

ProfilePolicyBinding *
ProfilePolicyBinding::Get(content::BrowserContext &browser_context) {
  return static_cast<ProfilePolicyBinding *>(
      browser_context.GetUserData(BindingKey()));
}

const ProfilePolicyBinding *
ProfilePolicyBinding::Get(const content::BrowserContext &browser_context) {
  return static_cast<const ProfilePolicyBinding *>(
      browser_context.GetUserData(BindingKey()));
}

bool ProfilePolicyBinding::UpdateAppliedProxyRules(
    std::string applied_proxy_rules) {
  if (!IsValidAppliedProxyRules(applied_proxy_rules)) {
    return false;
  }
  if (evaluator_->ExpectedProxyRules() != applied_proxy_rules) {
    return false;
  }
  applied_proxy_rules_ = std::move(applied_proxy_rules);
  return true;
}

bool ProfilePolicyBinding::RegisterTabContents(
    const browser::TabId &tab_id, content::WebContents &web_contents) {
  return tab_contents_.emplace(tab_id, &web_contents).second;
}

void ProfilePolicyBinding::UnregisterTabContents(
    const browser::TabId &tab_id, const content::WebContents &web_contents) {
  auto entry = tab_contents_.find(tab_id);
  if (entry != tab_contents_.end() && entry->second == &web_contents) {
    tab_contents_.erase(entry);
  }
}

bool ProfilePolicyBinding::OwnsTabContents(
    const browser::TabId &tab_id,
    const content::WebContents &web_contents) const {
  auto entry = tab_contents_.find(tab_id);
  return entry != tab_contents_.end() && entry->second == &web_contents;
}

} // namespace fireball::chromium
