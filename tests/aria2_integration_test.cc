#include "fireball/components/transfer/aria2_sidecar.h"
#include "fireball/components/transfer/hls_vod.h"
#include "fireball/components/transfer/transfer_queue.h"
#include "fireball/components/transfer/transfer_types.h"

#include <arpa/inet.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

std::uint16_t FindAvailableLoopbackPort() {
  const int descriptor = socket(AF_INET, SOCK_STREAM, 0);
  assert(descriptor >= 0);
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = 0;
  assert(bind(descriptor, reinterpret_cast<sockaddr*>(&address),
              sizeof(address)) == 0);
  socklen_t length = sizeof(address);
  assert(getsockname(descriptor, reinterpret_cast<sockaddr*>(&address),
                     &length) == 0);
  const std::uint16_t port = ntohs(address.sin_port);
  close(descriptor);
  return port;
}

std::vector<std::uint8_t> LocalTorrentMetainfo() {
  const std::string prefix =
      "d4:infod6:lengthi15e4:name12:fireball.txt12:piece lengthi16384e"
      "6:pieces20:";
  const std::uint8_t piece_hash[] = {
      0x2a, 0xbe, 0x14, 0x6a, 0x58, 0x31, 0x1a, 0x81, 0x0c, 0x09,
      0x28, 0xdd, 0x33, 0xed, 0xb8, 0x4d, 0xbc, 0x18, 0xa7, 0x9a,
  };
  std::vector<std::uint8_t> metainfo(prefix.begin(), prefix.end());
  metainfo.insert(metainfo.end(), std::begin(piece_hash), std::end(piece_hash));
  metainfo.push_back('e');
  metainfo.push_back('e');
  return metainfo;
}

bool HasExpectedPayload(const std::filesystem::path& path,
                        std::size_t expected_size) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return false;
  }
  for (std::size_t index = 0; index < expected_size; ++index) {
    char byte = 0;
    if (!stream.get(byte) ||
        static_cast<std::uint8_t>(byte) !=
            static_cast<std::uint8_t>((index * 31 + 17) % 251)) {
      return false;
    }
  }
  return stream.peek() == std::char_traits<char>::eof();
}

bool HasExpectedHlsPayload(const std::filesystem::path& path,
                           std::size_t segment_count,
                           std::size_t segment_size) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return false;
  }
  for (std::size_t segment = 0; segment < segment_count; ++segment) {
    for (std::size_t offset = 0; offset < segment_size; ++offset) {
      char byte = 0;
      const auto expected = static_cast<std::uint8_t>(
          (offset * 17 + segment * 29 + 3) % 251);
      if (!stream.get(byte) || static_cast<std::uint8_t>(byte) != expected) {
        return false;
      }
    }
  }
  return stream.peek() == std::char_traits<char>::eof();
}

}  // namespace

