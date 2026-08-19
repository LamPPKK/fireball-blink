#ifndef FIREBALL_COMPONENTS_TRANSFER_HLS_VOD_H_
#define FIREBALL_COMPONENTS_TRANSFER_HLS_VOD_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "fireball/components/transfer/aria2_rpc_client.h"
#include "fireball/components/transfer/transfer_types.h"

namespace fireball::transfer {

inline constexpr std::size_t kMaximumHlsManifestBytes = 1024 * 1024;
inline constexpr std::size_t kMaximumHlsVariants = 32;
inline constexpr std::size_t kMaximumHlsSegments = 2048;
inline constexpr std::uint64_t kMaximumHlsDurationMs = 12ULL * 60 * 60 * 1000;
inline constexpr std::uint64_t kMaximumHlsOutputBytes = 32ULL * 1024 * 1024 * 1024;

enum class HlsVodError {
  kNone,
  kInvalidManifest,
  kUnsupportedManifest,
  kUnsafeUri,
  kLimitExceeded,
};

std::string_view HlsVodErrorCode(HlsVodError error);

struct HlsVariant {
  std::string uri;
  std::uint64_t bandwidth = 0;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
};

struct HlsMasterPlaylist {
  std::vector<HlsVariant> variants;
};

struct HlsVodSegment {
  std::string uri;
  std::uint64_t duration_ms = 0;
};

struct HlsVodPlan {
  std::vector<HlsVodSegment> segments;
  std::uint64_t total_duration_ms = 0;
};

template <typename T>
struct HlsParseResult {
  std::optional<T> value;
  HlsVodError error = HlsVodError::kNone;

  bool ok() const { return value.has_value(); }
};

// Parses a master playlist without fetching child playlists. Variant URLs are
// resolved to validated HTTP(S) URLs. The caller selects one variant and feeds
// its observed media playlist to ParseHlsVodPlaylist.
HlsParseResult<HlsMasterPlaylist> ParseHlsMasterPlaylist(
    std::string_view manifest_uri,
    std::string_view manifest_body);

// Selects the highest bandwidth not above |maximum_bandwidth|. Zero means no
// cap. If every variant exceeds a non-zero cap, the lowest variant is chosen.
std::optional<HlsVariant> SelectHlsVariant(
    const HlsMasterPlaylist& playlist,
    std::uint64_t maximum_bandwidth);

// The first assembly lane intentionally accepts only finite, unencrypted
// MPEG-TS VOD playlists. Encryption, byte ranges, fMP4 maps, discontinuities,
// live/event playlists and low-latency extensions fail closed.
HlsParseResult<HlsVodPlan> ParseHlsVodPlaylist(
    std::string_view manifest_uri,
    std::string_view manifest_body);

enum class HlsVodJobState {
  kIdle,
  kQueued,
  kActive,
  kPaused,
  kAssembling,
  kComplete,
  kFailed,
  kCancelled,
};

struct HlsVodJobSnapshot {
  std::string id;
  std::string output_name;
  HlsVodJobState state = HlsVodJobState::kIdle;
  std::size_t segment_count = 0;
  std::size_t completed_segments = 0;
  std::uint64_t total_bytes = 0;
  std::uint64_t completed_bytes = 0;
  std::uint64_t bytes_per_second = 0;
  std::string failure_code;
};

// Downloads every validated segment through the same storage/egress-bound
// aria2 backend, then byte-concatenates MPEG-TS segments into a mode-0600 file.
// Source URLs and aria2 GIDs never appear in the product snapshot. The borrowed
// backend must outlive the session and callers must serialize its methods.
class HlsVodSession final {
 public:
  HlsVodSession(TransferBackend* backend,
                TransferPersistence persistence,
                std::string id,
                std::filesystem::path download_directory);
  ~HlsVodSession();

  HlsVodSession(const HlsVodSession&) = delete;
  HlsVodSession& operator=(const HlsVodSession&) = delete;
  HlsVodSession(HlsVodSession&&) = delete;
  HlsVodSession& operator=(HlsVodSession&&) = delete;

  bool Start(HlsVodPlan plan, std::string output_name);
  bool Refresh();
  bool Pause();
  bool Resume();
  bool Cancel();

  const HlsVodJobSnapshot& snapshot() const { return snapshot_; }

 private:
  struct SegmentRecord {
    std::string filename;
    std::string gid;
    Aria2TransferState state = Aria2TransferState::kWaiting;
    std::uint64_t total_bytes = 0;
    std::uint64_t completed_bytes = 0;
    std::uint64_t bytes_per_second = 0;
  };

  bool ValidateStart(const HlsVodPlan& plan,
                     std::string_view output_name) const;
  bool AssembleAndPublish();
  void BestEffortStopAndClean();
  void SetFailure(std::string_view code);

  TransferBackend* backend_;
  TransferPersistence persistence_;
  std::filesystem::path download_directory_;
  HlsVodJobSnapshot snapshot_;
  std::vector<SegmentRecord> segments_;
};

}  // namespace fireball::transfer

#endif  // FIREBALL_COMPONENTS_TRANSFER_HLS_VOD_H_
