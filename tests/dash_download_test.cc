#include "fireball/components/transfer/dash_download.h"

#include <sys/stat.h>
#include <unistd.h>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

class FakeBackend final : public fireball::transfer::TransferBackend {
 public:
  fireball::transfer::Aria2RpcResult<std::string> Enqueue(
      const fireball::transfer::TransferRequest& request) override {
    requests.push_back(request);
    const std::string gid = Gid(requests.size());
    statuses.push_back({gid, fireball::transfer::Aria2TransferState::kWaiting,
                        0, 0, 0, {}, {}});
    return {gid, {}};
  }

  fireball::transfer::Aria2RpcResult<fireball::transfer::Aria2TransferStatus>
  TellStatus(std::string_view gid) override {
    for (const auto& status : statuses) {
      if (status.gid == gid) {
        return {status, {}};
      }
    }
    return {std::nullopt, "unknown"};
  }

  fireball::transfer::Aria2RpcResult<std::string> Pause(
      std::string_view gid) override {
    auto* status = Find(gid);
    if (status == nullptr) {
      return {std::nullopt, "unknown"};
    }
    status->state = fireball::transfer::Aria2TransferState::kPaused;
    return {std::string(gid), {}};
  }

  fireball::transfer::Aria2RpcResult<std::string> Unpause(
      std::string_view gid) override {
    auto* status = Find(gid);
    if (status == nullptr) {
      return {std::nullopt, "unknown"};
    }
    status->state = fireball::transfer::Aria2TransferState::kWaiting;
    return {std::string(gid), {}};
  }

  fireball::transfer::Aria2RpcResult<std::string> Remove(
      std::string_view gid) override {
    auto* status = Find(gid);
    if (status == nullptr) {
      return {std::nullopt, "unknown"};
    }
    status->state = fireball::transfer::Aria2TransferState::kRemoved;
    ++remove_count;
    return {std::string(gid), {}};
  }

  fireball::transfer::Aria2RpcResult<std::string> ForgetDownloadResult(
      std::string_view gid) override {
    if (Find(gid) == nullptr) {
      return {std::nullopt, "unknown"};
    }
    ++forget_count;
    return {"OK", {}};
  }

  void Complete(std::size_t index, std::uint64_t bytes) {
    statuses.at(index).state =
        fireball::transfer::Aria2TransferState::kComplete;
    statuses.at(index).total_bytes = bytes;
    statuses.at(index).completed_bytes = bytes;
  }

  std::vector<fireball::transfer::TransferRequest> requests;
  std::vector<fireball::transfer::Aria2TransferStatus> statuses;
  std::size_t remove_count = 0;
  std::size_t forget_count = 0;

 private:
  static std::string Gid(std::size_t index) {
    constexpr char kHex[] = "0123456789abcdef";
    std::string value(16, '0');
    for (std::size_t position = 0; index != 0 && position < value.size();
         ++position) {
      value[value.size() - position - 1] = kHex[index & 0x0f];
      index >>= 4;
    }
    return value;
  }

  fireball::transfer::Aria2TransferStatus* Find(std::string_view gid) {
    for (auto& status : statuses) {
      if (status.gid == gid) {
        return &status;
      }
    }
    return nullptr;
  }
};

void WriteRequestOutput(const std::filesystem::path& directory,
                        const FakeBackend& backend,
                        std::size_t index,
                        std::string_view body) {
  assert(backend.requests.at(index).output_name.has_value());
  std::ofstream stream(directory / *backend.requests.at(index).output_name,
                       std::ios::binary);
  stream.write(body.data(), static_cast<std::streamsize>(body.size()));
  stream.close();
  assert(stream.good());
  assert(chmod((directory / *backend.requests.at(index).output_name).c_str(),
               0600) == 0);
}

}  // namespace

