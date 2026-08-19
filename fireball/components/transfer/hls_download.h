#ifndef FIREBALL_COMPONENTS_TRANSFER_HLS_DOWNLOAD_H_
#define FIREBALL_COMPONENTS_TRANSFER_HLS_DOWNLOAD_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "fireball/components/transfer/hls_vod.h"

namespace fireball::transfer {

enum class HlsDownloadState {
  kIdle,
  kFetchingManifest,
  kFetchingVariant,
  kDownloadingSegments,
  kPaused,
  kAssembling,
  kComplete,
  kFailed,
  kCancelled,
};

// Public product state deliberately omits manifest/variant/segment URLs and
// aria2 GIDs. Those values remain private to the coordinator and are erased as
// soon as their stage no longer needs them.
struct HlsDownloadSnapshot {
  std::string id;
  std::string output_name;
  HlsDownloadState state = HlsDownloadState::kIdle;
  std::size_t manifest_fetches_completed = 0;
  std::uint64_t selected_bandwidth = 0;
  std::uint32_t selected_width = 0;
  std::uint32_t selected_height = 0;
  std::size_t segment_count = 0;
  std::size_t completed_segments = 0;
  std::uint64_t total_bytes = 0;
  std::uint64_t completed_bytes = 0;
  std::uint64_t bytes_per_second = 0;
  std::string failure_code;
};

// Fetches one HLS entry manifest through the storage/egress-bound transfer
// backend, follows at most one selected master-playlist variant, then delegates
// finite MPEG-TS VOD segments to HlsVodSession. The borrowed backend must
// outlive this object, and callers must serialize every method on one sequence.
class HlsDownload final {
 public:
  HlsDownload(TransferBackend* backend,
              TransferPersistence persistence,
              std::string id,
              std::filesystem::path download_directory,
              std::uint64_t maximum_bandwidth = 0);
  ~HlsDownload();

  HlsDownload(const HlsDownload&) = delete;
  HlsDownload& operator=(const HlsDownload&) = delete;
  HlsDownload(HlsDownload&&) = delete;
  HlsDownload& operator=(HlsDownload&&) = delete;

  bool Start(std::string manifest_uri,
             std::string output_name,
             std::vector<TransferRequestHeader> request_headers = {});
  bool Refresh();
  bool Pause();
  bool Resume();
  bool Cancel();

  const HlsDownloadSnapshot& snapshot() const { return snapshot_; }

 private:
  enum class ManifestKind {
    kEntry,
    kVariant,
  };

  struct ManifestFetch {
    ManifestKind kind = ManifestKind::kEntry;
    std::string uri;
    std::string filename;
    std::string gid;
    Aria2TransferState state = Aria2TransferState::kWaiting;
  };

  bool EnqueueManifest(ManifestKind kind, std::string uri);
  bool RefreshManifest();
  bool ConsumeManifest();
  bool StartSegments(HlsVodPlan plan);
  void SyncSegmentSnapshot();
  bool CleanManifestFetch();
  void BestEffortClean();
  void ClearRequestHeaders();
  void SetFailure(std::string_view code);

  TransferBackend* backend_;
  TransferPersistence persistence_;
  std::filesystem::path download_directory_;
  std::uint64_t maximum_bandwidth_;
  HlsDownloadSnapshot snapshot_;
  std::vector<TransferRequestHeader> request_headers_;
  ManifestFetch manifest_;
  HlsDownloadState paused_from_ = HlsDownloadState::kIdle;
  std::unique_ptr<HlsVodSession> segment_session_;
};

}  // namespace fireball::transfer

#endif  // FIREBALL_COMPONENTS_TRANSFER_HLS_DOWNLOAD_H_
