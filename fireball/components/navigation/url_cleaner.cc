#include "fireball/components/navigation/url_cleaner.h"

#include <arpa/inet.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <utility>

#include "fireball/components/adblock/profile_policy.h"
#include "fireball/components/navigation/generated_url_cleaner_rules.h"

namespace fireball::navigation {
namespace {

constexpr std::size_t kMaximumTrackingParameters = 128;
constexpr std::size_t kMaximumQueryParameters = 512;

struct UrlParts {
  std::string hostname;
  std::size_t query_start = std::string_view::npos;
  std::size_t query_end = std::string_view::npos;
};

bool IsHex(char character) {
  return (character >= '0' && character <= '9') ||
         (character >= 'a' && character <= 'f') ||
         (character >= 'A' && character <= 'F');
}

std::uint8_t HexValue(char character) {
  if (character >= '0' && character <= '9') {
    return static_cast<std::uint8_t>(character - '0');
  }
  return static_cast<std::uint8_t>(
      std::tolower(static_cast<unsigned char>(character)) - 'a' + 10);
}

std::string LowerAscii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](char character) {
    return static_cast<char>(
        std::tolower(static_cast<unsigned char>(character)));
  });
  return value;
}

bool IsRulesVersion(std::string_view version) {
  if (version.empty() || version.size() > 32 || version.front() == '.' ||
      version.back() == '.') {
    return false;
  }
  bool previous_dot = false;
  for (char character : version) {
    if (character == '.') {
      if (previous_dot) {
        return false;
      }
      previous_dot = true;
    } else if (character >= '0' && character <= '9') {
      previous_dot = false;
    } else {
      return false;
    }
  }
  return true;
}

bool IsTrackingParameterName(std::string_view name) {
  return !name.empty() && name.size() <= 64 &&
         std::all_of(name.begin(), name.end(), [](char character) {
           return (character >= 'a' && character <= 'z') ||
                  (character >= '0' && character <= '9') ||
                  character == '_' || character == '-' || character == '.';
         });
}

bool IsCanonicalIpLiteral(std::string_view hostname) {
  if (hostname.empty() || hostname.find(':') == std::string_view::npos) {
    return false;
  }
  in6_addr address {};
  return inet_pton(AF_INET6, std::string(hostname).c_str(), &address) == 1;
}

std::optional<UrlParts> ParseUrl(std::string_view url) {
  if (url.empty() || url.size() > kMaximumNavigationUrlBytes ||
      std::any_of(url.begin(), url.end(), [](char character) {
        const unsigned char value = static_cast<unsigned char>(character);
        return value <= 0x20 || value == 0x7f || character == '\\';
      })) {
    return std::nullopt;
  }
  std::size_t authority_start = 0;
  if (url.starts_with("https://")) {
    authority_start = 8;
  } else if (url.starts_with("http://")) {
    authority_start = 7;
  } else {
    return std::nullopt;
  }
  const std::size_t authority_end =
      url.find_first_of("/?#", authority_start);
  const std::string_view authority = url.substr(
      authority_start, authority_end == std::string_view::npos
                           ? std::string_view::npos
                           : authority_end - authority_start);
  if (authority.empty() || authority.find('@') != std::string_view::npos) {
    return std::nullopt;
  }

  std::string_view hostname;
  std::string_view port;
  if (authority.front() == '[') {
    const std::size_t closing = authority.find(']');
    if (closing == std::string_view::npos || closing == 1 ||
        (closing + 1 < authority.size() && authority[closing + 1] != ':')) {
      return std::nullopt;
    }
    hostname = authority.substr(1, closing - 1);
    if (!IsCanonicalIpLiteral(hostname)) {
      return std::nullopt;
    }
    if (closing + 1 < authority.size()) {
      port = authority.substr(closing + 2);
    }
  } else {
    const std::size_t colon = authority.rfind(':');
    if (colon != std::string_view::npos) {
      if (colon == 0 || authority.find(':') != colon) {
        return std::nullopt;
      }
      hostname = authority.substr(0, colon);
      port = authority.substr(colon + 1);
    } else {
      hostname = authority;
    }
    if (!adblock::IsCanonicalHostname(LowerAscii(std::string(hostname)))) {
      return std::nullopt;
    }
  }
  if (!port.empty()) {
    std::uint32_t numeric_port = 0;
    for (char digit : port) {
      if (digit < '0' || digit > '9') {
        return std::nullopt;
      }
      numeric_port = numeric_port * 10 + static_cast<std::uint32_t>(digit - '0');
      if (numeric_port > 65535) {
        return std::nullopt;
      }
    }
    if (numeric_port == 0) {
      return std::nullopt;
    }
  } else if (authority.back() == ':') {
    return std::nullopt;
  }

  UrlParts result;
  result.hostname = LowerAscii(std::string(hostname));
  const std::size_t fragment = url.find('#', authority_end);
  result.query_start = url.find('?', authority_end);
  if (result.query_start != std::string_view::npos &&
      fragment != std::string_view::npos && result.query_start > fragment) {
    result.query_start = std::string_view::npos;
  }
  result.query_end = fragment == std::string_view::npos ? url.size() : fragment;
  return result;
}

