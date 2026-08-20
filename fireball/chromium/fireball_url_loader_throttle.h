#ifndef FIREBALL_CHROMIUM_FIREBALL_URL_LOADER_THROTTLE_H_
#define FIREBALL_CHROMIUM_FIREBALL_URL_LOADER_THROTTLE_H_

#include <memory>
#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "fireball/chromium/subresource_adapter_contract.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "url/origin.h"

namespace base {
class SequencedTaskRunner;
}

namespace content {
class BrowserContext;
}

namespace network {
struct ResourceRequest;
}

namespace fireball::chromium {

class ProfilePolicyBinding;

// Chromium-facing HTTP(S) subresource adapter. Initial requests are evaluated
// synchronously while CreateURLLoaderThrottles is on the Profile owner
// sequence. Redirects defer and marshal policy work back to that sequence,
// preserving adblock-rust's single-sequence contract.
class FireballURLLoaderThrottle final : public blink::URLLoaderThrottle {
 public:
  static std::unique_ptr<blink::URLLoaderThrottle> MaybeCreate(
      const network::ResourceRequest& request,
      content::BrowserContext& browser_context);

  FireballURLLoaderThrottle(const FireballURLLoaderThrottle&) = delete;
  FireballURLLoaderThrottle& operator=(const FireballURLLoaderThrottle&) =
      delete;
  ~FireballURLLoaderThrottle() override;

  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      network::HttpRequestHeadersUpdateParams* headers_update_params) override;
  const char* NameForLoggingWillStartRequest() override;

 private:
  FireballURLLoaderThrottle(
      SubresourceInput initial_input,
      SubresourceDecision initial_decision,
      std::optional<url::Origin> source_origin,
      scoped_refptr<base::SequencedTaskRunner> policy_task_runner,
      base::WeakPtr<ProfilePolicyBinding> policy_binding);

  void OnRedirectDecision(SubresourceDecision decision);
  void Cancel();

  SubresourceInput initial_input_;
  SubresourceDecision initial_decision_;
  std::optional<url::Origin> source_origin_;
  scoped_refptr<base::SequencedTaskRunner> policy_task_runner_;
  base::WeakPtr<ProfilePolicyBinding> policy_binding_;
  bool started_ = false;
  bool redirect_pending_ = false;
  base::WeakPtrFactory<FireballURLLoaderThrottle> weak_factory_{this};
};

}  // namespace fireball::chromium

#endif  // FIREBALL_CHROMIUM_FIREBALL_URL_LOADER_THROTTLE_H_
