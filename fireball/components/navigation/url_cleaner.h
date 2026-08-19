#ifndef FIREBALL_COMPONENTS_NAVIGATION_URL_CLEANER_H_
#define FIREBALL_COMPONENTS_NAVIGATION_URL_CLEANER_H_

#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "fireball/browser/domain_model.h"

namespace fireball::navigation {

inline constexpr std::size_t kMaximumNavigationUrlBytes = 8192;

enum class UrlCleanStatus {
  kInvalid,
  kUnchanged,
  kCleaned,
};

struct UrlCleanResult {
  UrlCleanStatus status = UrlCleanStatus::kInvalid;
  std::string url;
  std::size_t removed_parameters = 0;

  bool valid() const { return status != UrlCleanStatus::kInvalid; }
};

// Extracts a lower-case DNS/IP hostname from a strict HTTP(S) URL. Credentials,
// malformed ports, controls and backslashes are rejected.
std::optional<std::string> ExtractNavigationHostname(std::string_view url);
bool IsCanonicalNavigationHostname(std::string_view hostname);
bool IsSafeNavigationUrlForHostname(std::string_view url,
                                    std::string_view hostname);

// A versioned exact-name query cleaner. Rules and exemptions never inspect or
// retain parameter values. Policy is Profile-scoped so Spaces cannot carry an
// allowlist across cookie/storage boundaries.
class UrlCleaner final {
 public:
  static std::optional<UrlCleaner> Create(
      std::string rules_version,
      std::vector<std::string> tracking_parameters);
  static UrlCleaner CreateBuiltIn();

  bool AddProfile(browser::ProfileId profile_id);
  bool RemoveProfile(const browser::ProfileId& profile_id);
  bool HasProfile(const browser::ProfileId& profile_id) const;
  bool SetEnabled(const browser::ProfileId& profile_id, bool enabled);
  bool SetSiteExemption(const browser::ProfileId& profile_id,
                        std::string hostname,
                        bool exempt);

  UrlCleanResult Clean(const browser::ProfileId& profile_id,
                       std::string_view url,
                       std::string_view hostname) const;

  const std::string& rules_version() const { return rules_version_; }

 private:
  struct ProfileEntry {
    bool enabled = true;
    std::set<std::string, std::less<>> exemptions;
  };

  UrlCleaner(std::string rules_version,
             std::set<std::string, std::less<>> tracking_parameters);

  std::string rules_version_;
  std::set<std::string, std::less<>> tracking_parameters_;
  std::map<browser::ProfileId, ProfileEntry> profiles_;
};

}  // namespace fireball::navigation

#endif  // FIREBALL_COMPONENTS_NAVIGATION_URL_CLEANER_H_
