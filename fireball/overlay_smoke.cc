#include <iostream>
#include <memory>
#include <optional>
#include <string>

#include "fireball/browser/domain_model.h"
#include "fireball/chromium/navigation_adapter_contract.h"
#include "fireball/chromium/profile_request_policy_bundle.h"
#include "fireball/chromium/subresource_adapter_contract.h"
#include "fireball/components/adblock/profile_policy.h"
#include "fireball/components/egress/egress_route.h"
#include "fireball/components/navigation/url_cleaner.h"
#include "fireball/components/privacy/network_audit.h"
#include "fireball/components/transfer/transfer_types.h"

namespace {

class OverlayNetworkEvaluator final
    : public fireball::adblock::NetworkEvaluator {
 public:
  fireball::adblock::NetworkEvaluation Evaluate(
      const fireball::adblock::NetworkRequest&) override {
    return {fireball::adblock::EvaluationStatus::kOk, 0, std::nullopt,
            std::nullopt};
  }
};

}  // namespace

int main() {
  const auto profile = fireball::browser::ProfileId::Parse(
      "10000000-0000-4000-8000-000000000001");
  if (!profile) {
    return 1;
  }

  fireball::browser::BrowserModel browser;
  fireball::adblock::ProfilePolicy blocker;
  auto cleaner = fireball::navigation::UrlCleaner::CreateBuiltIn();
  const auto route = fireball::egress::MakeDirectRoute();
  const auto transfer = fireball::transfer::MakeUriTransferRequest(
      "https://example.com/fireball.bin",
      fireball::transfer::TransferPersistence::kEphemeral);
  auto policy_bundle = fireball::chromium::ProfileRequestPolicyBundle::Create(
      *profile, std::make_unique<OverlayNetworkEvaluator>());
  if (policy_bundle == nullptr) {
    return 2;
  }
  const auto navigation = fireball::chromium::EvaluatePrimaryMainFrame(
      {.profile_id = *profile,
       .url = "https://example.com/",
       .destination_hostname = "example.com",
       .source_hostname = "",
       .method = "GET"},
      policy_bundle.get(), policy_bundle->ExpectedProxyRules());
  const auto subresource = fireball::chromium::EvaluateSubresource(
      {.profile_id = *profile,
       .url = "https://cdn.example.com/app.js",
       .destination_hostname = "cdn.example.com",
       .source_hostname = "example.com",
       .method = "GET",
       .resource_type = fireball::navigation::RequestResourceType::kScript,
       .third_party = false},
      policy_bundle.get(), policy_bundle->ExpectedProxyRules());

  const bool linked =
      browser.AddProfile(*profile, fireball::browser::StorageMode::kPersistent) &&
      blocker.AddProfile(*profile) && cleaner.AddProfile(*profile) &&
      fireball::egress::IsValidRoute(route) && transfer.has_value() &&
      navigation.action == fireball::chromium::NavigationAction::kProceed &&
      subresource.action == fireball::chromium::SubresourceAction::kAllow &&
      !fireball::privacy::IsStartupRequestAllowed("fireball.overlay-smoke");
  if (!linked) {
    return 2;
  }

  std::cout
      << "{\"schema_version\":1,\"kind\":\"fireball-overlay-component-link\","
         "\"status\":\"ok\"}\n";
  return 0;
}
