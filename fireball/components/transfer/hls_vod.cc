#include "fireball/components/transfer/hls_vod.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <cctype>
#include <cstdio>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <utility>

namespace fireball::transfer {
namespace {

constexpr std::string_view kHeader = "#EXTM3U";

struct ScopedFd {
  explicit ScopedFd(int value = -1) : value(value) {}
  ~ScopedFd() {
    if (value >= 0) {
      close(value);
    }
  }
  ScopedFd(const ScopedFd&) = delete;
  ScopedFd& operator=(const ScopedFd&) = delete;
  int value;
};

bool IsSafeManifestText(std::string_view body) {
  if (body.empty() || body.size() > kMaximumHlsManifestBytes) {
    return false;
  }
  return std::none_of(body.begin(), body.end(), [](char character) {
    const unsigned char value = static_cast<unsigned char>(character);
    return value == 0 || (value < 0x20 && value != '\n' && value != '\r' &&
                          value != '\t') ||
           value == 0x7f;
  });
}

std::vector<std::string_view> Lines(std::string_view body) {
  std::vector<std::string_view> lines;
  while (!body.empty()) {
    const std::size_t newline = body.find('\n');
    std::string_view line = body.substr(0, newline);
    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }
    lines.push_back(line);
    if (newline == std::string_view::npos) {
      break;
    }
    body.remove_prefix(newline + 1);
  }
  return lines;
}

bool HasHeader(const std::vector<std::string_view>& lines) {
  return !lines.empty() && lines.front() == kHeader;
}

bool IsAttributeName(std::string_view name) {
  return !name.empty() &&
         std::all_of(name.begin(), name.end(), [](char character) {
           return (character >= 'A' && character <= 'Z') ||
                  (character >= '0' && character <= '9') || character == '-';
         });
}

std::optional<std::map<std::string, std::string, std::less<>>>
ParseAttributes(std::string_view input) {
  std::map<std::string, std::string, std::less<>> result;
  while (!input.empty()) {
    const std::size_t equals = input.find('=');
    if (equals == std::string_view::npos || equals == 0) {
      return std::nullopt;
    }
    const std::string_view name = input.substr(0, equals);
    if (!IsAttributeName(name)) {
      return std::nullopt;
    }
    input.remove_prefix(equals + 1);

    std::string value;
    if (!input.empty() && input.front() == '"') {
      input.remove_prefix(1);
      bool closed = false;
      while (!input.empty()) {
        const char character = input.front();
        input.remove_prefix(1);
        if (character == '"') {
          closed = true;
          break;
        }
        if (character == '\r' || character == '\n' || character == '\\') {
          return std::nullopt;
        }
        value.push_back(character);
      }
      if (!closed) {
        return std::nullopt;
      }
    } else {
      const std::size_t comma = input.find(',');
      const std::string_view raw = input.substr(0, comma);
      if (raw.empty() || raw.find_first_of(" \t\r\n\"") !=
                             std::string_view::npos) {
        return std::nullopt;
      }
      value.assign(raw);
      input.remove_prefix(comma == std::string_view::npos ? input.size()
                                                          : comma);
    }
    if (!result.emplace(std::string(name), std::move(value)).second) {
      return std::nullopt;
    }
    if (input.empty()) {
      break;
    }
    if (input.front() != ',') {
      return std::nullopt;
    }
    input.remove_prefix(1);
    if (input.empty()) {
      return std::nullopt;
    }
  }
  return result;
}

template <typename T>
std::optional<T> ParseUnsigned(std::string_view value) {
  if (value.empty()) {
    return std::nullopt;
  }
  T result = 0;
  const auto parsed =
      std::from_chars(value.data(), value.data() + value.size(), result);
  if (parsed.ec != std::errc() || parsed.ptr != value.data() + value.size()) {
    return std::nullopt;
  }
  return result;
}

std::optional<std::uint64_t> ParseDurationMs(std::string_view value) {
  if (value.empty() || value.front() == '-' || value.front() == '+') {
    return std::nullopt;
  }
  const std::size_t dot = value.find('.');
  const std::string_view seconds = value.substr(0, dot);
  if (seconds.empty() || (dot != std::string_view::npos &&
                          value.find('.', dot + 1) != std::string_view::npos)) {
    return std::nullopt;
  }
  const auto whole = ParseUnsigned<std::uint64_t>(seconds);
  if (!whole.has_value() || *whole > 60 * 60) {
    return std::nullopt;
  }
  std::uint64_t milliseconds = *whole * 1000;
  if (dot != std::string_view::npos) {
    const std::string_view fraction = value.substr(dot + 1);
    if (fraction.empty() || fraction.size() > 9 ||
        !std::all_of(fraction.begin(), fraction.end(), [](char character) {
          return character >= '0' && character <= '9';
        })) {
      return std::nullopt;
    }
    std::uint64_t numerator = 0;
    for (char digit : fraction) {
      numerator = numerator * 10 + static_cast<std::uint64_t>(digit - '0');
    }
    std::uint64_t denominator = 1;
    for (std::size_t index = 0; index < fraction.size(); ++index) {
      denominator *= 10;
    }
    milliseconds += (numerator * 1000 + denominator - 1) / denominator;
  }
  return milliseconds == 0 ? std::nullopt
                           : std::optional<std::uint64_t>(milliseconds);
}

void RemoveLastPathSegment(std::string* output) {
  const std::size_t slash = output->rfind('/');
  if (slash == std::string::npos) {
    output->clear();
  } else {
    output->erase(slash);
  }
}

std::string RemoveDotSegments(std::string input) {
  std::string output;
  output.reserve(input.size());
  while (!input.empty()) {
    if (input.starts_with("../")) {
      input.erase(0, 3);
    } else if (input.starts_with("./")) {
      input.erase(0, 2);
    } else if (input.starts_with("/./")) {
      input.erase(0, 2);
    } else if (input == "/.") {
      input = "/";
    } else if (input.starts_with("/../")) {
      input.erase(0, 3);
      RemoveLastPathSegment(&output);
    } else if (input == "/..") {
      input = "/";
      RemoveLastPathSegment(&output);
    } else if (input == "." || input == "..") {
      input.clear();
    } else {
      const std::size_t next =
          input.front() == '/' ? input.find('/', 1) : input.find('/');
      const std::size_t length =
          next == std::string::npos ? input.size() : next;
      output.append(input, 0, length);
      input.erase(0, length);
    }
  }
  return output;
}

std::optional<std::string> ResolveUri(std::string_view base,
                                      std::string_view reference) {
  if (!IsSafeHttpDownloadUri(base) || reference.empty() ||
      reference.size() > kMaximumUriBytes || reference.front() == ' ' ||
      reference.back() == ' ' || reference.find_first_of("\r\n\t\\#") !=
                                   std::string_view::npos) {
    return std::nullopt;
  }
  if (reference.starts_with("http://") || reference.starts_with("https://")) {
    return IsSafeHttpDownloadUri(reference)
               ? std::optional<std::string>(reference)
               : std::nullopt;
  }

  const std::size_t scheme_end = base.find("://");
  const std::size_t authority_start = scheme_end + 3;
  const std::size_t authority_end = base.find_first_of("/?#", authority_start);
  const std::string_view origin =
      base.substr(0, authority_end == std::string_view::npos
                         ? base.size()
                         : authority_end);
  const std::string_view scheme = base.substr(0, scheme_end);
  if (reference.starts_with("//")) {
    const std::string result = std::string(scheme) + ":" +
                               std::string(reference);
    return IsSafeHttpDownloadUri(result)
               ? std::optional<std::string>(result)
               : std::nullopt;
  }

  const std::size_t query = reference.find('?');
  std::string_view reference_path = reference.substr(0, query);
  const std::string_view reference_query =
      query == std::string_view::npos ? std::string_view()
                                      : reference.substr(query);
  if (reference_path.empty()) {
    return std::nullopt;
  }

  std::string combined;
  if (reference_path.front() == '/') {
    combined.assign(reference_path);
  } else {
    std::string_view base_path = authority_end == std::string_view::npos
                                     ? std::string_view("/")
                                     : base.substr(authority_end);
    base_path = base_path.substr(0, base_path.find_first_of("?#"));
    const std::size_t slash = base_path.rfind('/');
    combined = std::string(base_path.substr(0, slash + 1));
    combined += reference_path;
  }

  std::string normalized = RemoveDotSegments(std::move(combined));
  if (normalized.empty() || normalized.front() != '/') {
    normalized.insert(normalized.begin(), '/');
  }
  const std::string result = std::string(origin) + normalized +
                             std::string(reference_query);
  return IsSafeHttpDownloadUri(result) ? std::optional<std::string>(result)
                                       : std::nullopt;
}

bool IsMpegTsUri(std::string_view uri) {
  const std::size_t query = uri.find_first_of("?#");
  std::string path(uri.substr(0, query));
  std::transform(path.begin(), path.end(), path.begin(), [](char character) {
    return static_cast<char>(
        std::tolower(static_cast<unsigned char>(character)));
  });
  return path.ends_with(".ts");
}

std::optional<std::pair<std::uint32_t, std::uint32_t>> ParseResolution(
    std::string_view value) {
  const std::size_t separator = value.find('x');
  if (separator == std::string_view::npos ||
      value.find('x', separator + 1) != std::string_view::npos) {
    return std::nullopt;
  }
  auto width = ParseUnsigned<std::uint32_t>(value.substr(0, separator));
  auto height = ParseUnsigned<std::uint32_t>(value.substr(separator + 1));
  if (!width.has_value() || !height.has_value() || *width == 0 || *height == 0 ||
      *width > 16384 || *height > 16384) {
    return std::nullopt;
  }
  return std::pair(*width, *height);
}

std::string SegmentFilename(std::string_view id, std::size_t index) {
  std::array<char, 32> suffix{};
  std::snprintf(suffix.data(), suffix.size(), "-%06zu.ts", index);
  return ".fireball-hls-" + std::string(id) + suffix.data();
}

bool IsTerminalJob(HlsVodJobState state) {
  return state == HlsVodJobState::kComplete ||
         state == HlsVodJobState::kFailed ||
         state == HlsVodJobState::kCancelled;
}

bool IsSafeDownloadDirectory(const std::filesystem::path& path) {
  if (!path.is_absolute()) {
    return false;
  }
  struct stat status {};
  return lstat(path.c_str(), &status) == 0 && S_ISDIR(status.st_mode) &&
         status.st_uid == getuid() && (status.st_mode & 0022) == 0;
}

bool DoesNotExistAt(int directory, std::string_view name) {
  struct stat status {};
  if (fstatat(directory, std::string(name).c_str(), &status,
              AT_SYMLINK_NOFOLLOW) == 0) {
    return false;
  }
  return errno == ENOENT;
}

bool UnlinkIfPresent(int directory, std::string_view name) {
  const std::string owned_name(name);
  while (unlinkat(directory, owned_name.c_str(), 0) != 0) {
    if (errno == EINTR) {
      continue;
    }
    return errno == ENOENT;
  }
  return true;
}

bool RemoveSegmentArtifacts(int directory, std::string_view filename) {
  const bool data_removed = UnlinkIfPresent(directory, filename);
  const bool control_removed =
      UnlinkIfPresent(directory, std::string(filename) + ".aria2");
  return data_removed && control_removed;
}

bool WriteAll(int descriptor, const char* bytes, std::size_t size) {
  std::size_t written = 0;
  while (written < size) {
    const ssize_t count = write(descriptor, bytes + written, size - written);
    if (count < 0 && errno == EINTR) {
      continue;
    }
    if (count <= 0) {
      return false;
    }
    written += static_cast<std::size_t>(count);
  }
  return true;
}

}  // namespace

