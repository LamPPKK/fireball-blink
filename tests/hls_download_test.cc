#include "fireball/components/transfer/hls_download.h"

#include <sys/stat.h>
#include <unistd.h>

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

class FakeBackend final : public fireball::transfer::TransferBackend {
 public:
  fireball::transfer::Aria2RpcResult<std::string> Enqueue(
      const fireball::transfer::TransferRequest& request) override {
    char gid[17] = {};
    std::snprintf(gid, sizeof(gid), "%016zx", requests.size() + 1);
    requests.push_back(request);
    statuses[gid] = {gid,
                     fireball::transfer::Aria2TransferState::kWaiting,
                     0,
                     0,
                     0,
                     {},
                     {}};
    return {std::string(gid), {}};
  }

  fireball::transfer::Aria2RpcResult<fireball::transfer::Aria2TransferStatus>
  TellStatus(std::string_view gid) override {
    const auto found = statuses.find(gid);
    return found == statuses.end()
               ? fireball::transfer::Aria2RpcResult<
                     fireball::transfer::Aria2TransferStatus>{std::nullopt,
                                                               "missing"}
               : fireball::transfer::Aria2RpcResult<
                     fireball::transfer::Aria2TransferStatus>{found->second,
                                                               {}};
  }

  fireball::transfer::Aria2RpcResult<std::string> Pause(
      std::string_view gid) override {
    auto found = statuses.find(gid);
    if (found == statuses.end()) {
      return {std::nullopt, "missing"};
    }
    ++pause_count;
    found->second.state = fireball::transfer::Aria2TransferState::kPaused;
    return {std::string(gid), {}};
  }

  fireball::transfer::Aria2RpcResult<std::string> Unpause(
      std::string_view gid) override {
    auto found = statuses.find(gid);
    if (found == statuses.end()) {
      return {std::nullopt, "missing"};
    }
    ++resume_count;
    found->second.state = fireball::transfer::Aria2TransferState::kWaiting;
    return {std::string(gid), {}};
  }

  fireball::transfer::Aria2RpcResult<std::string> Remove(
      std::string_view gid) override {
    auto found = statuses.find(gid);
    if (found == statuses.end()) {
      return {std::nullopt, "missing"};
    }
    ++remove_count;
    found->second.state = fireball::transfer::Aria2TransferState::kRemoved;
    return {std::string(gid), {}};
  }

  fireball::transfer::Aria2RpcResult<std::string> ForgetDownloadResult(
      std::string_view gid) override {
    const auto found = statuses.find(gid);
    if (found == statuses.end()) {
      return {std::nullopt, "missing"};
    }
    ++forget_count;
    statuses.erase(found);
    return {std::string("OK"), {}};
  }

  void Complete(std::size_t request_index, std::uint64_t size) {
    const std::string gid = Gid(request_index);
    auto& status = statuses.at(gid);
    status.state = fireball::transfer::Aria2TransferState::kComplete;
    status.total_bytes = size;
    status.completed_bytes = size;
    status.bytes_per_second = 0;
  }

  void SetLengths(std::size_t request_index,
                  std::uint64_t total,
                  std::uint64_t completed) {
    auto& status = statuses.at(Gid(request_index));
    status.state = fireball::transfer::Aria2TransferState::kActive;
    status.total_bytes = total;
    status.completed_bytes = completed;
  }

  std::string Gid(std::size_t request_index) const {
    char gid[17] = {};
    std::snprintf(gid, sizeof(gid), "%016zx", request_index + 1);
    return gid;
  }

  std::vector<fireball::transfer::TransferRequest> requests;
  std::map<std::string,
           fireball::transfer::Aria2TransferStatus,
           std::less<>>
      statuses;
  int pause_count = 0;
  int resume_count = 0;
  int remove_count = 0;
  int forget_count = 0;
};

std::filesystem::path MakePrivateDirectory() {
  std::string pattern = "/tmp/fireball-hls-download-unit-XXXXXX";
  std::vector<char> mutable_pattern(pattern.begin(), pattern.end());
  mutable_pattern.push_back('\0');
  const char* value = mkdtemp(mutable_pattern.data());
  assert(value != nullptr);
  assert(chmod(value, 0700) == 0);
  return value;
}

