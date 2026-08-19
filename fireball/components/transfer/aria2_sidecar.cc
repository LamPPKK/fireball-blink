#include "fireball/components/transfer/aria2_sidecar.h"

#include "fireball/components/privacy/network_audit.h"

#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace fireball::transfer {
namespace {

bool ContainsLineBreak(const std::filesystem::path& path) {
  const std::string value = path.string();
  return value.find('\n') != std::string::npos ||
         value.find('\r') != std::string::npos;
}

bool IsLoopbackHttpProxy(std::string_view value) {
  constexpr std::string_view prefix = "http://127.0.0.1:";
  if (!value.starts_with(prefix)) {
    return false;
  }
  value.remove_prefix(prefix.size());
  if (value.empty() || value.size() > 5 ||
      !std::all_of(value.begin(), value.end(), [](char character) {
        return character >= '0' && character <= '9';
      })) {
    return false;
  }
  unsigned long port = 0;
  for (const char character : value) {
    port = port * 10 + static_cast<unsigned long>(character - '0');
  }
  return port > 0 && port <= 65535;
}

bool IsDirectory(const std::filesystem::path& path, struct stat* status) {
  return lstat(path.c_str(), status) == 0 && S_ISDIR(status->st_mode);
}

bool IsExecutableFile(const std::filesystem::path& path) {
  struct stat status {};
  return stat(path.c_str(), &status) == 0 && S_ISREG(status.st_mode) &&
         (status.st_mode & 0022) == 0 && access(path.c_str(), X_OK) == 0;
}

bool IsStrictDescendant(const std::filesystem::path& child,
                        const std::filesystem::path& parent) {
  std::error_code error;
  const auto canonical_child = std::filesystem::canonical(child, error);
  if (error) {
    return false;
  }
  const auto canonical_parent = std::filesystem::canonical(parent, error);
  if (error || canonical_child == canonical_parent) {
    return false;
  }
  auto child_part = canonical_child.begin();
  for (auto parent_part = canonical_parent.begin();
       parent_part != canonical_parent.end(); ++parent_part, ++child_part) {
    if (child_part == canonical_child.end() || *child_part != *parent_part) {
      return false;
    }
  }
  return true;
}

std::optional<std::string> GenerateSecret() {
  std::array<unsigned char, 32> bytes{};
  if (getentropy(bytes.data(), bytes.size()) != 0) {
    return std::nullopt;
  }
  constexpr char kHex[] = "0123456789abcdef";
  std::string secret;
  secret.reserve(bytes.size() * 2);
  for (const unsigned char byte : bytes) {
    secret.push_back(kHex[byte >> 4]);
    secret.push_back(kHex[byte & 0x0f]);
  }
  std::fill(bytes.begin(), bytes.end(), 0);
  return secret;
}

bool WriteAll(int descriptor, std::string_view content) {
  std::size_t written = 0;
  while (written < content.size()) {
    const ssize_t count =
        write(descriptor, content.data() + written, content.size() - written);
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

std::optional<std::filesystem::path> WritePrivateConfig(
    const Aria2SidecarConfig& config,
    std::string_view secret,
    std::string* error) {
  std::string pattern =
      (config.runtime_directory / ".fireball-aria2-XXXXXX").string();
  std::vector<char> mutable_pattern(pattern.begin(), pattern.end());
  mutable_pattern.push_back('\0');
  const int descriptor = mkstemp(mutable_pattern.data());
  if (descriptor < 0) {
    *error = "could not create private aria2 config: " +
             std::string(std::strerror(errno));
    return std::nullopt;
  }
  const std::filesystem::path path(mutable_pattern.data());

  bool success = fchmod(descriptor, 0600) == 0;
  const int descriptor_flags = fcntl(descriptor, F_GETFD);
  success = success && descriptor_flags >= 0 &&
            fcntl(descriptor, F_SETFD, descriptor_flags | FD_CLOEXEC) == 0;

  std::string content =
      "enable-rpc=true\n"
      "rpc-listen-all=false\n"
      "rpc-allow-origin-all=false\n"
      "rpc-listen-port=" +
      std::to_string(config.rpc_port) + "\n" + "rpc-secret=" +
      std::string(secret) +
      "\n"
      "rpc-save-upload-metadata=false\n"
      "dir=" +
      config.downloads_directory.string() +
      "\n"
      "check-certificate=true\n"
      "file-allocation=none\n"
      "allow-overwrite=false\n"
      "auto-file-renaming=true\n"
      "continue=true\n"
      "max-concurrent-downloads=" +
      std::to_string(config.maximum_concurrent_downloads) +
      "\n"
      "split=" +
      std::to_string(config.connections_per_download) +
      "\n"
      "max-connection-per-server=" +
      std::to_string(config.connections_per_download) +
      "\n"
      "min-split-size=1M\n"
      "seed-time=0\n"
      "enable-dht=false\n"
      "enable-dht6=false\n"
      "bt-enable-lpd=false\n"
      "enable-peer-exchange=false\n"
      "bt-save-metadata=false\n"
      "summary-interval=0\n"
      "console-log-level=warn\n"
      "download-result=hide\n"
      "max-download-result=100\n"
      "stop-with-process=" +
      std::to_string(getpid()) + "\n";
  if (config.outbound_http_proxy.has_value()) {
    content += "all-proxy=" + *config.outbound_http_proxy +
               "\nproxy-method=tunnel\nno-proxy=\n";
  }

  success = success && WriteAll(descriptor, content) && fsync(descriptor) == 0;
  std::fill(content.begin(), content.end(), '\0');
  const int close_result = close(descriptor);
  success = success && close_result == 0;
  if (!success) {
    *error = "could not write private aria2 config";
    unlink(path.c_str());
    return std::nullopt;
  }
  return path;
}

bool WaitForExit(int process_id, std::chrono::milliseconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  do {
    int status = 0;
    const pid_t result = waitpid(process_id, &status, WNOHANG);
    if (result == process_id || (result < 0 && errno == ECHILD)) {
      return true;
    }
    if (result < 0 && errno != EINTR) {
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
  } while (std::chrono::steady_clock::now() < deadline);
  return false;
}

}  // namespace

bool ValidateAria2SidecarConfig(const Aria2SidecarConfig& config,
                                std::string* error) {
  if (error == nullptr) {
    return false;
  }
  if (!config.executable.is_absolute() ||
      !config.downloads_directory.is_absolute() ||
      !config.runtime_directory.is_absolute() ||
      ContainsLineBreak(config.executable) ||
      ContainsLineBreak(config.downloads_directory) ||
      ContainsLineBreak(config.runtime_directory)) {
    *error = "aria2 paths must be absolute and contain no line breaks";
    return false;
  }
  if (!privacy::IsNetworkRequestAllowed(
          "fireball.transfer.aria2", privacy::NetworkPhase::kPostStartup,
          config.has_user_consent)) {
    *error = "aria2 sidecar requires an explicit user transfer action";
    return false;
  }
  if (!IsExecutableFile(config.executable)) {
    *error = "aria2 executable is missing, unsafe, or not executable";
    return false;
  }

  struct stat downloads_status {};
  if (!IsDirectory(config.downloads_directory, &downloads_status) ||
      downloads_status.st_uid != getuid() ||
      (downloads_status.st_mode & 0022) != 0) {
    *error =
        "download destination must be owned by this user and not writable by others";
    return false;
  }
  struct stat runtime_status {};
  if (!IsDirectory(config.runtime_directory, &runtime_status) ||
      runtime_status.st_uid != getuid() || (runtime_status.st_mode & 0077) != 0) {
    *error = "aria2 runtime directory must be private and owned by this user";
    return false;
  }
  if (config.persistence == TransferPersistence::kEphemeral &&
      !IsStrictDescendant(config.downloads_directory,
                          config.runtime_directory)) {
    *error = "ephemeral downloads must stay inside the private runtime directory";
    return false;
  }
  if (config.rpc_port == 0 || config.maximum_concurrent_downloads < 1 ||
      config.maximum_concurrent_downloads > 8 ||
      config.connections_per_download < 1 ||
      config.connections_per_download > 16) {
    *error = "aria2 limits or loopback port are outside the supported range";
    return false;
  }
  if (config.outbound_http_proxy.has_value() &&
      (!IsLoopbackHttpProxy(*config.outbound_http_proxy) ||
       config.allow_peer_to_peer)) {
    *error =
        "proxied aria2 requires a loopback HTTP proxy and disabled peer-to-peer";
    return false;
  }
  return true;
}

Aria2Sidecar::Aria2Sidecar(
    int process_id,
    std::uint16_t rpc_port,
    std::string secret,
    TransferPersistence persistence,
    bool allow_peer_to_peer,
    std::filesystem::path private_config_path)
    : process_id_(process_id),
      rpc_(rpc_port, std::move(secret), persistence, allow_peer_to_peer),
      private_config_path_(std::move(private_config_path)) {}

Aria2Sidecar::~Aria2Sidecar() {
  Stop();
}

std::unique_ptr<Aria2Sidecar> Aria2Sidecar::Launch(
    const Aria2SidecarConfig& config,
    std::string* error) {
  if (!ValidateAria2SidecarConfig(config, error)) {
    return nullptr;
  }
  auto secret = GenerateSecret();
  if (!secret.has_value()) {
    *error = "CSPRNG could not create an aria2 RPC secret";
    return nullptr;
  }
  auto config_path = WritePrivateConfig(config, *secret, error);
  if (!config_path.has_value()) {
    std::fill(secret->begin(), secret->end(), '\0');
    return nullptr;
  }

  const std::string executable = config.executable.string();
  const std::string config_argument = "--conf-path=" + config_path->string();
  std::array<char*, 3> arguments = {
      const_cast<char*>(executable.c_str()),
      const_cast<char*>(config_argument.c_str()),
      nullptr,
  };

  posix_spawn_file_actions_t actions;
  if (posix_spawn_file_actions_init(&actions) != 0) {
    *error = "could not initialize aria2 process actions";
    unlink(config_path->c_str());
    std::fill(secret->begin(), secret->end(), '\0');
    return nullptr;
  }
  const int stdout_result = posix_spawn_file_actions_addopen(
      &actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
  const int stderr_result = posix_spawn_file_actions_addopen(
      &actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
  if (stdout_result != 0 || stderr_result != 0) {
    posix_spawn_file_actions_destroy(&actions);
    *error = "could not isolate aria2 process output";
    unlink(config_path->c_str());
    std::fill(secret->begin(), secret->end(), '\0');
    return nullptr;
  }

  pid_t process_id = -1;
  std::array<char*, 2> clean_environment = {
      const_cast<char*>("LANG=C"),
      nullptr,
  };
  const int spawn_result = posix_spawn(
      &process_id, executable.c_str(), &actions, nullptr, arguments.data(),
      clean_environment.data());
  posix_spawn_file_actions_destroy(&actions);
  if (spawn_result != 0) {
    *error = "could not launch aria2 sidecar: " +
             std::string(std::strerror(spawn_result));
    unlink(config_path->c_str());
    std::fill(secret->begin(), secret->end(), '\0');
    return nullptr;
  }

  auto sidecar = std::unique_ptr<Aria2Sidecar>(new Aria2Sidecar(
      process_id, config.rpc_port, *secret, config.persistence,
      config.allow_peer_to_peer,
      std::move(*config_path)));
  std::fill(secret->begin(), secret->end(), '\0');
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(4);
  std::string last_error = "aria2 RPC did not become ready";
  do {
    int status = 0;
    const pid_t child = waitpid(process_id, &status, WNOHANG);
    if (child == process_id) {
      sidecar->process_id_ = -1;
      *error = "aria2 exited before its loopback RPC became ready";
      unlink(sidecar->private_config_path_.c_str());
      sidecar->private_config_path_.clear();
      return nullptr;
    }
    auto version = sidecar->rpc_.GetVersion();
    if (version.ok()) {
      unlink(sidecar->private_config_path_.c_str());
      sidecar->private_config_path_.clear();
      return sidecar;
    }
    last_error = std::move(version.error);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  } while (std::chrono::steady_clock::now() < deadline);

  sidecar->Stop();
  *error = "aria2 loopback RPC startup timed out: " + last_error;
  return nullptr;
}

void Aria2Sidecar::Stop() {
  if (process_id_ <= 0) {
    if (!private_config_path_.empty()) {
      unlink(private_config_path_.c_str());
      private_config_path_.clear();
    }
    return;
  }

  static_cast<void>(rpc_.ForceShutdown());
  if (!WaitForExit(process_id_, std::chrono::seconds(2))) {
    static_cast<void>(kill(process_id_, SIGTERM));
    if (!WaitForExit(process_id_, std::chrono::seconds(1))) {
      static_cast<void>(kill(process_id_, SIGKILL));
      static_cast<void>(WaitForExit(process_id_, std::chrono::seconds(1)));
    }
  }
  process_id_ = -1;
  if (!private_config_path_.empty()) {
    unlink(private_config_path_.c_str());
    private_config_path_.clear();
  }
}

}  // namespace fireball::transfer