std::optional<std::string> DecodeParameterName(std::string_view raw) {
  if (raw.empty() || raw.size() > 256) {
    return std::nullopt;
  }
  std::string decoded;
  decoded.reserve(raw.size());
  for (std::size_t index = 0; index < raw.size(); ++index) {
    unsigned char value = static_cast<unsigned char>(raw[index]);
    if (raw[index] == '%') {
      if (index + 2 >= raw.size() || !IsHex(raw[index + 1]) ||
          !IsHex(raw[index + 2])) {
        return std::nullopt;
      }
      value = static_cast<unsigned char>(HexValue(raw[index + 1]) * 16 +
                                         HexValue(raw[index + 2]));
      index += 2;
    }
    if (!((value >= 'a' && value <= 'z') ||
          (value >= 'A' && value <= 'Z') ||
          (value >= '0' && value <= '9') || value == '_' || value == '-' ||
          value == '.')) {
      return std::nullopt;
    }
    decoded.push_back(static_cast<char>(
        std::tolower(static_cast<unsigned char>(value))));
  }
  return decoded;
}

}  // namespace

std::optional<std::string> ExtractNavigationHostname(std::string_view url) {
  auto parsed = ParseUrl(url);
  return parsed.has_value()
             ? std::optional<std::string>(std::move(parsed->hostname))
             : std::nullopt;
}

bool IsCanonicalNavigationHostname(std::string_view hostname) {
  return adblock::IsCanonicalHostname(hostname) ||
         IsCanonicalIpLiteral(hostname);
}

bool IsSafeNavigationUrlForHostname(std::string_view url,
                                    std::string_view hostname) {
  auto parsed = ParseUrl(url);
  return parsed.has_value() && parsed->hostname == hostname;
}

UrlCleaner::UrlCleaner(
    std::string rules_version,
    std::set<std::string, std::less<>> tracking_parameters)
    : rules_version_(std::move(rules_version)),
      tracking_parameters_(std::move(tracking_parameters)) {}

std::optional<UrlCleaner> UrlCleaner::Create(
    std::string rules_version,
    std::vector<std::string> tracking_parameters) {
  if (!IsRulesVersion(rules_version) || tracking_parameters.empty() ||
      tracking_parameters.size() > kMaximumTrackingParameters) {
    return std::nullopt;
  }
  std::set<std::string, std::less<>> normalized;
  for (std::string parameter : tracking_parameters) {
    parameter = LowerAscii(std::move(parameter));
    if (!IsTrackingParameterName(parameter) ||
        !normalized.insert(std::move(parameter)).second) {
      return std::nullopt;
    }
  }
  return UrlCleaner(std::move(rules_version), std::move(normalized));
}

