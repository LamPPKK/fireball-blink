#include "fireball/components/navigation/url_cleaner.h"

#include <iostream>
#include <string>
#include <string_view>

#include "fireball/components/navigation/generated_url_cleaner_rules.h"
#include "tests/generated/url_cleaner_corpus.h"

namespace {

std::string_view StatusName(fireball::navigation::UrlCleanStatus status) {
  using fireball::navigation::UrlCleanStatus;
  switch (status) {
    case UrlCleanStatus::kInvalid:
      return "invalid";
    case UrlCleanStatus::kUnchanged:
      return "unchanged";
    case UrlCleanStatus::kCleaned:
      return "cleaned";
  }
  return "unknown";
}

}  // namespace

int main() {
  using fireball::browser::ProfileId;
  using fireball::navigation::UrlCleaner;
  using fireball::navigation::generated::kUrlCleanerRulesVersion;
  using fireball::navigation::test_data::kUrlCleanerCorpus;
  using fireball::navigation::test_data::kUrlCleanerCorpusRulesVersion;

  if (kUrlCleanerRulesVersion != kUrlCleanerCorpusRulesVersion) {
    std::cerr << "URL Cleaner corpus version does not match runtime rules\n";
    return 1;
  }
  const ProfileId profile =
      *ProfileId::Parse("19000000-0000-4000-8000-000000000001");
  for (const auto& test_case : kUrlCleanerCorpus) {
    UrlCleaner cleaner = UrlCleaner::CreateBuiltIn();
    if (!cleaner.AddProfile(profile) ||
        !cleaner.SetEnabled(profile, test_case.enabled) ||
        (test_case.site_exempt &&
         !cleaner.SetSiteExemption(profile, std::string(test_case.hostname),
                                   true))) {
      std::cerr << test_case.name << ": unable to configure profile policy\n";
      return 1;
    }
    const auto result =
        cleaner.Clean(profile, test_case.input_url, test_case.hostname);
    if (StatusName(result.status) != test_case.expected_status ||
        result.url != test_case.expected_url ||
        result.removed_parameters != test_case.removed_parameters) {
      std::cerr << test_case.name << ": corpus expectation failed\n";
      return 1;
    }
  }
  return 0;
}
