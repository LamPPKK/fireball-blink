#include "fireball/components/transfer/hls_vod.h"

#include <sys/stat.h>
#include <unistd.h>

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

class FakeHlsBackend final : public fireball::transfer::TransferBackend {
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
    if (fail_status) {
      return {std::nullopt, "backend failed https://private.invalid/token"};
    }
    auto entry = statuses.find(gid);
    return entry == statuses.end()
               ? fireball::transfer::Aria2RpcResult<
                     fireball::transfer::Aria2TransferStatus>{
                     std::nullopt, "missing"}
               : fireball::transfer::Aria2RpcResult<
                     fireball::transfer::Aria2TransferStatus>{entry->second,
                                                               {}};
  }

  fireball::transfer::Aria2RpcResult<std::string> Pause(
      std::string_view gid) override {
    ++pause_count;
    statuses[std::string(gid)].state =
        fireball::transfer::Aria2TransferState::kPaused;
    return {std::string(gid), {}};
  }

  fireball::transfer::Aria2RpcResult<std::string> Unpause(
      std::string_view gid) override {
    ++resume_count;
    statuses[std::string(gid)].state =
        fireball::transfer::Aria2TransferState::kWaiting;
    return {std::string(gid), {}};
  }

  fireball::transfer::Aria2RpcResult<std::string> Remove(
      std::string_view gid) override {
    ++remove_count;
    statuses[std::string(gid)].state =
        fireball::transfer::Aria2TransferState::kRemoved;
    return {std::string(gid), {}};
  }

  fireball::transfer::Aria2RpcResult<std::string> ForgetDownloadResult(
      std::string_view gid) override {
    ++forget_count;
    const auto entry = statuses.find(gid);
    if (entry != statuses.end()) {
      statuses.erase(entry);
    }
    return {std::string("OK"), {}};
  }

  void CompleteAll() {
    for (auto& [gid, status] : statuses) {
      static_cast<void>(gid);
      status.state = fireball::transfer::Aria2TransferState::kComplete;
      status.total_bytes = 1;
      status.completed_bytes = 1;
    }
  }

  std::vector<fireball::transfer::TransferRequest> requests;
  std::map<std::string,
           fireball::transfer::Aria2TransferStatus,
           std::less<>>
      statuses;
  bool fail_status = false;
  int pause_count = 0;
  int resume_count = 0;
  int remove_count = 0;
  int forget_count = 0;
};

std::filesystem::path MakePrivateDirectory() {
  std::string pattern = "/tmp/fireball-hls-unit-XXXXXX";
  std::vector<char> mutable_pattern(pattern.begin(), pattern.end());
  mutable_pattern.push_back('\0');
  const char* value = mkdtemp(mutable_pattern.data());
  assert(value != nullptr);
  assert(chmod(value, 0700) == 0);
  return value;
}

std::string ReadFile(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(stream),
          std::istreambuf_iterator<char>()};
}

fireball::transfer::HlsVodPlan SmallPlan() {
  return {{{"https://cdn.example.test/vod/one.ts", 1000},
           {"https://cdn.example.test/vod/two.ts", 1500}},
          2500};
}

}  // namespace

