#include "fireball/components/transfer/ffmpeg_muxer.h"

#include "fireball/components/transfer/transfer_types.h"

#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace fireball::transfer {
namespace {

class ScopedDescriptor final {
 public:
  explicit ScopedDescriptor(int value) : value(value) {}
  ~ScopedDescriptor() {
    if (value >= 0) {
      close(value);
    }
  }

  ScopedDescriptor(const ScopedDescriptor&) = delete;
  ScopedDescriptor& operator=(const ScopedDescriptor&) = delete;

  int value;
};

bool ContainsLineBreak(const std::filesystem::path& path) {
  const std::string value = path.string();
  return value.find('\r') != std::string::npos ||
         value.find('\n') != std::string::npos;
}

bool IsExecutableFile(const std::filesystem::path& path) {
  struct stat status {};
  return path.is_absolute() && !ContainsLineBreak(path) &&
         stat(path.c_str(), &status) == 0 && S_ISREG(status.st_mode) &&
         (status.st_mode & 0022) == 0 && access(path.c_str(), X_OK) == 0;
}

bool IsMp4Name(std::string_view name) {
  return name.ends_with(".mp4") && IsSafeOutputName(name);
}

bool IsSafeDirectory(int descriptor) {
  struct stat status {};
  return fstat(descriptor, &status) == 0 && S_ISDIR(status.st_mode) &&
         status.st_uid == getuid() && (status.st_mode & 0022) == 0;
}

int OpenSafeInput(int directory, std::string_view name) {
  if (!IsMp4Name(name)) {
    return -1;
  }
  const int descriptor =
      openat(directory, std::string(name).c_str(),
             O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
  struct stat status {};
  if (descriptor < 0 || fstat(descriptor, &status) != 0 ||
      !S_ISREG(status.st_mode) || status.st_uid != getuid() ||
      status.st_nlink != 1 || (status.st_mode & 0022) != 0 ||
      status.st_size <= 0 ||
      static_cast<std::uint64_t>(status.st_size) > kMaximumDashTrackBytes) {
    if (descriptor >= 0) {
      close(descriptor);
    }
    return -1;
  }
  return descriptor;
}

bool DoesNotExist(int directory, std::string_view name) {
  struct stat status {};
  if (fstatat(directory, std::string(name).c_str(), &status,
              AT_SYMLINK_NOFOLLOW) == 0) {
    return false;
  }
  return errno == ENOENT;
}

bool UnlinkIfPresent(int directory, std::string_view name) {
  const std::string owned_name(name);
  while (unlinkat(directory, owned_name.c_str(), 0) != 0) {
    if (errno == EINTR) {
      continue;
    }
    return errno == ENOENT;
  }
  return true;
}

bool WaitForExit(pid_t process_id,
                 std::chrono::milliseconds timeout,
                 int* status) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  do {
    const pid_t result = waitpid(process_id, status, WNOHANG);
    if (result == process_id) {
      return true;
    }
    if (result < 0 && errno != EINTR) {
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
  } while (std::chrono::steady_clock::now() < deadline);
  return false;
}

void StopProcessGroup(pid_t process_id) {
  if (process_id <= 0) {
    return;
  }
  static_cast<void>(kill(-process_id, SIGTERM));
  int status = 0;
  if (!WaitForExit(process_id, std::chrono::seconds(1), &status)) {
    static_cast<void>(kill(-process_id, SIGKILL));
    static_cast<void>(WaitForExit(process_id, std::chrono::seconds(1), &status));
  }
}

FfmpegMuxResult Failure(std::string code) {
  return {false, std::move(code)};
}

}  // namespace

FfmpegMuxResult RunFfmpegDashMux(const FfmpegMuxRequest& request) {
  if (!IsExecutableFile(request.executable) ||
      !request.download_directory.is_absolute() ||
      ContainsLineBreak(request.download_directory) ||
      request.timeout < std::chrono::seconds(1) ||
      request.timeout > std::chrono::minutes(10) ||
      !IsMp4Name(request.video_input_name) ||
      !IsMp4Name(request.temporary_output_name) ||
      (request.audio_input_name.has_value() &&
       !IsMp4Name(*request.audio_input_name)) ||
      request.video_input_name == request.temporary_output_name ||
      (request.audio_input_name.has_value() &&
       (*request.audio_input_name == request.video_input_name ||
        *request.audio_input_name == request.temporary_output_name))) {
    return Failure("DASH_MUX_INVALID_CONFIG");
  }

  ScopedDescriptor directory(open(request.download_directory.c_str(),
                                      O_RDONLY | O_DIRECTORY | O_CLOEXEC |
                                          O_NOFOLLOW));
  if (directory.value < 0 || !IsSafeDirectory(directory.value)) {
    return Failure("DASH_MUX_UNSAFE_DIRECTORY");
  }
  ScopedDescriptor video_input(
      OpenSafeInput(directory.value, request.video_input_name));
  ScopedDescriptor audio_input(
      request.audio_input_name.has_value()
          ? OpenSafeInput(directory.value, *request.audio_input_name)
          : -1);
  if (video_input.value < 0 ||
      (request.audio_input_name.has_value() && audio_input.value < 0) ||
      !DoesNotExist(directory.value, request.temporary_output_name)) {
    return Failure("DASH_MUX_UNSAFE_INPUT");
  }

  const std::string executable = request.executable.string();
  const std::string output_path =
      (request.download_directory / request.temporary_output_name).string();
  constexpr int kVideoInputDescriptor = 10;
  constexpr int kAudioInputDescriptor = 11;

  std::vector<std::string> owned_arguments = {
      executable, "-nostdin", "-hide_banner", "-loglevel", "error", "-n",
      "-protocol_whitelist", "pipe", "-i", "pipe:10"};
  if (request.audio_input_name.has_value()) {
    owned_arguments.insert(owned_arguments.end(),
                           {"-protocol_whitelist", "pipe", "-i", "pipe:11"});
  }
  owned_arguments.insert(owned_arguments.end(), {"-map", "0:v:0"});
  if (request.audio_input_name.has_value()) {
    owned_arguments.insert(owned_arguments.end(), {"-map", "1:a:0"});
  }
  owned_arguments.insert(owned_arguments.end(),
                         {"-map_metadata", "-1", "-map_chapters", "-1", "-c",
                          "copy", "-movflags", "+faststart", "-f", "mp4",
                          output_path});
  std::vector<char*> arguments;
  arguments.reserve(owned_arguments.size() + 1);
  for (std::string& argument : owned_arguments) {
    arguments.push_back(argument.data());
  }
  arguments.push_back(nullptr);

  posix_spawn_file_actions_t actions;
  if (posix_spawn_file_actions_init(&actions) != 0) {
    return Failure("DASH_MUX_SPAWN_FAILED");
  }
  const int stdin_result = posix_spawn_file_actions_addopen(
      &actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
  const int stdout_result = posix_spawn_file_actions_addopen(
      &actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
  const int stderr_result = posix_spawn_file_actions_addopen(
      &actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
  const int video_input_result = posix_spawn_file_actions_adddup2(
      &actions, video_input.value, kVideoInputDescriptor);
  const int audio_input_result = request.audio_input_name.has_value()
                                     ? posix_spawn_file_actions_adddup2(
                                           &actions, audio_input.value,
                                           kAudioInputDescriptor)
                                     : 0;

  posix_spawnattr_t attributes;
  const int attribute_result = posix_spawnattr_init(&attributes);
  int group_result = attribute_result;
  int flags_result = attribute_result;
  if (attribute_result == 0) {
    group_result = posix_spawnattr_setpgroup(&attributes, 0);
    flags_result =
        posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
  }
  if (stdin_result != 0 || stdout_result != 0 || stderr_result != 0 ||
      video_input_result != 0 || audio_input_result != 0 ||
      attribute_result != 0 || group_result != 0 || flags_result != 0) {
    if (attribute_result == 0) {
      posix_spawnattr_destroy(&attributes);
    }
    posix_spawn_file_actions_destroy(&actions);
    return Failure("DASH_MUX_SPAWN_FAILED");
  }

  std::array<char*, 2> clean_environment = {
      const_cast<char*>("LANG=C"), nullptr};
  pid_t process_id = -1;
  const int spawn_result =
      posix_spawn(&process_id, executable.c_str(), &actions, &attributes,
                  arguments.data(), clean_environment.data());
  posix_spawnattr_destroy(&attributes);
  posix_spawn_file_actions_destroy(&actions);
  if (spawn_result != 0) {
    static_cast<void>(UnlinkIfPresent(directory.value,
                                      request.temporary_output_name));
    return Failure("DASH_MUX_SPAWN_FAILED");
  }

  int status = 0;
  if (!WaitForExit(process_id, request.timeout, &status)) {
    StopProcessGroup(process_id);
    static_cast<void>(UnlinkIfPresent(directory.value,
                                      request.temporary_output_name));
    return Failure("DASH_MUX_TIMEOUT");
  }
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    static_cast<void>(UnlinkIfPresent(directory.value,
                                      request.temporary_output_name));
    return Failure("DASH_MUX_PROCESS_FAILED");
  }

  ScopedDescriptor output(openat(directory.value,
                                 request.temporary_output_name.c_str(),
                                 O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
  struct stat output_status {};
  if (output.value < 0 || fstat(output.value, &output_status) != 0 ||
      !S_ISREG(output_status.st_mode) || output_status.st_uid != getuid() ||
      output_status.st_size <= 0 ||
      static_cast<std::uint64_t>(output_status.st_size) >
          kMaximumDashTrackBytes ||
      fchmod(output.value, 0600) != 0 || fsync(output.value) != 0 ||
      fsync(directory.value) != 0) {
    static_cast<void>(UnlinkIfPresent(directory.value,
                                      request.temporary_output_name));
    return Failure("DASH_MUX_SYNC_FAILED");
  }
  return {true, {}};
}

}  // namespace fireball::transfer
