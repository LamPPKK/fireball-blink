#include "fireball/chromium/fireball_navigation_throttle.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "fireball/chromium/navigation_adapter_contract.h"
#include "fireball/chromium/profile_policy_binding.h"
#include "url/gurl.h"

namespace fireball::chromium {

bool FireballNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  content::NavigationHandle& handle = registry.GetNavigationHandle();
  if (!handle.IsInPrimaryMainFrame() ||
      !handle.GetURL().SchemeIsHTTPOrHTTPS()) {
    return false;
  }
  content::WebContents* web_contents = handle.GetWebContents();
  if (web_contents == nullptr) {
    return false;
  }
  ProfilePolicyBinding* binding =
      ProfilePolicyBinding::Get(*web_contents->GetBrowserContext());
  if (binding == nullptr) {
    return false;
  }
  registry.AddThrottle(
      std::make_unique<FireballNavigationThrottle>(registry, *binding));
  return true;
}

FireballNavigationThrottle::FireballNavigationThrottle(
    content::NavigationThrottleRegistry& registry,
    const ProfilePolicyBinding& binding)
    : content::NavigationThrottle(registry), binding_(binding) {}

FireballNavigationThrottle::~FireballNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
FireballNavigationThrottle::WillStartRequest() {
  return EvaluateCurrentUrl();
}

content::NavigationThrottle::ThrottleCheckResult
FireballNavigationThrottle::WillRedirectRequest() {
  return EvaluateCurrentUrl();
}

const char* FireballNavigationThrottle::GetNameForLogging() {
  return "FireballNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
FireballNavigationThrottle::EvaluateCurrentUrl() {
  content::NavigationHandle* handle = navigation_handle();
  const GURL& url = handle->GetURL();
  if (!handle->IsInPrimaryMainFrame() || !url.SchemeIsHTTPOrHTTPS()) {
    return PROCEED;
  }
  const GURL& referrer = handle->GetReferrer().url;
  NavigationInput input{
      .profile_id = binding_->profile_id(),
      .url = url.spec(),
      .destination_hostname = url.host(),
      .source_hostname = referrer.SchemeIsHTTPOrHTTPS() ? referrer.host() : "",
      .method = handle->GetRequestMethod(),
      .same_document = handle->IsSameDocument(),
  };
  NavigationDecision decision = EvaluatePrimaryMainFrame(
      input, binding_->evaluator(), binding_->applied_proxy_rules());
  switch (decision.action) {
    case NavigationAction::kProceed:
      return PROCEED;
    case NavigationAction::kBlock:
      return BLOCK_REQUEST;
    case NavigationAction::kRestartWithCleanedUrl:
      break;
  }

  GURL replacement(decision.replacement_url);
  if (!replacement.is_valid() || !replacement.SchemeIsHTTPOrHTTPS()) {
    return BLOCK_REQUEST;
  }
  content::OpenURLParams params =
      content::OpenURLParams::FromNavigationHandle(handle);
  params.url = std::move(replacement);
  base::WeakPtr<content::WebContents> weak_web_contents =
      handle->GetWebContents()->GetWeakPtr();
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<content::WebContents> web_contents,
             content::OpenURLParams open_url_params) {
            if (web_contents) {
              web_contents->OpenURL(open_url_params, {});
            }
          },
          std::move(weak_web_contents), std::move(params)));
  return CANCEL_AND_IGNORE;
}

}  // namespace fireball::chromium
