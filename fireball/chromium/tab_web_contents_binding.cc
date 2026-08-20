#include "fireball/chromium/tab_web_contents_binding.h"

#include <limits>
#include <utility>

#include "content/public/browser/web_contents.h"
#include "fireball/chromium/profile_policy_binding.h"

namespace fireball::chromium {

WEB_CONTENTS_USER_DATA_KEY_IMPL(TabWebContentsBinding);

// static
bool TabWebContentsBinding::Install(content::WebContents &web_contents,
                                    browser::ProfileId profile_id,
                                    browser::TabId tab_id) {
  if (TabWebContentsBinding::FromWebContents(&web_contents) != nullptr) {
    return false;
  }
  ProfilePolicyBinding *profile_binding =
      ProfilePolicyBinding::Get(*web_contents.GetBrowserContext());
  if (profile_binding == nullptr ||
      profile_binding->profile_id() != profile_id ||
      !profile_binding->RegisterTabContents(tab_id, web_contents)) {
    return false;
  }
  TabWebContentsBinding::CreateForWebContents(&web_contents, profile_id, tab_id,
                                              profile_binding->GetWeakPtr());
  if (TabWebContentsBinding::FromWebContents(&web_contents) == nullptr) {
    profile_binding->UnregisterTabContents(tab_id, web_contents);
    return false;
  }
  return true;
}

TabWebContentsBinding::TabWebContentsBinding(
    content::WebContents *web_contents, browser::ProfileId profile_id,
    browser::TabId tab_id, base::WeakPtr<ProfilePolicyBinding> profile_binding)
    : content::WebContentsUserData<TabWebContentsBinding>(*web_contents),
      profile_id_(std::move(profile_id)), tab_id_(std::move(tab_id)),
      profile_binding_(std::move(profile_binding)) {}

TabWebContentsBinding::~TabWebContentsBinding() {
  weak_factory_.InvalidateWeakPtrs();
  if (profile_binding_) {
    profile_binding_->UnregisterTabContents(tab_id_, GetWebContents());
  }
}

bool TabWebContentsBinding::Matches(const browser::ProfileId &profile_id,
                                    const browser::TabId &tab_id) const {
  return profile_id_ == profile_id && tab_id_ == tab_id && profile_binding_ &&
         profile_binding_->profile_id() == profile_id &&
         profile_binding_->OwnsTabContents(tab_id, GetWebContents());
}

std::optional<std::uint64_t>
TabWebContentsBinding::AcquireCosmeticController() {
  if (controller_claimed_ ||
      claim_generation_ == std::numeric_limits<std::uint64_t>::max()) {
    return std::nullopt;
  }
  controller_claimed_ = true;
  return ++claim_generation_;
}

bool TabWebContentsBinding::OwnsCosmeticController(std::uint64_t claim) const {
  return controller_claimed_ && claim != 0 && claim == claim_generation_;
}

void TabWebContentsBinding::ReleaseCosmeticController(std::uint64_t claim) {
  if (OwnsCosmeticController(claim)) {
    controller_claimed_ = false;
  }
}

} // namespace fireball::chromium