std::string_view HlsVodErrorCode(HlsVodError error) {
  switch (error) {
    case HlsVodError::kNone:
      return "HLS_OK";
    case HlsVodError::kInvalidManifest:
      return "HLS_INVALID_MANIFEST";
    case HlsVodError::kUnsupportedManifest:
      return "HLS_UNSUPPORTED_MANIFEST";
    case HlsVodError::kUnsafeUri:
      return "HLS_UNSAFE_URI";
    case HlsVodError::kLimitExceeded:
      return "HLS_LIMIT_EXCEEDED";
  }
  return "HLS_INVALID_MANIFEST";
}

HlsParseResult<HlsMasterPlaylist> ParseHlsMasterPlaylist(
    std::string_view manifest_uri,
    std::string_view manifest_body) {
  if (!IsSafeHttpDownloadUri(manifest_uri) ||
      manifest_uri.find('#') != std::string_view::npos ||
      !IsSafeManifestText(manifest_body)) {
    return {std::nullopt, HlsVodError::kInvalidManifest};
  }
  const auto lines = Lines(manifest_body);
  if (!HasHeader(lines)) {
    return {std::nullopt, HlsVodError::kInvalidManifest};
  }

  HlsMasterPlaylist playlist;
  std::optional<std::map<std::string, std::string, std::less<>>> pending;
  std::set<std::string, std::less<>> seen;
  for (std::size_t index = 1; index < lines.size(); ++index) {
    const std::string_view line = lines[index];
    if (line.empty()) {
      continue;
    }
    if (line.starts_with("#EXTINF:")) {
      return {std::nullopt, HlsVodError::kUnsupportedManifest};
    }
    if (line.starts_with("#EXT-X-STREAM-INF:")) {
      if (pending.has_value()) {
        return {std::nullopt, HlsVodError::kInvalidManifest};
      }
      pending = ParseAttributes(line.substr(18));
      if (!pending.has_value()) {
        return {std::nullopt, HlsVodError::kInvalidManifest};
      }
      continue;
    }
    if (line.front() == '#') {
      if (line.starts_with("#EXT-X-") &&
          !line.starts_with("#EXT-X-VERSION:") &&
          line != "#EXT-X-INDEPENDENT-SEGMENTS") {
        return {std::nullopt, HlsVodError::kUnsupportedManifest};
      }
      continue;
    }
    if (!pending.has_value()) {
      return {std::nullopt, HlsVodError::kInvalidManifest};
    }
    const auto bandwidth_entry = pending->find("BANDWIDTH");
    if (bandwidth_entry == pending->end()) {
      return {std::nullopt, HlsVodError::kInvalidManifest};
    }
    const auto bandwidth =
        ParseUnsigned<std::uint64_t>(bandwidth_entry->second);
    auto uri = ResolveUri(manifest_uri, line);
    if (!bandwidth.has_value() || *bandwidth == 0 ||
        *bandwidth > 10'000'000'000ULL || !uri.has_value()) {
      return {std::nullopt, uri.has_value() ? HlsVodError::kInvalidManifest
                                            : HlsVodError::kUnsafeUri};
    }
    HlsVariant variant;
    variant.uri = std::move(*uri);
    variant.bandwidth = *bandwidth;
    const auto resolution_entry = pending->find("RESOLUTION");
    if (resolution_entry != pending->end()) {
      auto resolution = ParseResolution(resolution_entry->second);
      if (!resolution.has_value()) {
        return {std::nullopt, HlsVodError::kInvalidManifest};
      }
      variant.width = resolution->first;
      variant.height = resolution->second;
    }
    if (!seen.insert(variant.uri).second) {
      return {std::nullopt, HlsVodError::kInvalidManifest};
    }
    playlist.variants.push_back(std::move(variant));
    pending.reset();
    if (playlist.variants.size() > kMaximumHlsVariants) {
      return {std::nullopt, HlsVodError::kLimitExceeded};
    }
  }
  if (pending.has_value() || playlist.variants.empty()) {
    return {std::nullopt, HlsVodError::kInvalidManifest};
  }
  return {std::move(playlist), HlsVodError::kNone};
}

