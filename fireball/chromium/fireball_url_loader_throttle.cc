#include "fireball/chromium/fireball_url_loader_throttle.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/browser_context.h"
#include "fireball/chromium/profile_policy_binding.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "url/gurl.h"

namespace fireball::chromium {
namespace {

using network::mojom::RequestDestination;

navigation::RequestResourceType ResourceTypeForDestination(
    RequestDestination destination) {
  switch (destination) {
    case RequestDestination::kFrame:
    case RequestDestination::kIframe:
    case RequestDestination::kFencedframe:
      return navigation::RequestResourceType::kSubdocument;
    case RequestDestination::kScript:
    case RequestDestination::kServiceWorker:
    case RequestDestination::kSharedWorker:
    case RequestDestination::kWorker:
    case RequestDestination::kAudioWorklet:
    case RequestDestination::kPaintWorklet:
    case RequestDestination::kSharedStorageWorklet:
      return navigation::RequestResourceType::kScript;
    case RequestDestination::kStyle:
    case RequestDestination::kXslt:
      return navigation::RequestResourceType::kStylesheet;
    case RequestDestination::kImage:
      return navigation::RequestResourceType::kImage;
    case RequestDestination::kAudio:
    case RequestDestination::kTrack:
    case RequestDestination::kVideo:
      return navigation::RequestResourceType::kMedia;
    case RequestDestination::kFont:
      return navigation::RequestResourceType::kFont;
    case RequestDestination::kEmpty:
      return navigation::RequestResourceType::kFetch;
    case RequestDestination::kDocument:
    case RequestDestination::kEmbed:
    case RequestDestination::kManifest:
    case RequestDestination::kObject:
    case RequestDestination::kReport:
    case RequestDestination::kWebBundle:
    case RequestDestination::kWebIdentity:
    case RequestDestination::kDictionary:
    case RequestDestination::kSpeculationRules:
    case RequestDestination::kJson:
    case RequestDestination::kEmailVerification:
    case RequestDestination::kText:
      return navigation::RequestResourceType::kOther;
  }
  return navigation::RequestResourceType::kOther;
}

std::optional<url::Origin> HttpInitiator(
    const network::ResourceRequest& request) {
  if (!request.request_initiator.has_value() ||
      request.request_initiator->opaque() ||
      (request.request_initiator->scheme() != "http" &&
       request.request_initiator->scheme() != "https")) {
    return std::nullopt;
  }
  return request.request_initiator;
}

bool IsThirdParty(const GURL& url,
                  const std::optional<url::Origin>& source_origin) {
  return !source_origin.has_value() ||
         !net::registry_controlled_domains::SameDomainOrHost(
             url, *source_origin,
             net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

SubresourceInput BuildInput(
    const network::ResourceRequest& request,
    const ProfilePolicyBinding& binding,
    const std::optional<url::Origin>& source_origin) {
  return {
      .profile_id = binding.profile_id(),
      .url = request.url.spec(),
      .destination_hostname = request.url.host(),
      .source_hostname = source_origin.has_value() ? source_origin->host() : "",
      .method = request.method,
      .resource_type = ResourceTypeForDestination(request.destination),
      .third_party = IsThirdParty(request.url, source_origin),
  };
}

void RunReply(
    base::OnceCallback<void(SubresourceDecision)> callback,
    SubresourceDecision decision) {
  std::move(callback).Run(std::move(decision));
}

void EvaluateOnPolicySequence(
    base::WeakPtr<ProfilePolicyBinding> binding,
    SubresourceInput input,
    scoped_refptr<base::SequencedTaskRunner> reply_task_runner,
    base::OnceCallback<void(SubresourceDecision)> reply) {
  SubresourceDecision decision;
  if (binding) {
    decision = EvaluateSubresource(input, binding->evaluator(),
                                   binding->applied_proxy_rules());
  } else {
    decision.error_code = "PROFILE_POLICY_UNAVAILABLE";
  }
  reply_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&RunReply, std::move(reply),
                                std::move(decision)));
}

}  // namespace

std::unique_ptr<blink::URLLoaderThrottle>
FireballURLLoaderThrottle::MaybeCreate(
    const network::ResourceRequest& request,
    content::BrowserContext& browser_context) {
  if (!request.url.SchemeIsHTTPOrHTTPS() ||
      request.destination == RequestDestination::kDocument) {
    return nullptr;
  }
  ProfilePolicyBinding* binding = ProfilePolicyBinding::Get(browser_context);
  if (binding == nullptr) {
    return nullptr;
  }
  std::optional<url::Origin> source_origin = HttpInitiator(request);
  SubresourceInput input = BuildInput(request, *binding, source_origin);
  SubresourceDecision decision = EvaluateSubresource(
      input, binding->evaluator(), binding->applied_proxy_rules());
  return std::unique_ptr<blink::URLLoaderThrottle>(
      new FireballURLLoaderThrottle(
          std::move(input), std::move(decision), std::move(source_origin),
          base::SequencedTaskRunner::GetCurrentDefault(),
          binding->GetWeakPtr()));
}

FireballURLLoaderThrottle::FireballURLLoaderThrottle(
    SubresourceInput initial_input,
    SubresourceDecision initial_decision,
    std::optional<url::Origin> source_origin,
    scoped_refptr<base::SequencedTaskRunner> policy_task_runner,
    base::WeakPtr<ProfilePolicyBinding> policy_binding)
    : initial_input_(std::move(initial_input)),
      initial_decision_(std::move(initial_decision)),
      source_origin_(std::move(source_origin)),
      policy_task_runner_(std::move(policy_task_runner)),
      policy_binding_(std::move(policy_binding)) {}

FireballURLLoaderThrottle::~FireballURLLoaderThrottle() = default;

void FireballURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  if (defer != nullptr) {
    *defer = false;
  }
  if (request == nullptr || defer == nullptr || started_) {
    Cancel();
    return;
  }
  started_ = true;
  if (request->url.spec() != initial_input_.url ||
      request->method != initial_input_.method) {
    Cancel();
    return;
  }

