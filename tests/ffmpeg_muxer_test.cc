#include "fireball/components/transfer/ffmpeg_muxer.h"

#include <sys/stat.h>
#include <unistd.h>

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

int main(int argc, char** argv) {
  assert(argc == 3);
  const std::filesystem::path executable(argv[1]);
  const std::filesystem::path directory(argv[2]);

  fireball::transfer::FfmpegMuxRequest request;
  request.executable = executable;
  request.download_directory = directory;
  request.video_input_name = "video-track.mp4";
  request.audio_input_name = "audio-track.mp4";
  request.temporary_output_name = ".fireball-dash-mux.mp4";
  request.timeout = std::chrono::seconds(30);
  auto result = fireball::transfer::RunFfmpegDashMux(request);
  assert(result.success && result.error_code.empty());

  const std::filesystem::path output =
      directory / request.temporary_output_name;
  struct stat output_status {};
  assert(lstat(output.c_str(), &output_status) == 0);
  assert(S_ISREG(output_status.st_mode));
  assert((output_status.st_mode & 0777) == 0600);
  assert(output_status.st_size > 0);

  auto collision = fireball::transfer::RunFfmpegDashMux(request);
  assert(!collision.success &&
         collision.error_code == "DASH_MUX_UNSAFE_INPUT");

  fireball::transfer::FfmpegMuxRequest relative = request;
  relative.executable = "ffmpeg";
  relative.temporary_output_name = "relative.mp4";
  auto invalid = fireball::transfer::RunFfmpegDashMux(relative);
  assert(!invalid.success &&
         invalid.error_code == "DASH_MUX_INVALID_CONFIG");

  const std::filesystem::path symlink = directory / "symlink-track.mp4";
  assert(::symlink((directory / "video-track.mp4").c_str(), symlink.c_str()) ==
         0);
  fireball::transfer::FfmpegMuxRequest linked = request;
  linked.video_input_name = symlink.filename().string();
  linked.temporary_output_name = "linked-output.mp4";
  auto unsafe = fireball::transfer::RunFfmpegDashMux(linked);
  assert(!unsafe.success && unsafe.error_code == "DASH_MUX_UNSAFE_INPUT");
  assert(std::filesystem::remove(symlink));

  const std::filesystem::path corrupt = directory / "corrupt-track.mp4";
  {
    std::ofstream stream(corrupt, std::ios::binary);
    stream << "not an mp4";
  }
  assert(chmod(corrupt.c_str(), 0600) == 0);
  fireball::transfer::FfmpegMuxRequest corrupt_request = request;
  corrupt_request.video_input_name = corrupt.filename().string();
  corrupt_request.audio_input_name.reset();
  corrupt_request.temporary_output_name = "corrupt-output.mp4";
  auto process_failure =
      fireball::transfer::RunFfmpegDashMux(corrupt_request);
  assert(!process_failure.success &&
         process_failure.error_code == "DASH_MUX_PROCESS_FAILED");
  assert(!std::filesystem::exists(directory / "corrupt-output.mp4"));
  return 0;
}