void WriteRequestOutput(const std::filesystem::path& directory,
                        const FakeBackend& backend,
                        std::size_t request_index,
                        std::string_view body) {
  assert(request_index < backend.requests.size());
  assert(backend.requests[request_index].output_name.has_value());
  std::ofstream stream(
      directory / *backend.requests[request_index].output_name,
      std::ios::binary);
  stream.write(body.data(), static_cast<std::streamsize>(body.size()));
  assert(stream.good());
}

std::string ReadFile(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(stream),
          std::istreambuf_iterator<char>()};
}

bool HasPrivateHlsArtifacts(const std::filesystem::path& directory) {
  for (const auto& entry : std::filesystem::directory_iterator(directory)) {
    if (entry.path().filename().string().starts_with(".fireball-hls-")) {
      return true;
    }
  }
  return false;
}

}  // namespace

int main() {
  using fireball::transfer::HlsDownload;
  using fireball::transfer::HlsDownloadState;
  using fireball::transfer::TransferPersistence;

  const std::filesystem::path directory = MakePrivateDirectory();
  const std::string master =
      "#EXTM3U\n"
      "#EXT-X-STREAM-INF:BANDWIDTH=800000,RESOLUTION=640x360\n"
      "low/index.m3u8\n"
      "#EXT-X-STREAM-INF:BANDWIDTH=2400000,RESOLUTION=1920x1080\n"
      "high/index.m3u8\n";
  const std::string media =
      "#EXTM3U\n"
      "#EXT-X-PLAYLIST-TYPE:VOD\n"
      "#EXTINF:1.0,\n"
      "../one.ts\n"
      "#EXTINF:1.5,\n"
      "../two.ts\n"
      "#EXT-X-ENDLIST\n";

  FakeBackend backend;
  HlsDownload download(
      &backend, TransferPersistence::kPersistent,
      "70000000-0000-4000-8000-000000000001", directory, 1'000'000);
  assert(download.Start("https://media.example.test/hls/master.m3u8",
                        "Fireball video.ts"));
  assert(download.snapshot().state == HlsDownloadState::kFetchingManifest);
  assert(backend.requests.size() == 1);
  assert(!backend.requests[0].allow_automatic_renaming);
  assert(download.Pause() && backend.pause_count == 1);
  assert(download.snapshot().state == HlsDownloadState::kPaused);
  assert(download.Resume() && backend.resume_count == 1);
  assert(download.snapshot().state == HlsDownloadState::kFetchingManifest);

  WriteRequestOutput(directory, backend, 0, master);
  backend.Complete(0, master.size());
  assert(download.Refresh());
  assert(download.snapshot().state == HlsDownloadState::kFetchingVariant);
  assert(download.snapshot().manifest_fetches_completed == 1);
  assert(download.snapshot().selected_bandwidth == 800'000);
  assert(download.snapshot().selected_width == 640);
  assert(download.snapshot().selected_height == 360);
  assert(backend.requests.size() == 2);
  assert(backend.requests[1].source ==
         "https://media.example.test/hls/low/index.m3u8");

  WriteRequestOutput(directory, backend, 1, media);
  backend.Complete(1, media.size());
  assert(download.Refresh());
  assert(download.snapshot().state ==
         HlsDownloadState::kDownloadingSegments);
  assert(download.snapshot().manifest_fetches_completed == 2);
  assert(download.snapshot().segment_count == 2);
  assert(backend.requests.size() == 4);
  assert(backend.requests[2].source ==
         "https://media.example.test/hls/one.ts");
  assert(backend.requests[3].source ==
         "https://media.example.test/hls/two.ts");
  assert(download.Pause() && backend.pause_count == 3);
  assert(download.snapshot().state == HlsDownloadState::kPaused);
  assert(download.Resume() && backend.resume_count == 3);
  assert(download.snapshot().state ==
         HlsDownloadState::kDownloadingSegments);

  WriteRequestOutput(directory, backend, 2, "alpha");
  WriteRequestOutput(directory, backend, 3, "beta");
  backend.Complete(2, 5);
  backend.Complete(3, 4);
  assert(download.Refresh());
  assert(download.snapshot().state == HlsDownloadState::kComplete);
  assert(download.snapshot().completed_segments == 2);
  assert(download.snapshot().total_bytes == 9);
  assert(download.snapshot().completed_bytes == 9);
  assert(download.snapshot().failure_code.empty());
  assert(ReadFile(directory / "Fireball video.ts") == "alphabeta");
  assert(backend.forget_count == 4);
  assert(!HasPrivateHlsArtifacts(directory));

  FakeBackend direct_backend;
  {
    std::vector<fireball::transfer::TransferRequestHeader> request_headers;
    request_headers.emplace_back(
        fireball::transfer::TransferRequestHeaderKind::kAuthorization,
        "Bearer hls-private-token");
    request_headers.emplace_back(
        fireball::transfer::TransferRequestHeaderKind::kCookie,
        "session=hls-private");
    HlsDownload direct(
        &direct_backend, TransferPersistence::kPersistent,
        "70000000-0000-4000-8000-000000000002", directory);
    assert(direct.Start("https://media.example.test/vod/index.m3u8",
                        "Direct media.ts", std::move(request_headers)));
    WriteRequestOutput(directory, direct_backend, 0, media);
    direct_backend.Complete(0, media.size());
    assert(direct.Refresh());
    assert(direct.snapshot().manifest_fetches_completed == 1);
    assert(direct.snapshot().segment_count == 2);
    assert(direct_backend.requests.size() == 3);
    for (const auto& request : direct_backend.requests) {
      assert(request.request_headers.size() == 2);
      assert(request.request_headers[0].value.view() ==
             "Bearer hls-private-token");
      assert(request.request_headers[1].value.view() ==
             "session=hls-private");
    }
  }
  assert(direct_backend.remove_count == 2);
  assert(direct_backend.forget_count == 3);
  assert(!HasPrivateHlsArtifacts(directory));

  FakeBackend cancel_backend;
  HlsDownload cancelled(
      &cancel_backend, TransferPersistence::kPersistent,
      "70000000-0000-4000-8000-000000000003", directory);
  assert(cancelled.Start("https://media.example.test/vod/index.m3u8",
                         "Cancelled media.ts"));
  WriteRequestOutput(directory, cancel_backend, 0, "partial");
  std::ofstream(directory /
                (*cancel_backend.requests[0].output_name + ".aria2"))
      << "control";
  assert(cancelled.Cancel());
  assert(cancelled.snapshot().state == HlsDownloadState::kCancelled);
  assert(cancel_backend.remove_count == 1);
  assert(cancel_backend.forget_count == 1);
  assert(!HasPrivateHlsArtifacts(directory));

  FakeBackend oversized_backend;
  HlsDownload oversized(
      &oversized_backend, TransferPersistence::kPersistent,
      "70000000-0000-4000-8000-000000000004", directory);
  assert(oversized.Start("https://media.example.test/vod/index.m3u8",
                         "Oversized media.ts"));
  oversized_backend.SetLengths(0,
                               fireball::transfer::kMaximumHlsManifestBytes + 1,
                               1);
  assert(!oversized.Refresh());
  assert(oversized.snapshot().state == HlsDownloadState::kFailed);
  assert(oversized.snapshot().failure_code == "HLS_MANIFEST_SIZE_LIMIT");
  assert(oversized_backend.remove_count == 1);
  assert(oversized_backend.forget_count == 1);

  FakeBackend invalid_backend;
  HlsDownload invalid(
      &invalid_backend, TransferPersistence::kPersistent,
      "70000000-0000-4000-8000-000000000005", directory);
  assert(invalid.Start("https://media.example.test/vod/index.m3u8",
                       "Invalid media.ts"));
  WriteRequestOutput(directory, invalid_backend, 0, "not a playlist");
  invalid_backend.Complete(0, 14);
  assert(!invalid.Refresh());
  assert(invalid.snapshot().state == HlsDownloadState::kFailed);
  assert(invalid.snapshot().failure_code == "HLS_INVALID_MANIFEST");
  assert(!HasPrivateHlsArtifacts(directory));

  FakeBackend conflict_backend;
  HlsDownload conflict(
      &conflict_backend, TransferPersistence::kPersistent,
      "70000000-0000-4000-8000-000000000006", directory);
  assert(!conflict.Start("https://media.example.test/vod/index.m3u8",
                         "Fireball video.ts"));
  assert(conflict.snapshot().failure_code == "HLS_OUTPUT_CONFLICT");
  assert(conflict_backend.requests.empty());

  std::filesystem::remove_all(directory);
  return 0;
}