std::optional<HlsVariant> SelectHlsVariant(
    const HlsMasterPlaylist& playlist,
    std::uint64_t maximum_bandwidth) {
  if (playlist.variants.empty()) {
    return std::nullopt;
  }
  const HlsVariant* lowest = &playlist.variants.front();
  const HlsVariant* selected = nullptr;
  for (const HlsVariant& variant : playlist.variants) {
    if (variant.bandwidth < lowest->bandwidth) {
      lowest = &variant;
    }
    if ((maximum_bandwidth == 0 || variant.bandwidth <= maximum_bandwidth) &&
        (selected == nullptr || variant.bandwidth > selected->bandwidth)) {
      selected = &variant;
    }
  }
  return selected == nullptr ? std::optional<HlsVariant>(*lowest)
                             : std::optional<HlsVariant>(*selected);
}

HlsParseResult<HlsVodPlan> ParseHlsVodPlaylist(
    std::string_view manifest_uri,
    std::string_view manifest_body) {
  if (!IsSafeHttpDownloadUri(manifest_uri) ||
      manifest_uri.find('#') != std::string_view::npos ||
      !IsSafeManifestText(manifest_body)) {
    return {std::nullopt, HlsVodError::kInvalidManifest};
  }
  const auto lines = Lines(manifest_body);
  if (!HasHeader(lines)) {
    return {std::nullopt, HlsVodError::kInvalidManifest};
  }

  HlsVodPlan plan;
  std::optional<std::uint64_t> pending_duration;
  bool end_list = false;
  for (std::size_t index = 1; index < lines.size(); ++index) {
    const std::string_view line = lines[index];
    if (line.empty()) {
      continue;
    }
    if (line.starts_with("#EXT-X-STREAM-INF:") ||
        line.starts_with("#EXT-X-I-FRAME-STREAM-INF:") ||
        line.starts_with("#EXT-X-MEDIA:") ||
        line.starts_with("#EXT-X-KEY:") ||
        line.starts_with("#EXT-X-MAP:") ||
        line.starts_with("#EXT-X-BYTERANGE:") ||
        line == "#EXT-X-DISCONTINUITY" || line == "#EXT-X-GAP" ||
        line.starts_with("#EXT-X-PART:") ||
        line.starts_with("#EXT-X-PRELOAD-HINT:") ||
        line.starts_with("#EXT-X-SERVER-CONTROL:")) {
      return {std::nullopt, HlsVodError::kUnsupportedManifest};
    }
    if (line == "#EXT-X-ENDLIST") {
      if (pending_duration.has_value() || end_list) {
        return {std::nullopt, HlsVodError::kInvalidManifest};
      }
      end_list = true;
      continue;
    }
    if (line.starts_with("#EXT-X-PLAYLIST-TYPE:")) {
      if (line != "#EXT-X-PLAYLIST-TYPE:VOD") {
        return {std::nullopt, HlsVodError::kUnsupportedManifest};
      }
      continue;
    }
    if (line.starts_with("#EXTINF:")) {
      if (pending_duration.has_value() || end_list) {
        return {std::nullopt, HlsVodError::kInvalidManifest};
      }
      std::string_view duration = line.substr(8);
      duration = duration.substr(0, duration.find(','));
      pending_duration = ParseDurationMs(duration);
      if (!pending_duration.has_value()) {
        return {std::nullopt, HlsVodError::kInvalidManifest};
      }
      continue;
    }
    if (line.front() == '#') {
      if (line.starts_with("#EXT-X-") &&
          !line.starts_with("#EXT-X-VERSION:") &&
          !line.starts_with("#EXT-X-TARGETDURATION:") &&
          !line.starts_with("#EXT-X-MEDIA-SEQUENCE:") &&
          line != "#EXT-X-INDEPENDENT-SEGMENTS") {
        return {std::nullopt, HlsVodError::kUnsupportedManifest};
      }
      continue;
    }
    if (!pending_duration.has_value() || end_list) {
      return {std::nullopt, HlsVodError::kInvalidManifest};
    }
    auto uri = ResolveUri(manifest_uri, line);
    if (!uri.has_value()) {
      return {std::nullopt, HlsVodError::kUnsafeUri};
    }
    if (!IsMpegTsUri(*uri)) {
      return {std::nullopt, HlsVodError::kUnsupportedManifest};
    }
    if (plan.total_duration_ms >
        kMaximumHlsDurationMs - *pending_duration) {
      return {std::nullopt, HlsVodError::kLimitExceeded};
    }
    plan.total_duration_ms += *pending_duration;
    plan.segments.push_back({std::move(*uri), *pending_duration});
    pending_duration.reset();
    if (plan.segments.size() > kMaximumHlsSegments) {
      return {std::nullopt, HlsVodError::kLimitExceeded};
    }
  }
  if (!end_list || pending_duration.has_value() || plan.segments.empty()) {
    return {std::nullopt, end_list ? HlsVodError::kInvalidManifest
                                   : HlsVodError::kUnsupportedManifest};
  }
  return {std::move(plan), HlsVodError::kNone};
}

