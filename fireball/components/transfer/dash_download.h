#ifndef FIREBALL_COMPONENTS_TRANSFER_DASH_DOWNLOAD_H_
#define FIREBALL_COMPONENTS_TRANSFER_DASH_DOWNLOAD_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "fireball/components/transfer/aria2_rpc_client.h"
#include "fireball/components/transfer/dash_vod.h"
#include "fireball/components/transfer/ffmpeg_muxer.h"

namespace fireball::transfer {

enum class DashDownloadState {
  kIdle,
  kFetchingManifest,
  kDownloadingSegments,
  kPaused,
  kAssembling,
  kMuxing,
  kComplete,
  kFailed,
  kCancelled,
};

struct DashDownloadSnapshot {
  std::string id;
  std::string output_name;
  DashDownloadState state = DashDownloadState::kIdle;
  std::uint64_t selected_video_bandwidth = 0;
  std::uint32_t selected_width = 0;
  std::uint32_t selected_height = 0;
  bool has_audio = false;
  std::size_t artifact_count = 0;
  std::size_t completed_artifacts = 0;
  std::uint64_t total_bytes = 0;
  std::uint64_t completed_bytes = 0;
  std::uint64_t bytes_per_second = 0;
  std::string failure_code;
};

// Fetches and parses one bounded static DASH MPD, downloads the selected fMP4
// initialization/media fragments through one storage/egress-bound backend,
// assembles private track files and stream-copies them into one MP4 with
// FFmpeg. Public state contains no MPD/segment URL, request header or backend
// GID. The borrowed backend must outlive this object and calls are single-sequence.
class DashDownload final {
 public:
  DashDownload(TransferBackend* backend,
               TransferPersistence persistence,
               std::string id,
               std::filesystem::path download_directory,
               std::filesystem::path ffmpeg_executable,
               std::uint64_t maximum_video_bandwidth = 0);
  ~DashDownload();

  DashDownload(const DashDownload&) = delete;
  DashDownload& operator=(const DashDownload&) = delete;
  DashDownload(DashDownload&&) = delete;
  DashDownload& operator=(DashDownload&&) = delete;

  bool Start(std::string manifest_uri,
             std::string output_name,
             std::vector<TransferRequestHeader> request_headers = {});
  bool Refresh();
  bool Pause();
  bool Resume();
  bool Cancel();

  const DashDownloadSnapshot& snapshot() const { return snapshot_; }

 private:
  enum class ArtifactKind {
    kVideoInitialization,
    kVideoSegment,
    kAudioInitialization,
    kAudioSegment,
  };

  struct ManifestFetch {
    std::string uri;
    std::string filename;
    std::string gid;
    Aria2TransferState state = Aria2TransferState::kWaiting;
  };

  struct ArtifactRecord {
    ArtifactKind kind = ArtifactKind::kVideoSegment;
    std::string filename;
    std::string gid;
    Aria2TransferState state = Aria2TransferState::kWaiting;
    std::uint64_t total_bytes = 0;
    std::uint64_t completed_bytes = 0;
    std::uint64_t bytes_per_second = 0;
  };

  bool EnqueueManifest(std::string uri);
  bool RefreshManifest();
  bool ConsumeManifest();
  bool EnqueuePlan(DashVodPlan plan);
  bool EnqueueTrack(DashTrackPlan* track, bool audio);
  bool RefreshArtifacts();
  bool AssembleTrack(bool audio, std::string_view output_name);
  bool AssembleMuxAndPublish();
  bool CleanManifest();
  bool CleanArtifacts(bool stop_active);
  void CleanPrivateFiles();
  void ClearRequestHeaders();
  void SetFailure(std::string_view code);

  TransferBackend* backend_;
  TransferPersistence persistence_;
  std::filesystem::path download_directory_;
  std::filesystem::path ffmpeg_executable_;
  std::uint64_t maximum_video_bandwidth_;
  DashDownloadSnapshot snapshot_;
  std::vector<TransferRequestHeader> request_headers_;
  ManifestFetch manifest_;
  std::vector<ArtifactRecord> artifacts_;
  DashDownloadState paused_from_ = DashDownloadState::kIdle;
};

}  // namespace fireball::transfer

#endif  // FIREBALL_COMPONENTS_TRANSFER_DASH_DOWNLOAD_H_
