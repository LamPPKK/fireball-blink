#ifndef FIREBALL_COMPONENTS_TRANSFER_FFMPEG_MUXER_H_
#define FIREBALL_COMPONENTS_TRANSFER_FFMPEG_MUXER_H_

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace fireball::transfer {

inline constexpr std::uint64_t kMaximumDashTrackBytes =
    32ULL * 1024 * 1024 * 1024;

struct FfmpegMuxRequest {
  std::filesystem::path executable;
  std::filesystem::path download_directory;
  std::string video_input_name;
  std::optional<std::string> audio_input_name;
  std::string temporary_output_name;
  std::chrono::milliseconds timeout = std::chrono::minutes(2);
};

struct FfmpegMuxResult {
  bool success = false;
  std::string error_code;
};

// Runs one absolute, non-group/world-writable FFmpeg executable without a
// shell. Inputs are opened without following symlinks, validated and exposed to
// FFmpeg only as pre-opened pipe descriptors; output remains confined to the
// same owner-controlled directory. The mux is stream-copy only and never
// overwrites an existing output.
FfmpegMuxResult RunFfmpegDashMux(const FfmpegMuxRequest& request);

}  // namespace fireball::transfer

#endif  // FIREBALL_COMPONENTS_TRANSFER_FFMPEG_MUXER_H_
