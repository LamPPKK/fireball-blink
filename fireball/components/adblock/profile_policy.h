#ifndef FIREBALL_COMPONENTS_ADBLOCK_PROFILE_POLICY_H_
#define FIREBALL_COMPONENTS_ADBLOCK_PROFILE_POLICY_H_

#include <map>
#include <set>
#include <string>
#include <string_view>

#include "fireball/browser/domain_model.h"

namespace fireball::adblock {

enum class BlockingMode {
  kDisabled,
  kStandard,
  kAggressive,
};

// Profile is the privacy boundary. Site exemptions and the selected blocking
// level never follow a Space into another Profile.
class ProfilePolicy final {
 public:
  bool AddProfile(browser::ProfileId profile_id,
                  BlockingMode initial_mode = BlockingMode::kStandard);
  bool RemoveProfile(const browser::ProfileId& profile_id);
  bool SetMode(const browser::ProfileId& profile_id, BlockingMode mode);
  bool SetSiteExemption(const browser::ProfileId& profile_id,
                        std::string hostname,
                        bool exempt);

  BlockingMode GetMode(const browser::ProfileId& profile_id) const;
  bool ShouldEvaluate(const browser::ProfileId& profile_id,
                      std::string_view source_hostname) const;

 private:
  struct Entry {
    BlockingMode mode;
    std::set<std::string> exemptions;
  };

  std::map<browser::ProfileId, Entry> profiles_;
};

bool IsCanonicalHostname(std::string_view hostname);

}  // namespace fireball::adblock

#endif  // FIREBALL_COMPONENTS_ADBLOCK_PROFILE_POLICY_H_
