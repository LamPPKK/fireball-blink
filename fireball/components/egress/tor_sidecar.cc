#include "fireball/components/egress/tor_sidecar.h"

#include "fireball/components/egress/socks5_probe.h"
#include "fireball/components/egress/tor_config.h"
#include "fireball/components/privacy/network_audit.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

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

namespace fireball::egress {
namespace {

bool IsDirectory(const std::filesystem::path& path, struct stat* status) {
  return lstat(path.c_str(), status) == 0 && S_ISDIR(status->st_mode);
}

bool IsExecutableFile(const std::filesystem::path& path) {
  struct stat status {};
  return stat(path.c_str(), &status) == 0 && S_ISREG(status.st_mode) &&
         (status.st_mode & 0022) == 0 && access(path.c_str(), X_OK) == 0;
}

bool IsLoopbackPortAvailable(std::uint16_t port) {
  const int descriptor = socket(AF_INET, SOCK_STREAM, 0);
  if (descriptor < 0) {
    return false;
  }
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  const bool available =
      bind(descriptor, reinterpret_cast<sockaddr*>(&address),
           sizeof(address)) == 0;
  close(descriptor);
  return available;
}

bool ContainsLineBreak(const std::filesystem::path& path) {
  const std::string value = path.string();
  return value.find('\n') != std::string::npos ||
         value.find('\r') != std::string::npos;
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

void RemoveEphemeralPath(const std::filesystem::path& path) {
  if (path.empty()) {
    return;
  }
  std::error_code error;
  std::filesystem::remove_all(path, error);
}

std::optional<std::filesystem::path> CreateDataDirectory(
    const TorSidecarConfig& config,
    std::string* error) {
  const auto path =
      config.runtime_directory / ("tor-" + config.profile_id.value());
  if (mkdir(path.c_str(), 0700) != 0) {
    *error = "could not create isolated Tor data directory: " +
             std::string(std::strerror(errno));
    return std::nullopt;
  }
  return path;
}

std::optional<std::filesystem::path> WritePrivateTorConfig(
    const TorSidecarConfig& config,
    const std::filesystem::path& data_directory,
    std::string* error) {
  auto content = BuildTorConfiguration(data_directory, config.socks5_port,
                                       config.http_connect_port, error);
  if (!content.has_value()) {
    return std::nullopt;
  }
  std::string pattern =
      (config.runtime_directory / ".fireball-torrc-XXXXXX").string();
  std::vector<char> mutable_pattern(pattern.begin(), pattern.end());
  mutable_pattern.push_back('\0');
  const int descriptor = mkstemp(mutable_pattern.data());
  if (descriptor < 0) {
    *error = "could not create private Tor config";
    return std::nullopt;
  }
  const std::filesystem::path path(mutable_pattern.data());
  bool success = fchmod(descriptor, 0600) == 0;
  const int descriptor_flags = fcntl(descriptor, F_GETFD);
  success = success && descriptor_flags >= 0 &&
            fcntl(descriptor, F_SETFD, descriptor_flags | FD_CLOEXEC) == 0;
  success = success && WriteAll(descriptor, *content) && fsync(descriptor) == 0;
  success = success && close(descriptor) == 0;
  if (!success) {
    *error = "could not write private Tor config";
    unlink(path.c_str());
    return std::nullopt;
  }
  return path;
}

}  // namespace

bool ValidateTorSidecarConfig(const TorSidecarConfig& config,
                              std::string* error) {
  if (error == nullptr) {
    return false;
  }
  error->clear();
  if (!privacy::IsNetworkRequestAllowed(
          "fireball.egress.tor", privacy::NetworkPhase::kPostStartup,
          config.has_user_consent)) {
    *error = "Tor activation requires an explicit user action";
    return false;
  }
  if (!config.executable.is_absolute() ||
      !config.runtime_directory.is_absolute() ||
      ContainsLineBreak(config.executable) ||
      ContainsLineBreak(config.runtime_directory)) {
    *error = "Tor paths must be absolute and contain no line breaks";
    return false;
  }
  if (!IsExecutableFile(config.executable)) {
    *error = "Tor executable is missing, unsafe, or not executable";
    return false;
  }
  struct stat runtime_status {};
  if (!IsDirectory(config.runtime_directory, &runtime_status) ||
      runtime_status.st_uid != getuid() || (runtime_status.st_mode & 0077) != 0) {
    *error = "Tor runtime directory must be private and owned by this user";
    return false;
  }
  if (!MakeTorRoute(config.socks5_port, config.http_connect_port).has_value()) {
    *error = "Tor proxy ports are invalid";
    return false;
  }
  if (!IsLoopbackPortAvailable(config.socks5_port) ||
      !IsLoopbackPortAvailable(config.http_connect_port)) {
    *error = "Tor proxy port is already in use";
    return false;
  }
  return true;
}

TorSidecar::TorSidecar(int process_id,
                       EgressRoute route,
                       std::filesystem::path config_path,
                       std::filesystem::path data_directory)
    : process_id_(process_id),
      route_(std::move(route)),
      config_path_(std::move(config_path)),
      data_directory_(std::move(data_directory)) {}

TorSidecar::~TorSidecar() {
  Stop();
}

std::unique_ptr<TorSidecar> TorSidecar::Launch(const TorSidecarConfig& config,
                                               std::string* error) {
  if (!ValidateTorSidecarConfig(config, error)) {
    return nullptr;
  }
  auto route = MakeTorRoute(config.socks5_port, config.http_connect_port);
  auto data_directory = CreateDataDirectory(config, error);
  if (!data_directory.has_value()) {
    return nullptr;
  }
  auto config_path = WritePrivateTorConfig(config, *data_directory, error);
  if (!config_path.has_value()) {
    RemoveEphemeralPath(*data_directory);
    return nullptr;
  }

  const std::string executable = config.executable.string();
  const std::string config_file = config_path->string();
  std::array<char*, 4> arguments = {
      const_cast<char*>(executable.c_str()),
      const_cast<char*>("-f"),
      const_cast<char*>(config_file.c_str()),
      nullptr,
  };
  posix_spawn_file_actions_t actions;
  if (posix_spawn_file_actions_init(&actions) != 0) {
    *error = "could not initialize Tor process actions";
    unlink(config_path->c_str());
    RemoveEphemeralPath(*data_directory);
    return nullptr;
  }
  const int stdout_result = posix_spawn_file_actions_addopen(
      &actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
  const int stderr_result = posix_spawn_file_actions_addopen(
      &actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
  if (stdout_result != 0 || stderr_result != 0) {
    posix_spawn_file_actions_destroy(&actions);
    *error = "could not isolate Tor process output";
    unlink(config_path->c_str());
    RemoveEphemeralPath(*data_directory);
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
    *error = "could not launch Tor sidecar: " +
             std::string(std::strerror(spawn_result));
    unlink(config_path->c_str());
    RemoveEphemeralPath(*data_directory);
    return nullptr;
  }

  auto sidecar = std::unique_ptr<TorSidecar>(new TorSidecar(
      process_id, *route, std::move(*config_path),
      std::move(*data_directory)));
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(30);
  std::string last_error = "Tor SOCKS5 endpoint did not become ready";
  do {
    int status = 0;
    const pid_t child = waitpid(process_id, &status, WNOHANG);
    if (child == process_id) {
      sidecar->process_id_ = -1;
      *error = "Tor exited before its SOCKS5 endpoint became ready";
      sidecar->Stop();
      return nullptr;
    }
    if (ProbeLoopbackSocks5(config.socks5_port,
                            std::chrono::milliseconds(250), &last_error)) {
      // A pre-existing listener must not be mistaken for the child. Both ports
      // were bind-tested before spawn; give Tor time to surface a late bind or
      // configuration failure, then prove the owned child is still alive.
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      int final_status = 0;
      pid_t final_child = -1;
      do {
        final_child = waitpid(process_id, &final_status, WNOHANG);
      } while (final_child < 0 && errno == EINTR);
      if (final_child != 0) {
        if (final_child == process_id || errno == ECHILD) {
          sidecar->process_id_ = -1;
        }
        *error = "Tor exited after SOCKS5 readiness negotiation";
        sidecar->Stop();
        return nullptr;
      }
      unlink(sidecar->config_path_.c_str());
      sidecar->config_path_.clear();
      return sidecar;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  } while (std::chrono::steady_clock::now() < deadline);

  sidecar->Stop();
  *error = "Tor startup timed out: " + last_error;
  return nullptr;
}

void TorSidecar::Stop() {
  if (process_id_ > 0) {
    static_cast<void>(kill(process_id_, SIGTERM));
    if (!WaitForExit(process_id_, std::chrono::seconds(3))) {
      static_cast<void>(kill(process_id_, SIGKILL));
      static_cast<void>(WaitForExit(process_id_, std::chrono::seconds(1)));
    }
    process_id_ = -1;
  }
  if (!config_path_.empty()) {
    unlink(config_path_.c_str());
    config_path_.clear();
  }
  RemoveEphemeralPath(data_directory_);
  data_directory_.clear();
}

}  // namespace fireball::egress