UrlCleaner UrlCleaner::CreateBuiltIn() {
  std::vector<std::string> parameters;
  parameters.reserve(generated::kUrlCleanerTrackingParameters.size());
  for (const std::string_view parameter :
       generated::kUrlCleanerTrackingParameters) {
    parameters.emplace_back(parameter);
  }
  auto cleaner = Create(std::string(generated::kUrlCleanerRulesVersion),
                        std::move(parameters));
  return std::move(*cleaner);
}

bool UrlCleaner::AddProfile(browser::ProfileId profile_id) {
  return profiles_.emplace(std::move(profile_id), ProfileEntry{}).second;
}

bool UrlCleaner::RemoveProfile(const browser::ProfileId& profile_id) {
  return profiles_.erase(profile_id) == 1;
}

bool UrlCleaner::HasProfile(const browser::ProfileId& profile_id) const {
  return profiles_.contains(profile_id);
}

bool UrlCleaner::SetEnabled(const browser::ProfileId& profile_id,
                            bool enabled) {
  auto profile = profiles_.find(profile_id);
  if (profile == profiles_.end()) {
    return false;
  }
  profile->second.enabled = enabled;
  return true;
}

bool UrlCleaner::SetSiteExemption(const browser::ProfileId& profile_id,
                                  std::string hostname,
                                  bool exempt) {
  auto profile = profiles_.find(profile_id);
  hostname = LowerAscii(std::move(hostname));
  if (profile == profiles_.end() ||
      !adblock::IsCanonicalHostname(hostname)) {
    return false;
  }
  if (exempt) {
    profile->second.exemptions.insert(std::move(hostname));
  } else {
    profile->second.exemptions.erase(hostname);
  }
  return true;
}

UrlCleanResult UrlCleaner::Clean(const browser::ProfileId& profile_id,
                                 std::string_view url,
                                 std::string_view hostname) const {
  const auto profile = profiles_.find(profile_id);
  auto parsed = ParseUrl(url);
  if (profile == profiles_.end() || !parsed.has_value() ||
      parsed->hostname != hostname) {
    return {};
  }
  if (!profile->second.enabled ||
      profile->second.exemptions.contains(std::string(hostname)) ||
      parsed->query_start == std::string_view::npos) {
    return {UrlCleanStatus::kUnchanged, std::string(url), 0};
  }

  const std::string_view query = url.substr(
      parsed->query_start + 1, parsed->query_end - parsed->query_start - 1);
  std::vector<std::string_view> kept;
  kept.reserve(16);
  std::size_t removed = 0;
  std::size_t parameter_count = 0;
  std::string_view remaining = query;
  while (true) {
    if (++parameter_count > kMaximumQueryParameters) {
      return {UrlCleanStatus::kUnchanged, std::string(url), 0};
    }
    const std::size_t separator = remaining.find('&');
    const std::string_view parameter = remaining.substr(0, separator);
    const std::string_view raw_name = parameter.substr(0, parameter.find('='));
    const auto decoded_name = DecodeParameterName(raw_name);
    if (decoded_name.has_value() &&
        tracking_parameters_.contains(*decoded_name)) {
      ++removed;
    } else {
      kept.push_back(parameter);
    }
    if (separator == std::string_view::npos) {
      break;
    }
    remaining.remove_prefix(separator + 1);
  }
  if (removed == 0) {
    return {UrlCleanStatus::kUnchanged, std::string(url), 0};
  }

  std::string cleaned(url.substr(0, parsed->query_start));
  if (!kept.empty()) {
    cleaned.push_back('?');
    for (std::size_t index = 0; index < kept.size(); ++index) {
      if (index != 0) {
        cleaned.push_back('&');
      }
      cleaned.append(kept[index]);
    }
  }
  cleaned.append(url.substr(parsed->query_end));
  return {UrlCleanStatus::kCleaned, std::move(cleaned), removed};
}

}  // namespace fireball::navigation