int main() {
  using fireball::transfer::HlsVodError;
  using fireball::transfer::HlsVodJobState;
  using fireball::transfer::HlsVodSession;
  using fireball::transfer::ParseHlsMasterPlaylist;
  using fireball::transfer::ParseHlsVodPlaylist;
  using fireball::transfer::SelectHlsVariant;
  using fireball::transfer::TransferPersistence;

  const std::string master =
      "#EXTM3U\n"
      "#EXT-X-STREAM-INF:BANDWIDTH=800000,RESOLUTION=640x360\n"
      "low/index.m3u8?token=one\n"
      "#EXT-X-STREAM-INF:BANDWIDTH=2400000,RESOLUTION=1920x1080,"
      "CODECS=\"avc1.640028,mp4a.40.2\"\n"
      "../high/index.m3u8?token=two\n";
  auto parsed_master = ParseHlsMasterPlaylist(
      "https://media.example.test/catalog/master.m3u8?session=private",
      master);
  assert(parsed_master.ok());
  assert(parsed_master.value->variants.size() == 2);
  assert(parsed_master.value->variants[0].uri ==
         "https://media.example.test/catalog/low/index.m3u8?token=one");
  assert(parsed_master.value->variants[1].uri ==
         "https://media.example.test/high/index.m3u8?token=two");
  assert(SelectHlsVariant(*parsed_master.value, 1'000'000)->bandwidth ==
         800'000);
  assert(SelectHlsVariant(*parsed_master.value, 100'000)->bandwidth ==
         800'000);
  assert(SelectHlsVariant(*parsed_master.value, 0)->bandwidth == 2'400'000);
  auto repeated_slash = ParseHlsMasterPlaylist(
      "https://media.example.test/catalog/master.m3u8",
      "#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=1\n"
      "segments//low/../index.m3u8\n");
  assert(repeated_slash.ok());
  assert(repeated_slash.value->variants.front().uri ==
         "https://media.example.test/catalog/segments//index.m3u8");

  assert(!ParseHlsMasterPlaylist(
              "https://media.example.test/master.m3u8",
              "#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=1,BANDWIDTH=2\na.m3u8\n")
              .ok());
  assert(!ParseHlsMasterPlaylist(
              "https://media.example.test/master.m3u8",
              "#EXTM3U\n#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"audio\","
              "URI=\"audio.m3u8\"\n"
              "#EXT-X-STREAM-INF:BANDWIDTH=1\na.m3u8\n")
              .ok());
  assert(!ParseHlsMasterPlaylist(
              "https://media.example.test/master.m3u8",
              "#EXTM3U\n#EXT-X-SESSION-KEY:METHOD=AES-128,URI=\"key\"\n"
              "#EXT-X-STREAM-INF:BANDWIDTH=1\na.m3u8\n")
              .ok());
  auto unsafe_master = ParseHlsMasterPlaylist(
      "https://media.example.test/master.m3u8",
      "#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=1\n"
      "https://user:secret@media.example.test/a.m3u8\n");
  assert(!unsafe_master.ok() && unsafe_master.error == HlsVodError::kUnsafeUri);
  assert(!ParseHlsVodPlaylist(
              "https://media.example.test/vod/index.m3u8#fragment",
              "#EXTM3U\n#EXTINF:1,\none.ts\n#EXT-X-ENDLIST\n")
              .ok());

  const std::string media =
      "#EXTM3U\n"
      "#EXT-X-VERSION:3\n"
      "#EXT-X-PLAYLIST-TYPE:VOD\n"
      "#EXT-X-TARGETDURATION:7\n"
      "#EXTINF:6.006,Opening\n"
      "segments/one.ts?token=private\n"
      "#EXTINF:4.5,Closing\n"
      "../two.ts\n"
      "#EXT-X-ENDLIST\n";
  auto plan = ParseHlsVodPlaylist(
      "https://media.example.test/vod/index.m3u8?session=private", media);
  assert(plan.ok());
  assert(plan.value->segments.size() == 2);
  assert(plan.value->total_duration_ms == 10'506);
  assert(plan.value->segments[0].uri ==
         "https://media.example.test/vod/segments/one.ts?token=private");
  assert(plan.value->segments[1].uri ==
         "https://media.example.test/two.ts");

  auto live = ParseHlsVodPlaylist(
      "https://media.example.test/live.m3u8",
      "#EXTM3U\n#EXTINF:5,\none.ts\n");
  assert(!live.ok() && live.error == HlsVodError::kUnsupportedManifest);
  const std::vector<std::string> unsupported = {
      "#EXT-X-KEY:METHOD=AES-128,URI=\"key.bin\"",
      "#EXT-X-MAP:URI=\"init.mp4\"",
      "#EXT-X-BYTERANGE:100@0",
      "#EXT-X-DISCONTINUITY",
      "#EXT-X-PART:DURATION=0.2,URI=\"part.ts\"",
  };
  for (const std::string& tag : unsupported) {
    const std::string body = "#EXTM3U\n" + tag +
                             "\n#EXTINF:1,\none.ts\n#EXT-X-ENDLIST\n";
    auto result = ParseHlsVodPlaylist(
        "https://media.example.test/vod/index.m3u8", body);
    assert(!result.ok() && result.error == HlsVodError::kUnsupportedManifest);
  }
  auto fmp4 = ParseHlsVodPlaylist(
      "https://media.example.test/vod/index.m3u8",
      "#EXTM3U\n#EXTINF:1,\none.m4s\n#EXT-X-ENDLIST\n");
  assert(!fmp4.ok() && fmp4.error == HlsVodError::kUnsupportedManifest);

  std::string oversized = "#EXTM3U\n";
  for (std::size_t index = 0;
       index < fireball::transfer::kMaximumHlsSegments + 1; ++index) {
    oversized += "#EXTINF:1,\nsegment-" + std::to_string(index) + ".ts\n";
  }
  oversized += "#EXT-X-ENDLIST\n";
  auto limited = ParseHlsVodPlaylist(
      "https://media.example.test/vod/index.m3u8", oversized);
  assert(!limited.ok() && limited.error == HlsVodError::kLimitExceeded);

  const std::filesystem::path directory = MakePrivateDirectory();
  FakeHlsBackend backend;
  HlsVodSession session(&backend, TransferPersistence::kPersistent,
                        "60000000-0000-4000-8000-000000000001", directory);
  assert(session.Start(SmallPlan(), "Fireball capture.ts"));
  assert(session.snapshot().state == HlsVodJobState::kQueued);
  assert(backend.requests.size() == 2);
  for (const auto& request : backend.requests) {
    assert(!request.allow_automatic_renaming);
    assert(request.output_name.has_value());
  }
  assert(session.Pause() && backend.pause_count == 2);
  assert(session.Resume() && backend.resume_count == 2);
  for (std::size_t index = 0; index < backend.requests.size(); ++index) {
    std::ofstream(directory / *backend.requests[index].output_name,
                  std::ios::binary)
        << (index == 0 ? "alpha" : "beta");
  }
  backend.CompleteAll();
  assert(session.Refresh());
  assert(session.snapshot().state == HlsVodJobState::kComplete);
  assert(session.snapshot().completed_segments == 2);
  assert(session.snapshot().total_bytes == 9);
  assert(session.snapshot().failure_code.empty());
  assert(ReadFile(directory / "Fireball capture.ts") == "alphabeta");
  struct stat output_status {};
  assert(stat((directory / "Fireball capture.ts").c_str(), &output_status) ==
         0);
  assert((output_status.st_mode & 0777) == 0600);
  assert(backend.forget_count == 2);
  for (const auto& request : backend.requests) {
    assert(!std::filesystem::exists(directory / *request.output_name));
  }

  FakeHlsBackend conflict_backend;
  HlsVodSession conflict(
      &conflict_backend, TransferPersistence::kPersistent,
      "60000000-0000-4000-8000-000000000004", directory);
  assert(!conflict.Start(SmallPlan(), "Fireball capture.ts"));
  assert(conflict.snapshot().state == HlsVodJobState::kFailed);
  assert(conflict.snapshot().failure_code == "HLS_OUTPUT_CONFLICT");
  assert(conflict_backend.requests.empty());
  assert(ReadFile(directory / "Fireball capture.ts") == "alphabeta");

  FakeHlsBackend cancel_backend;
  HlsVodSession cancelled(
      &cancel_backend, TransferPersistence::kPersistent,
      "60000000-0000-4000-8000-000000000002", directory);
  assert(cancelled.Start(SmallPlan(), "Cancelled capture.ts"));
  std::ofstream(directory / *cancel_backend.requests.front().output_name)
      << "partial";
  std::ofstream(directory /
                (*cancel_backend.requests.front().output_name + ".aria2"))
      << "control";
  assert(cancelled.Cancel());
  assert(cancelled.snapshot().state == HlsVodJobState::kCancelled);
  assert(cancel_backend.remove_count == 2);
  assert(!std::filesystem::exists(
      directory / *cancel_backend.requests.front().output_name));
  assert(!std::filesystem::exists(
      directory /
      (*cancel_backend.requests.front().output_name + ".aria2")));

  FakeHlsBackend failed_backend;
  HlsVodSession failed(&failed_backend, TransferPersistence::kPersistent,
                       "60000000-0000-4000-8000-000000000003", directory);
  assert(failed.Start(SmallPlan(), "Failed capture.ts"));
  failed_backend.fail_status = true;
  assert(!failed.Refresh());
  assert(failed.snapshot().state == HlsVodJobState::kFailed);
  assert(failed.snapshot().failure_code == "HLS_SEGMENT_FAILED");

  std::filesystem::remove_all(directory);
  return 0;
}
