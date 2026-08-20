#include <iostream>
#include <string>

#include "fireball/browser/domain_model.h"
#include "fireball/chromium/navigation_adapter_contract.h"
#include "fireball/components/adblock/profile_policy.h"
#include "fireball/components/egress/egress_route.h"
#include "fireball/components/navigation/url_cleaner.h"
#include "fireball/components/privacy/network_audit.h"
#include "fireball/components/transfer/transfer_types.h"

namespace {

class OverlayPolicyEvaluator final
    : public fireball::chromium::NavigationPolicyEvaluator {
 public:
  fireball::navigation::RequestPolicyDecision Evaluate(
      const fireball::navigation::RequestContext& context) override {
    fireball::navigation::RequestPolicyDecision decision;
    decision.action = fireball::navigation::RequestAction::kAllow;
    decision.request_url = context.url;
    decision.proxy_rules = "direct://";
    return decision;
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
  OverlayPolicyEvaluator evaluator;
  const auto navigation = fireball::chromium::EvaluatePrimaryMainFrame(
      {.profile_id = *profile,
       .url = "https://example.com/",
       .destination_hostname = "example.com",
       .source_hostname = "",
       .method = "GET"},
      &evaluator, "direct://");

  const bool linked =
      browser.AddProfile(*profile, fireball::browser::StorageMode::kPersistent) &&
      blocker.AddProfile(*profile) && cleaner.AddProfile(*profile) &&
      fireball::egress::IsValidRoute(route) && transfer.has_value() &&
      navigation.action == fireball::chromium::NavigationAction::kProceed &&
      !fireball::privacy::IsStartupRequestAllowed("fireball.overlay-smoke");
  if (!linked) {
    return 2;
  }

  std::cout
      << "{\"schema_version\":1,\"kind\":\"fireball-overlay-component-link\","
         "\"status\":\"ok\"}\n";
  return 0;
}
