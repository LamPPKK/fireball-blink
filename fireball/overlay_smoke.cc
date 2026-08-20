#include <iostream>
#include <string>

#include "fireball/browser/domain_model.h"
#include "fireball/components/adblock/profile_policy.h"
#include "fireball/components/egress/egress_route.h"
#include "fireball/components/navigation/url_cleaner.h"
#include "fireball/components/privacy/network_audit.h"
#include "fireball/components/transfer/transfer_types.h"

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

  const bool linked =
      browser.AddProfile(*profile, fireball::browser::StorageMode::kPersistent) &&
      blocker.AddProfile(*profile) && cleaner.AddProfile(*profile) &&
      fireball::egress::IsValidRoute(route) && transfer.has_value() &&
      !fireball::privacy::IsStartupRequestAllowed("fireball.overlay-smoke");
  if (!linked) {
    return 2;
  }

  std::cout
      << "{\"schema_version\":1,\"kind\":\"fireball-overlay-component-link\","
         "\"status\":\"ok\"}\n";
  return 0;
}