  switch (initial_decision_.action) {
    case SubresourceAction::kAllow:
      return;
    case SubresourceAction::kBlock:
      Cancel();
      return;
    case SubresourceAction::kRedirectToDataUrl:
    case SubresourceAction::kRewriteSameOrigin:
      break;
  }

  GURL replacement(initial_decision_.replacement_url);
  const bool valid_redirect =
      initial_decision_.action == SubresourceAction::kRedirectToDataUrl &&
      replacement.is_valid() && replacement.SchemeIs("data");
  const bool valid_rewrite =
      initial_decision_.action == SubresourceAction::kRewriteSameOrigin &&
      replacement.is_valid() && replacement.SchemeIsHTTPOrHTTPS() &&
      replacement.host() == initial_input_.destination_hostname &&
      replacement.scheme() == request->url.scheme();
  if (!valid_redirect && !valid_rewrite) {
    Cancel();
    return;
  }
  request->url = std::move(replacement);
}

void FireballURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead&,
    bool* defer,
    network::HttpRequestHeadersUpdateParams*) {
  if (defer != nullptr) {
    *defer = false;
  }
  if (redirect_info == nullptr || defer == nullptr || !started_ ||
      redirect_pending_ || !redirect_info->new_url.SchemeIsHTTPOrHTTPS()) {
    Cancel();
    return;
  }
  SubresourceInput input = initial_input_;
  input.url = redirect_info->new_url.spec();
  input.destination_hostname = redirect_info->new_url.host();
  input.method = redirect_info->new_method;
  input.third_party = IsThirdParty(redirect_info->new_url, source_origin_);

  *defer = true;
  redirect_pending_ = true;
  scoped_refptr<base::SequencedTaskRunner> reply_task_runner =
      base::SequencedTaskRunner::GetCurrentDefault();
  auto reply = base::BindOnce(
      &FireballURLLoaderThrottle::OnRedirectDecision,
      weak_factory_.GetWeakPtr());
  if (!policy_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&EvaluateOnPolicySequence, policy_binding_,
                         std::move(input), std::move(reply_task_runner),
                         std::move(reply)))) {
    redirect_pending_ = false;
    Cancel();
  }
}

const char* FireballURLLoaderThrottle::NameForLoggingWillStartRequest() {
  return "FireballURLLoaderThrottle";
}

void FireballURLLoaderThrottle::OnRedirectDecision(
    SubresourceDecision decision) {
  if (!redirect_pending_) {
    return;
  }
  redirect_pending_ = false;
  if (decision.action == SubresourceAction::kAllow) {
    delegate_->Resume();
    return;
  }
  // RedirectInfo may only be modified during WillRedirectRequest. A decision
  // requiring a data redirect or same-origin rewrite therefore fails closed
  // after asynchronous evaluation instead of touching an expired pointer.
  Cancel();
}

void FireballURLLoaderThrottle::Cancel() {
  if (delegate_ != nullptr) {
    delegate_->CancelWithError(net::ERR_BLOCKED_BY_CLIENT,
                               "fireball-request-policy");
  }
}

}  // namespace fireball::chromium