int main(int argc, char** argv) {
  assert(argc == 7);
  const std::filesystem::path aria2_executable(argv[1]);
  const std::string download_url(argv[2]);
  const std::size_t payload_size = std::stoull(argv[3]);
  const std::uint16_t proxy_port =
      static_cast<std::uint16_t>(std::stoul(argv[4]));
  const std::size_t hls_segment_count = std::stoull(argv[5]);
  const std::size_t hls_segment_size = std::stoull(argv[6]);
  assert(proxy_port != 0);
  assert(hls_segment_count > 0 && hls_segment_size > 0);

  std::string root_pattern = "/tmp/fireball-aria2-test-XXXXXX";
  std::vector<char> mutable_pattern(root_pattern.begin(), root_pattern.end());
  mutable_pattern.push_back('\0');
  const char* root_value = mkdtemp(mutable_pattern.data());
  assert(root_value != nullptr);
  const std::filesystem::path root(root_value);
  const std::filesystem::path downloads = root / "downloads";
  const std::filesystem::path runtime = root / "runtime";
  assert(std::filesystem::create_directory(downloads));
  assert(std::filesystem::create_directory(runtime));
  assert(chmod(downloads.c_str(), 0700) == 0);
  assert(chmod(runtime.c_str(), 0700) == 0);

  fireball::transfer::Aria2SidecarConfig config;
  config.executable = aria2_executable;
  config.downloads_directory = downloads;
  config.runtime_directory = runtime;
  config.rpc_port = FindAvailableLoopbackPort();
  config.maximum_concurrent_downloads = 2;
  config.connections_per_download = 4;

  std::string error;
  assert(!fireball::transfer::ValidateAria2SidecarConfig(config, &error));
  config.has_user_consent = true;
  error.clear();
  config.persistence = fireball::transfer::TransferPersistence::kEphemeral;
  assert(!fireball::transfer::ValidateAria2SidecarConfig(config, &error));
  const std::filesystem::path ephemeral_downloads = runtime / "downloads";
  assert(std::filesystem::create_directory(ephemeral_downloads));
  assert(chmod(ephemeral_downloads.c_str(), 0700) == 0);
  config.downloads_directory = ephemeral_downloads;
  error.clear();
  assert(fireball::transfer::ValidateAria2SidecarConfig(config, &error));
  config.persistence = fireball::transfer::TransferPersistence::kPersistent;
  config.downloads_directory = downloads;
  assert(std::filesystem::remove(ephemeral_downloads));
  config.outbound_http_proxy = "http://127.0.0.1:40000";
  error.clear();
  assert(!fireball::transfer::ValidateAria2SidecarConfig(config, &error));
  config.allow_peer_to_peer = false;
  error.clear();
  assert(fireball::transfer::ValidateAria2SidecarConfig(config, &error));
  config.outbound_http_proxy = "socks5://127.0.0.1:40000";
  error.clear();
  assert(!fireball::transfer::ValidateAria2SidecarConfig(config, &error));
  config.outbound_http_proxy.reset();
  config.allow_peer_to_peer = true;
  error.clear();
  auto sidecar = fireball::transfer::Aria2Sidecar::Launch(config, &error);
  assert(sidecar != nullptr && error.empty());
  const int process_id = sidecar->process_id_for_testing();
  assert(process_id > 0);
  assert(std::filesystem::is_empty(runtime));
  assert(sidecar->rpc().GetVersion().ok());
  fireball::transfer::Aria2RpcClient unauthorized(
      config.rpc_port, std::string(64, '0'),
      fireball::transfer::TransferPersistence::kPersistent);
  assert(!unauthorized.GetVersion().ok());
  auto unknown = sidecar->rpc().TellStatus("0000000000000000");
  assert(!unknown.ok() && !unknown.error.empty() && unknown.error.size() <= 512);

  auto request = fireball::transfer::MakeUriTransferRequest(
      download_url, fireball::transfer::TransferPersistence::kPersistent,
      "fireball-range.bin");
  assert(request.has_value());
  auto wrong_boundary = fireball::transfer::MakeUriTransferRequest(
      download_url, fireball::transfer::TransferPersistence::kEphemeral,
      "must-not-start.bin");
  assert(wrong_boundary.has_value());
  assert(!sidecar->rpc().Enqueue(*wrong_boundary).ok());
  fireball::transfer::TransferQueue queue(
      &sidecar->rpc(), fireball::transfer::TransferPersistence::kPersistent);
  constexpr std::string_view kHttpTransferId =
      "40000000-0000-4000-8000-000000000010";
  assert(queue.Enqueue(std::string(kHttpTransferId), *request,
                       "fireball-range.bin",
                       fireball::transfer::MediaCandidateKind::kDirectVideo));

  bool paused_and_resumed = false;
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(15);
  while (std::chrono::steady_clock::now() < deadline) {
    assert(queue.Refresh(kHttpTransferId));
    const fireball::transfer::TransferItem* status =
        queue.Find(kHttpTransferId);
    assert(status != nullptr);
    if (!paused_and_resumed &&
        status->state == fireball::transfer::TransferState::kActive) {
      assert(queue.Pause(kHttpTransferId));
      assert(queue.Resume(kHttpTransferId));
      paused_and_resumed = true;
    }
    if (status->state == fireball::transfer::TransferState::kComplete) {
      assert(status->completed_bytes == payload_size);
      break;
    }
    assert(status->state != fireball::transfer::TransferState::kFailed);
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
  }
  assert(paused_and_resumed);
  assert(HasExpectedPayload(downloads / "fireball-range.bin", payload_size));
  assert(queue.Find(kHttpTransferId)->state ==
         fireball::transfer::TransferState::kComplete);
  assert(queue.ForgetFinished(kHttpTransferId));
  assert(queue.Find(kHttpTransferId) == nullptr);

  const std::size_t last_slash = download_url.rfind('/');
  assert(last_slash != std::string::npos);
  const std::string hls_origin = download_url.substr(0, last_slash);
  const std::string hls_manifest_uri = hls_origin + "/hls/index.m3u8";
  std::string hls_manifest =
      "#EXTM3U\n#EXT-X-VERSION:3\n#EXT-X-PLAYLIST-TYPE:VOD\n";
  for (std::size_t index = 0; index < hls_segment_count; ++index) {
    hls_manifest += "#EXTINF:2.0,\nsegment-" + std::to_string(index) +
                    ".ts\n";
  }
  hls_manifest += "#EXT-X-ENDLIST\n";
  auto hls_plan = fireball::transfer::ParseHlsVodPlaylist(
      hls_manifest_uri, hls_manifest);
  assert(hls_plan.ok());
  fireball::transfer::HlsVodSession hls_session(
      &sidecar->rpc(), fireball::transfer::TransferPersistence::kPersistent,
      "60000000-0000-4000-8000-000000000010", downloads);
  assert(hls_session.Start(std::move(*hls_plan.value), "fireball-hls-vod.ts"));
  const auto hls_deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(15);
  while (std::chrono::steady_clock::now() < hls_deadline &&
         hls_session.snapshot().state !=
             fireball::transfer::HlsVodJobState::kComplete) {
    assert(hls_session.Refresh());
    assert(hls_session.snapshot().state !=
           fireball::transfer::HlsVodJobState::kFailed);
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
  }
  assert(hls_session.snapshot().state ==
         fireball::transfer::HlsVodJobState::kComplete);
  assert(hls_session.snapshot().segment_count == hls_segment_count);
  assert(hls_session.snapshot().completed_segments == hls_segment_count);
  assert(HasExpectedHlsPayload(downloads / "fireball-hls-vod.ts",
                               hls_segment_count, hls_segment_size));
  struct stat hls_status {};
  assert(stat((downloads / "fireball-hls-vod.ts").c_str(), &hls_status) == 0);
  assert((hls_status.st_mode & 0777) == 0600);
  for (const auto& entry : std::filesystem::directory_iterator(downloads)) {
    assert(!entry.path().filename().string().starts_with(".fireball-hls-"));
  }

  auto torrent = fireball::transfer::MakeTorrentTransferRequest(
      LocalTorrentMetainfo(),
      fireball::transfer::TransferPersistence::kPersistent);
  assert(torrent.has_value());
  auto torrent_added = sidecar->rpc().Enqueue(*torrent);
  assert(torrent_added.ok());
  assert(sidecar->rpc().TellStatus(*torrent_added.value).ok());
  assert(sidecar->rpc().Remove(*torrent_added.value).ok());

  for (const auto& entry : std::filesystem::directory_iterator(downloads)) {
    assert(entry.path().extension() != ".torrent");
  }

  sidecar->Stop();
  assert(kill(process_id, 0) == -1 && errno == ESRCH);
  sidecar.reset();

  config.rpc_port = FindAvailableLoopbackPort();
  config.outbound_http_proxy =
      "http://127.0.0.1:" + std::to_string(proxy_port);
  config.allow_peer_to_peer = false;
  error.clear();
  auto proxied_sidecar =
      fireball::transfer::Aria2Sidecar::Launch(config, &error);
  assert(proxied_sidecar != nullptr && error.empty());
  auto proxied_request = fireball::transfer::MakeUriTransferRequest(
      download_url, fireball::transfer::TransferPersistence::kPersistent,
      "fireball-proxied.bin");
  assert(proxied_request.has_value());
  auto proxied_added = proxied_sidecar->rpc().Enqueue(*proxied_request);
  assert(proxied_added.ok());
  auto blocked_torrent = fireball::transfer::MakeTorrentTransferRequest(
      LocalTorrentMetainfo(),
      fireball::transfer::TransferPersistence::kPersistent);
  assert(blocked_torrent.has_value());
  assert(!proxied_sidecar->rpc().Enqueue(*blocked_torrent).ok());

  const auto proxy_deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(15);
  bool proxy_complete = false;
  while (std::chrono::steady_clock::now() < proxy_deadline) {
    auto status = proxied_sidecar->rpc().TellStatus(*proxied_added.value);
    assert(status.ok());
    if (status.value->state ==
        fireball::transfer::Aria2TransferState::kComplete) {
      proxy_complete = true;
      break;
    }
    assert(status.value->state !=
           fireball::transfer::Aria2TransferState::kError);
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
  }
  assert(proxy_complete);
  assert(HasExpectedPayload(downloads / "fireball-proxied.bin", payload_size));
  proxied_sidecar->Stop();
  proxied_sidecar.reset();

  std::filesystem::remove_all(root);
  return 0;
}