HlsVodSession::HlsVodSession(TransferBackend* backend,
                             TransferPersistence persistence,
                             std::string id,
                             std::filesystem::path download_directory)
    : backend_(backend),
      persistence_(persistence),
      download_directory_(std::move(download_directory)) {
  snapshot_.id = std::move(id);
}

HlsVodSession::~HlsVodSession() {
  if (!IsTerminalJob(snapshot_.state) &&
      snapshot_.state != HlsVodJobState::kIdle) {
    BestEffortStopAndClean();
  }
}

bool HlsVodSession::ValidateStart(const HlsVodPlan& plan,
                                  std::string_view output_name) const {
  if (backend_ == nullptr || snapshot_.state != HlsVodJobState::kIdle ||
      !IsCanonicalTransferId(snapshot_.id) ||
      !IsSafeDownloadDirectory(download_directory_) ||
      !IsSafeOutputName(output_name) || !IsMpegTsUri(output_name) ||
      plan.segments.empty() || plan.segments.size() > kMaximumHlsSegments ||
      plan.total_duration_ms == 0 ||
      plan.total_duration_ms > kMaximumHlsDurationMs) {
    return false;
  }
  std::uint64_t duration = 0;
  for (const HlsVodSegment& segment : plan.segments) {
    if (!IsSafeHttpDownloadUri(segment.uri) || !IsMpegTsUri(segment.uri) ||
        segment.duration_ms == 0 ||
        duration > kMaximumHlsDurationMs - segment.duration_ms) {
      return false;
    }
    duration += segment.duration_ms;
  }
  return duration == plan.total_duration_ms;
}

