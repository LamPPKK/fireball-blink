#ifndef FIREBALL_COMPONENTS_TRANSFER_DASH_VOD_H_
#define FIREBALL_COMPONENTS_TRANSFER_DASH_VOD_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fireball::transfer {

inline constexpr std::size_t kMaximumDashManifestBytes = 1024 * 1024;
inline constexpr std::size_t kMaximumDashRepresentations = 64;
inline constexpr std::size_t kMaximumDashSegmentsPerTrack = 4096;
inline constexpr std::uint64_t kMaximumDashDurationMs =
    12ULL * 60 * 60 * 1000;

enum class DashVodError {
  kNone,
  kInvalidManifest,
  kUnsupportedManifest,
  kUnsafeUri,
  kLimitExceeded,
};

std::string_view DashVodErrorCode(DashVodError error);

enum class DashTrackKind {
  kVideo,
  kAudio,
};

struct DashTrackPlan {
  DashTrackKind kind = DashTrackKind::kVideo;
  std::string representation_id;
  std::string mime_type;
  std::string codecs;
  std::uint64_t bandwidth = 0;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::string initialization_uri;
  std::vector<std::string> segment_uris;
};

struct DashVodPlan {
  DashTrackPlan video;
  std::optional<DashTrackPlan> audio;
  std::uint64_t duration_ms = 0;
};

template <typename T>
struct DashParseResult {
  std::optional<T> value;
  DashVodError error = DashVodError::kNone;

  bool ok() const { return value.has_value(); }
};

// Parses a deliberately closed DASH VOD subset. The manifest must be a static,
// single-Period, unencrypted fMP4 MPD using SegmentTemplate with either a
// bounded duration or SegmentTimeline. DTDs, custom entities, XLink, SegmentBase,
// SegmentList, live/low-latency features and DRM fail closed.
DashParseResult<DashVodPlan> ParseDashVodManifest(
    std::string_view manifest_uri,
    std::string_view manifest_body,
    std::uint64_t maximum_video_bandwidth = 0);

}  // namespace fireball::transfer

#endif  // FIREBALL_COMPONENTS_TRANSFER_DASH_VOD_H_
