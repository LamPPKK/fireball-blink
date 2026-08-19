#include "fireball/components/transfer/aria2_sidecar.h"
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

}  // namespace

int main(int argc, char** argv) {
  assert(argc == 4);
  const std::filesystem::path aria2_executable(argv[1]);
  const std::string download_url(argv[2]);
  const std::size_t payload_size = std::stoull(argv[3]);

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
  auto added = sidecar->rpc().Enqueue(*request);
  assert(added.ok());

  bool paused_and_resumed = false;
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(15);
  while (std::chrono::steady_clock::now() < deadline) {
    auto status = sidecar->rpc().TellStatus(*added.value);
    assert(status.ok());
    if (!paused_and_resumed &&
        status.value->state ==
            fireball::transfer::Aria2TransferState::kActive) {
      auto paused = sidecar->rpc().Pause(*added.value);
      assert(paused.ok() && *paused.value == *added.value);
      auto unpaused = sidecar->rpc().Unpause(*added.value);
      assert(unpaused.ok() && *unpaused.value == *added.value);
      paused_and_resumed = true;
    }
    if (status.value->state ==
        fireball::transfer::Aria2TransferState::kComplete) {
      assert(status.value->completed_bytes == payload_size);
      break;
    }
    assert(status.value->state !=
           fireball::transfer::Aria2TransferState::kError);
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
  }
  assert(paused_and_resumed);
  assert(HasExpectedPayload(downloads / "fireball-range.bin", payload_size));

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
  std::filesystem::remove_all(root);
  return 0;
}
