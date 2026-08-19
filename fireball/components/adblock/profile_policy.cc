#include "fireball/components/adblock/profile_policy.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

namespace fireball::adblock {
namespace {

std::string LowerAscii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](char character) {
    return static_cast<char>(
        std::tolower(static_cast<unsigned char>(character)));
  });
  return value;
}

}  // namespace

bool IsCanonicalHostname(std::string_view hostname) {
  if (hostname.empty() || hostname.size() > 253 || hostname.front() == '.' ||
      hostname.back() == '.' || hostname.find("..") != std::string_view::npos) {
    return false;
  }
  std::size_t label_length = 0;
  for (const char character : hostname) {
    if (character == '.') {
      if (label_length == 0 || label_length > 63) {
        return false;
      }
      label_length = 0;
      continue;
    }
    if (!((character >= 'a' && character <= 'z') ||
          (character >= '0' && character <= '9') || character == '-')) {
      return false;
    }
    ++label_length;
  }
  return label_length > 0 && label_length <= 63;
}

bool ProfilePolicy::AddProfile(browser::ProfileId profile_id,
                               BlockingMode initial_mode) {
  return profiles_.emplace(std::move(profile_id), Entry{initial_mode, {}})
      .second;
}

bool ProfilePolicy::RemoveProfile(const browser::ProfileId& profile_id) {
  return profiles_.erase(profile_id) == 1;
}

bool ProfilePolicy::SetMode(const browser::ProfileId& profile_id,
                            BlockingMode mode) {
  auto profile = profiles_.find(profile_id);
  if (profile == profiles_.end()) {
    return false;
  }
  profile->second.mode = mode;
  return true;
}

bool ProfilePolicy::SetSiteExemption(const browser::ProfileId& profile_id,
                                     std::string hostname,
                                     bool exempt) {
  auto profile = profiles_.find(profile_id);
  hostname = LowerAscii(std::move(hostname));
  if (profile == profiles_.end() || !IsCanonicalHostname(hostname)) {
    return false;
  }
  if (exempt) {
    profile->second.exemptions.insert(std::move(hostname));
  } else {
    profile->second.exemptions.erase(hostname);
  }
  return true;
}

BlockingMode ProfilePolicy::GetMode(
    const browser::ProfileId& profile_id) const {
  const auto profile = profiles_.find(profile_id);
  return profile == profiles_.end() ? BlockingMode::kDisabled
                                    : profile->second.mode;
}

bool ProfilePolicy::ShouldEvaluate(
    const browser::ProfileId& profile_id,
    std::string_view source_hostname) const {
  const auto profile = profiles_.find(profile_id);
  if (profile == profiles_.end() ||
      profile->second.mode == BlockingMode::kDisabled ||
      !IsCanonicalHostname(source_hostname)) {
    return false;
  }
  return !profile->second.exemptions.contains(std::string(source_hostname));
}

}  // namespace fireball::adblock