bool HlsVodSession::Start(HlsVodPlan plan, std::string output_name) {
  if (!ValidateStart(plan, output_name)) {
    SetFailure("HLS_INVALID_JOB");
    return false;
  }
  ScopedFd directory(open(download_directory_.c_str(),
                          O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
  if (directory.value < 0 || !DoesNotExistAt(directory.value, output_name) ||
      !DoesNotExistAt(directory.value,
                      ".fireball-hls-" + snapshot_.id + ".partial")) {
    SetFailure("HLS_OUTPUT_CONFLICT");
    return false;
  }

  snapshot_.output_name = std::move(output_name);
  snapshot_.segment_count = plan.segments.size();
  segments_.reserve(plan.segments.size());
  std::set<std::string, std::less<>> gids;
  for (std::size_t index = 0; index < plan.segments.size(); ++index) {
    const std::string filename = SegmentFilename(snapshot_.id, index);
    if (!DoesNotExistAt(directory.value, filename)) {
      BestEffortStopAndClean();
      SetFailure("HLS_SEGMENT_CONFLICT");
      return false;
    }
    TransferRequest request{TransferSourceKind::kHttp,
                            persistence_,
                            std::move(plan.segments[index].uri),
                            filename,
                            {},
                            false};
    auto result = backend_->Enqueue(request);
    std::fill(request.source.begin(), request.source.end(), '\0');
    if (!result.ok() || !IsValidAria2Gid(*result.value) ||
        !gids.insert(*result.value).second) {
      BestEffortStopAndClean();
      SetFailure("HLS_ENQUEUE_FAILED");
      return false;
    }
    segments_.push_back({filename, std::move(*result.value),
                         Aria2TransferState::kWaiting, 0, 0, 0});
  }
  snapshot_.state = HlsVodJobState::kQueued;
  snapshot_.failure_code.clear();
  return true;
}

bool HlsVodSession::Refresh() {
  if (IsTerminalJob(snapshot_.state) || snapshot_.state == HlsVodJobState::kIdle) {
    return false;
  }
  snapshot_.completed_segments = 0;
  snapshot_.total_bytes = 0;
  snapshot_.completed_bytes = 0;
  snapshot_.bytes_per_second = 0;
  bool any_active = false;
  bool all_paused = true;
  bool all_complete = true;
  for (SegmentRecord& segment : segments_) {
    if (segment.state != Aria2TransferState::kComplete) {
      auto result = backend_->TellStatus(segment.gid);
      if (!result.ok() || result.value->gid != segment.gid ||
          result.value->state == Aria2TransferState::kUnknown ||
          result.value->state == Aria2TransferState::kError ||
          result.value->state == Aria2TransferState::kRemoved ||
          (result.value->total_bytes != 0 &&
           result.value->completed_bytes > result.value->total_bytes)) {
        BestEffortStopAndClean();
        SetFailure("HLS_SEGMENT_FAILED");
        return false;
      }
      segment.state = result.value->state;
      segment.total_bytes = result.value->total_bytes;
      segment.completed_bytes = result.value->completed_bytes;
      segment.bytes_per_second = result.value->bytes_per_second;
    }
    if (segment.total_bytes >
            kMaximumHlsOutputBytes - snapshot_.total_bytes ||
        segment.completed_bytes >
            kMaximumHlsOutputBytes - snapshot_.completed_bytes ||
        segment.bytes_per_second >
            std::numeric_limits<std::uint64_t>::max() -
                snapshot_.bytes_per_second) {
      BestEffortStopAndClean();
      SetFailure("HLS_SIZE_LIMIT_EXCEEDED");
      return false;
    }
    snapshot_.total_bytes += segment.total_bytes;
    snapshot_.completed_bytes += segment.completed_bytes;
    snapshot_.bytes_per_second += segment.bytes_per_second;
    if (segment.state == Aria2TransferState::kComplete) {
      ++snapshot_.completed_segments;
    } else {
      all_complete = false;
    }
    any_active = any_active || segment.state == Aria2TransferState::kActive;
    all_paused = all_paused &&
                 (segment.state == Aria2TransferState::kPaused ||
                  segment.state == Aria2TransferState::kComplete);
  }
  if (all_complete) {
    snapshot_.state = HlsVodJobState::kAssembling;
    return AssembleAndPublish();
  }
  snapshot_.state = all_paused ? HlsVodJobState::kPaused
                               : (any_active ? HlsVodJobState::kActive
                                             : HlsVodJobState::kQueued);
  snapshot_.failure_code.clear();
  return true;
}

bool HlsVodSession::Pause() {
  if (snapshot_.state != HlsVodJobState::kQueued &&
      snapshot_.state != HlsVodJobState::kActive) {
    return false;
  }
  bool success = true;
  for (SegmentRecord& segment : segments_) {
    if (segment.state == Aria2TransferState::kWaiting ||
        segment.state == Aria2TransferState::kActive) {
      auto result = backend_->Pause(segment.gid);
      if (!result.ok() || *result.value != segment.gid) {
        success = false;
      } else {
        segment.state = Aria2TransferState::kPaused;
      }
    }
  }
  if (success) {
    snapshot_.state = HlsVodJobState::kPaused;
    snapshot_.failure_code.clear();
  } else {
    snapshot_.failure_code = "HLS_CONTROL_FAILED";
  }
  return success;
}

bool HlsVodSession::Resume() {
  if (snapshot_.state != HlsVodJobState::kPaused) {
    return false;
  }
  bool success = true;
  for (SegmentRecord& segment : segments_) {
    if (segment.state == Aria2TransferState::kPaused) {
      auto result = backend_->Unpause(segment.gid);
      if (!result.ok() || *result.value != segment.gid) {
        success = false;
      } else {
        segment.state = Aria2TransferState::kWaiting;
      }
    }
  }
  if (success) {
    snapshot_.state = HlsVodJobState::kQueued;
    snapshot_.failure_code.clear();
  } else {
    snapshot_.failure_code = "HLS_CONTROL_FAILED";
  }
  return success;
}

bool HlsVodSession::Cancel() {
  if (IsTerminalJob(snapshot_.state) || snapshot_.state == HlsVodJobState::kIdle) {
    return false;
  }
  bool success = true;
  for (SegmentRecord& segment : segments_) {
    if (segment.state == Aria2TransferState::kComplete) {
      auto result = backend_->ForgetDownloadResult(segment.gid);
      success = success && result.ok() && *result.value == "OK";
    } else {
      auto removed = backend_->Remove(segment.gid);
      const bool remove_ok =
          removed.ok() && *removed.value == segment.gid;
      auto forgotten = backend_->ForgetDownloadResult(segment.gid);
      success = success && remove_ok && forgotten.ok() &&
                *forgotten.value == "OK";
    }
  }
  ScopedFd directory(open(download_directory_.c_str(),
                          O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
  if (directory.value < 0) {
    success = false;
  } else {
    for (const SegmentRecord& segment : segments_) {
      if (!RemoveSegmentArtifacts(directory.value, segment.filename)) {
        success = false;
      }
    }
    const std::string temporary =
        ".fireball-hls-" + snapshot_.id + ".partial";
    if (unlinkat(directory.value, temporary.c_str(), 0) != 0 &&
        errno != ENOENT) {
      success = false;
    }
    if (fsync(directory.value) != 0) {
      success = false;
    }
  }
  snapshot_.bytes_per_second = 0;
  if (success) {
    snapshot_.state = HlsVodJobState::kCancelled;
    snapshot_.failure_code.clear();
  } else {
    SetFailure("HLS_CANCEL_INCOMPLETE");
  }
  return success;
}

bool HlsVodSession::AssembleAndPublish() {
  ScopedFd directory(open(download_directory_.c_str(),
                          O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
  if (directory.value < 0 ||
      !DoesNotExistAt(directory.value, snapshot_.output_name)) {
    BestEffortStopAndClean();
    SetFailure("HLS_OUTPUT_CONFLICT");
    return false;
  }
  const std::string temporary =
      ".fireball-hls-" + snapshot_.id + ".partial";
  ScopedFd output(openat(directory.value, temporary.c_str(),
                         O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
                         0600));
  if (output.value < 0) {
    BestEffortStopAndClean();
    SetFailure("HLS_ASSEMBLY_FAILED");
    return false;
  }

  std::array<char, 64 * 1024> buffer{};
  std::uint64_t assembled_bytes = 0;
  bool success = true;
  for (const SegmentRecord& segment : segments_) {
    ScopedFd input(openat(directory.value, segment.filename.c_str(),
                          O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
    struct stat status {};
    if (input.value < 0 || fstat(input.value, &status) != 0 ||
        !S_ISREG(status.st_mode) || status.st_uid != getuid() ||
        status.st_nlink != 1 || status.st_size <= 0 ||
        static_cast<std::uint64_t>(status.st_size) >
            kMaximumHlsOutputBytes - assembled_bytes) {
      success = false;
      break;
    }
    assembled_bytes += static_cast<std::uint64_t>(status.st_size);
    std::uint64_t copied_bytes = 0;
    while (true) {
      const ssize_t count = read(input.value, buffer.data(), buffer.size());
      if (count < 0 && errno == EINTR) {
        continue;
      }
      if (count < 0 ||
          (count > 0 &&
           (static_cast<std::uint64_t>(count) >
                static_cast<std::uint64_t>(status.st_size) - copied_bytes ||
            !WriteAll(output.value, buffer.data(),
                      static_cast<std::size_t>(count))))) {
        success = false;
      }
      if (count > 0 && success) {
        copied_bytes += static_cast<std::uint64_t>(count);
      }
      if (count <= 0 || !success) {
        break;
      }
    }
    if (!success || copied_bytes != static_cast<std::uint64_t>(status.st_size)) {
      success = false;
      break;
    }
  }
  success = success && fsync(output.value) == 0;
  if (!success) {
    unlinkat(directory.value, temporary.c_str(), 0);
    BestEffortStopAndClean();
    SetFailure("HLS_ASSEMBLY_FAILED");
    return false;
  }

  for (const SegmentRecord& segment : segments_) {
    auto forgotten = backend_->ForgetDownloadResult(segment.gid);
    if (!forgotten.ok() || *forgotten.value != "OK") {
      unlinkat(directory.value, temporary.c_str(), 0);
      BestEffortStopAndClean();
      SetFailure("HLS_RESULT_CLEANUP_FAILED");
      return false;
    }
  }
  for (const SegmentRecord& segment : segments_) {
    if (!RemoveSegmentArtifacts(directory.value, segment.filename)) {
      unlinkat(directory.value, temporary.c_str(), 0);
      BestEffortStopAndClean();
      SetFailure("HLS_SEGMENT_CLEANUP_FAILED");
      return false;
    }
  }
  const bool published =
      linkat(directory.value, temporary.c_str(), directory.value,
             snapshot_.output_name.c_str(), 0) == 0;
  const bool temporary_removed =
      published && UnlinkIfPresent(directory.value, temporary);
  const bool directory_synced =
      temporary_removed && fsync(directory.value) == 0;
  if (!published || !temporary_removed || !directory_synced) {
    static_cast<void>(UnlinkIfPresent(directory.value, temporary));
    if (published) {
      static_cast<void>(
          UnlinkIfPresent(directory.value, snapshot_.output_name));
      static_cast<void>(fsync(directory.value));
    }
    SetFailure("HLS_PUBLISH_FAILED");
    return false;
  }
  snapshot_.state = HlsVodJobState::kComplete;
  snapshot_.total_bytes = assembled_bytes;
  snapshot_.completed_bytes = assembled_bytes;
  snapshot_.completed_segments = segments_.size();
  snapshot_.bytes_per_second = 0;
  snapshot_.failure_code.clear();
  return true;
}

void HlsVodSession::BestEffortStopAndClean() {
  if (backend_ != nullptr) {
    for (const SegmentRecord& segment : segments_) {
      if (segment.state == Aria2TransferState::kComplete) {
        static_cast<void>(backend_->ForgetDownloadResult(segment.gid));
      } else {
        static_cast<void>(backend_->Remove(segment.gid));
        static_cast<void>(backend_->ForgetDownloadResult(segment.gid));
      }
    }
  }
  ScopedFd directory(open(download_directory_.c_str(),
                          O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
  if (directory.value >= 0) {
    for (const SegmentRecord& segment : segments_) {
      static_cast<void>(RemoveSegmentArtifacts(directory.value,
                                               segment.filename));
    }
    const std::string temporary =
        ".fireball-hls-" + snapshot_.id + ".partial";
    static_cast<void>(unlinkat(directory.value, temporary.c_str(), 0));
    static_cast<void>(fsync(directory.value));
  }
}

void HlsVodSession::SetFailure(std::string_view code) {
  snapshot_.state = HlsVodJobState::kFailed;
  snapshot_.bytes_per_second = 0;
  snapshot_.failure_code.assign(code.substr(0, 64));
}

}  // namespace fireball::transfer