int main() {
  using fireball::transfer::DashDownload;
  using fireball::transfer::DashDownloadState;
  using fireball::transfer::TransferPersistence;

  std::string pattern = "/tmp/fireball-dash-download-test-XXXXXX";
  std::vector<char> mutable_pattern(pattern.begin(), pattern.end());
  mutable_pattern.push_back('\0');
  const char* directory_value = mkdtemp(mutable_pattern.data());
  assert(directory_value != nullptr);
  const std::filesystem::path directory(directory_value);
  assert(chmod(directory.c_str(), 0700) == 0);

  constexpr char kManifest[] = R"xml(
<MPD type="static" mediaPresentationDuration="PT2S">
  <Period>
    <AdaptationSet contentType="video" mimeType="video/mp4"
                   codecs="avc1.4d401e">
      <SegmentTemplate timescale="1000" duration="1000"
          initialization="video-init.mp4" media="video-$Number$.m4s"/>
      <Representation id="video" bandwidth="800000" width="640" height="360"/>
    </AdaptationSet>
    <AdaptationSet contentType="audio" mimeType="audio/mp4"
                   codecs="mp4a.40.2">
      <SegmentTemplate timescale="1000" duration="1000"
          initialization="audio-init.mp4" media="audio-$Number$.m4s"/>
      <Representation id="audio" bandwidth="128000"/>
    </AdaptationSet>
  </Period>
</MPD>)xml";

  FakeBackend backend;
  DashDownload download(
      &backend, TransferPersistence::kPersistent,
      "80000000-0000-4000-8000-000000000001", directory, "/usr/bin/false",
      1'000'000);
  assert(download.Start("https://media.example.test/dash/manifest.mpd",
                        "Fireball DASH.mp4"));
  assert(download.snapshot().state == DashDownloadState::kFetchingManifest);
  assert(backend.requests.size() == 1);
  assert(!backend.requests[0].allow_automatic_renaming);
  assert(download.Pause());
  assert(download.snapshot().state == DashDownloadState::kPaused);
  assert(download.Resume());

  WriteRequestOutput(directory, backend, 0, kManifest);
  backend.Complete(0, sizeof(kManifest) - 1);
  assert(download.Refresh());
  assert(download.snapshot().state ==
         DashDownloadState::kDownloadingSegments);
  assert(download.snapshot().selected_video_bandwidth == 800000);
  assert(download.snapshot().selected_width == 640);
  assert(download.snapshot().selected_height == 360);
  assert(download.snapshot().has_audio);
  assert(download.snapshot().artifact_count == 6);
  assert(backend.requests.size() == 7);
  for (std::size_t index = 1; index < backend.requests.size(); ++index) {
    assert(!backend.requests[index].allow_automatic_renaming);
    assert(backend.requests[index].request_headers.empty());
  }
  assert(download.Pause());
  assert(download.snapshot().state == DashDownloadState::kPaused);
  assert(download.Resume());
  assert(download.snapshot().state ==
         DashDownloadState::kDownloadingSegments);
  assert(download.Cancel());
  assert(download.snapshot().state == DashDownloadState::kCancelled);
  assert(backend.remove_count == 6);
  assert(backend.forget_count == 7);
  for (const auto& entry : std::filesystem::directory_iterator(directory)) {
    assert(!entry.path().filename().string().starts_with(".fireball-dash-"));
  }

  FakeBackend conflict_backend;
  {
    std::ofstream existing(directory / "Existing.mp4");
    existing << "existing";
  }
  DashDownload conflict(
      &conflict_backend, TransferPersistence::kPersistent,
      "80000000-0000-4000-8000-000000000002", directory, "/usr/bin/false");
  assert(!conflict.Start("https://media.example.test/manifest.mpd",
                         "Existing.mp4"));
  assert(conflict.snapshot().state == DashDownloadState::kFailed);
  assert(conflict.snapshot().failure_code == "DASH_OUTPUT_CONFLICT");

  std::filesystem::remove_all(directory);
  return 0;
}
