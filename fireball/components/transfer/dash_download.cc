#include "fireball/components/transfer/dash_download.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace fireball::transfer {
namespace {

struct ScopedFd {
  explicit ScopedFd(int value = -1) : value(value) {}
  ~ScopedFd() {
    if (value >= 0) {
      close(value);
    }
  }
  ScopedFd(const ScopedFd&) = delete;
  ScopedFd& operator=(const ScopedFd&) = delete;
  int value;
};

bool IsTerminal(DashDownloadState state) {
  return state == DashDownloadState::kComplete ||
         state == DashDownloadState::kFailed ||
         state == DashDownloadState::kCancelled;
}

bool IsSafeDownloadDirectory(const std::filesystem::path& path) {
  if (!path.is_absolute()) {
    return false;
  }
  struct stat status {};
  return lstat(path.c_str(), &status) == 0 && S_ISDIR(status.st_mode) &&
         status.st_uid == getuid() && (status.st_mode & 0022) == 0;
}

bool IsMp4Output(std::string_view name) {
  std::string lowered(name);
  std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](char value) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
  });
  return lowered.ends_with(".mp4");
}

bool DoesNotExistAt(int directory, std::string_view name) {
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

bool RemoveDownloadArtifacts(int directory, std::string_view filename) {
  return UnlinkIfPresent(directory, filename) &&
         UnlinkIfPresent(directory, std::string(filename) + ".aria2");
}

void Erase(std::string* value) {
  volatile char* bytes = value->empty() ? nullptr : value->data();
  for (std::size_t index = 0; index < value->size(); ++index) {
    bytes[index] = 0;
  }
  value->clear();
}

void EraseTrackUris(DashTrackPlan* track) {
  Erase(&track->initialization_uri);
  for (std::string& uri : track->segment_uris) {
    Erase(&uri);
  }
  track->segment_uris.clear();
}

std::string ManifestFilename(std::string_view id) {
  return ".fireball-dash-" + std::string(id) + "-manifest.mpd";
}

std::string PartFilename(std::string_view id,
                         bool audio,
                         bool initialization,
                         std::size_t index) {
  if (initialization) {
    return ".fireball-dash-" + std::string(id) +
           (audio ? "-audio-init.mp4" : "-video-init.mp4");
  }
  std::array<char, 32> suffix{};
  std::snprintf(suffix.data(), suffix.size(), "-%06zu.m4s", index);
  return ".fireball-dash-" + std::string(id) +
         (audio ? "-audio" : "-video") + suffix.data();
}

std::string TrackFilename(std::string_view id, bool audio) {
  return ".fireball-dash-" + std::string(id) +
         (audio ? "-audio-track.mp4" : "-video-track.mp4");
}

std::string MuxFilename(std::string_view id) {
  return ".fireball-dash-" + std::string(id) + "-mux.mp4";
}

std::optional<std::string> ReadBoundedManifest(
    const std::filesystem::path& directory_path,
    std::string_view filename) {
  ScopedFd directory(open(directory_path.c_str(),
                          O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
  ScopedFd input(directory.value < 0
                     ? -1
                     : openat(directory.value, std::string(filename).c_str(),
                              O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
  struct stat status {};
  if (input.value < 0 || fstat(input.value, &status) != 0 ||
      !S_ISREG(status.st_mode) || status.st_uid != getuid() ||
      status.st_nlink != 1 || status.st_size <= 0 ||
      static_cast<std::uint64_t>(status.st_size) >
          kMaximumDashManifestBytes) {
    return std::nullopt;
  }
  std::string body(static_cast<std::size_t>(status.st_size), '\0');
  std::size_t offset = 0;
  while (offset < body.size()) {
    const ssize_t count =
        read(input.value, body.data() + offset, body.size() - offset);
    if (count < 0 && errno == EINTR) {
      continue;
    }
    if (count <= 0) {
      Erase(&body);
      return std::nullopt;
    }
    offset += static_cast<std::size_t>(count);
  }
  char extra = 0;
  ssize_t extra_count = 0;
  do {
    extra_count = read(input.value, &extra, 1);
  } while (extra_count < 0 && errno == EINTR);
  if (extra_count != 0) {
    Erase(&body);
    return std::nullopt;
  }
  return body;
}

bool WriteAll(int descriptor, const char* bytes, std::size_t size) {
  std::size_t offset = 0;
  while (offset < size) {
    const ssize_t count = write(descriptor, bytes + offset, size - offset);
    if (count < 0 && errno == EINTR) {
      continue;
    }
    if (count <= 0) {
      return false;
    }
    offset += static_cast<std::size_t>(count);
  }
  return true;
}

bool AppendFile(int directory,
                std::string_view name,
                int output,
                std::uint64_t* total_bytes) {
  ScopedFd input(openat(directory, std::string(name).c_str(),
                        O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
  struct stat status {};
  if (input.value < 0 || fstat(input.value, &status) != 0 ||
      !S_ISREG(status.st_mode) || status.st_uid != getuid() ||
      status.st_nlink != 1 || status.st_size <= 0 ||
      static_cast<std::uint64_t>(status.st_size) >
          kMaximumDashTrackBytes - *total_bytes) {
    return false;
  }
  std::array<char, 128 * 1024> buffer{};
  std::uint64_t copied = 0;
  while (true) {
    const ssize_t count = read(input.value, buffer.data(), buffer.size());
    if (count < 0 && errno == EINTR) {
      continue;
    }
    if (count < 0 ||
        (count > 0 &&
         !WriteAll(output, buffer.data(), static_cast<std::size_t>(count)))) {
      return false;
    }
    if (count == 0) {
      break;
    }
    copied += static_cast<std::uint64_t>(count);
  }
  if (copied != static_cast<std::uint64_t>(status.st_size)) {
    return false;
  }
  *total_bytes += copied;
  return true;
}

bool PublishNoOverwrite(int directory,
                        std::string_view temporary,
                        std::string_view output_name) {
  const std::string temporary_name(temporary);
  const std::string destination(output_name);
  if (!DoesNotExistAt(directory, destination) ||
      linkat(directory, temporary_name.c_str(), directory, destination.c_str(),
             0) != 0) {
    return false;
  }
  bool success = fsync(directory) == 0;
  if (!UnlinkIfPresent(directory, temporary_name)) {
    success = false;
  }
  if (fsync(directory) != 0) {
    success = false;
  }
  if (!success) {
    static_cast<void>(UnlinkIfPresent(directory, destination));
    static_cast<void>(UnlinkIfPresent(directory, temporary_name));
    static_cast<void>(fsync(directory));
  }
  return success;
}

}  // namespace

DashDownload::DashDownload(TransferBackend* backend,
                           TransferPersistence persistence,
                           std::string id,
                           std::filesystem::path download_directory,
                           std::filesystem::path ffmpeg_executable,
                           std::uint64_t maximum_video_bandwidth)
    : backend_(backend),
      persistence_(persistence),
      download_directory_(std::move(download_directory)),
      ffmpeg_executable_(std::move(ffmpeg_executable)),
      maximum_video_bandwidth_(maximum_video_bandwidth) {
  snapshot_.id = std::move(id);
}

DashDownload::~DashDownload() {
  // Failed cleanup is retried at destruction. Complete and Cancelled are the
  // only terminal states that already proved their private artifacts are gone.
  if (snapshot_.state != DashDownloadState::kIdle &&
      snapshot_.state != DashDownloadState::kComplete &&
      snapshot_.state != DashDownloadState::kCancelled) {
    static_cast<void>(CleanManifest());
    static_cast<void>(CleanArtifacts(true));
    CleanPrivateFiles();
    ClearRequestHeaders();
  }
}

bool DashDownload::Start(
    std::string manifest_uri,
    std::string output_name,
    std::vector<TransferRequestHeader> request_headers) {
  if (backend_ == nullptr || snapshot_.state != DashDownloadState::kIdle ||
      !IsCanonicalTransferId(snapshot_.id) ||
      !IsSafeDownloadDirectory(download_directory_) ||
      !ffmpeg_executable_.is_absolute() ||
      !IsSafeHttpDownloadUri(manifest_uri) ||
      manifest_uri.find('#') != std::string::npos ||
      !IsSafeOutputName(output_name) || !IsMp4Output(output_name) ||
      !IsValidTransferRequestHeaders(request_headers)) {
    Erase(&manifest_uri);
    SetFailure("DASH_INVALID_JOB");
    return false;
  }
  ScopedFd directory(open(download_directory_.c_str(),
                          O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
  if (directory.value < 0 || !DoesNotExistAt(directory.value, output_name)) {
    Erase(&manifest_uri);
    SetFailure("DASH_OUTPUT_CONFLICT");
    return false;
  }
  snapshot_.output_name = std::move(output_name);
  request_headers_ = std::move(request_headers);
  if (!EnqueueManifest(std::move(manifest_uri))) {
    SetFailure("DASH_MANIFEST_ENQUEUE_FAILED");
    return false;
  }
  snapshot_.state = DashDownloadState::kFetchingManifest;
  snapshot_.failure_code.clear();
  return true;
}

bool DashDownload::EnqueueManifest(std::string uri) {
  const std::string filename = ManifestFilename(snapshot_.id);
  ScopedFd directory(open(download_directory_.c_str(),
                          O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
  if (directory.value < 0 || !DoesNotExistAt(directory.value, filename) ||
      !DoesNotExistAt(directory.value, filename + ".aria2")) {
    Erase(&uri);
    return false;
  }
  TransferRequest request{TransferSourceKind::kHttp,
                          persistence_,
                          uri,
                          filename,
                          {},
                          false,
                          request_headers_};
  auto result = backend_->Enqueue(request);
  Erase(&request.source);
  if (!result.ok() || !IsValidAria2Gid(*result.value)) {
    static_cast<void>(RemoveDownloadArtifacts(directory.value, filename));
    Erase(&uri);
    return false;
  }
  manifest_ = {std::move(uri), filename, std::move(*result.value),
               Aria2TransferState::kWaiting};
  return true;
}

bool DashDownload::Refresh() {
  if (snapshot_.state == DashDownloadState::kFetchingManifest) {
    return RefreshManifest();
  }
  if (snapshot_.state == DashDownloadState::kDownloadingSegments) {
    return RefreshArtifacts();
  }
  return false;
}

bool DashDownload::RefreshManifest() {
  auto result = backend_->TellStatus(manifest_.gid);
  if (!result.ok() || result.value->gid != manifest_.gid ||
      result.value->state == Aria2TransferState::kUnknown ||
      result.value->state == Aria2TransferState::kError ||
      result.value->state == Aria2TransferState::kRemoved ||
      (result.value->total_bytes != 0 &&
       result.value->completed_bytes > result.value->total_bytes) ||
      result.value->total_bytes > kMaximumDashManifestBytes ||
      result.value->completed_bytes > kMaximumDashManifestBytes) {
    SetFailure(result.ok() &&
                       (result.value->total_bytes > kMaximumDashManifestBytes ||
                        result.value->completed_bytes >
                            kMaximumDashManifestBytes)
                   ? "DASH_MANIFEST_SIZE_LIMIT"
                   : "DASH_MANIFEST_FETCH_FAILED");
    return false;
  }
  manifest_.state = result.value->state;
  snapshot_.total_bytes = result.value->total_bytes;
  snapshot_.completed_bytes = result.value->completed_bytes;
  snapshot_.bytes_per_second = result.value->bytes_per_second;
  snapshot_.failure_code.clear();
  if (manifest_.state == Aria2TransferState::kComplete) {
    return ConsumeManifest();
  }
  if (manifest_.state == Aria2TransferState::kPaused) {
    paused_from_ = DashDownloadState::kFetchingManifest;
    snapshot_.state = DashDownloadState::kPaused;
  }
  return true;
}

bool DashDownload::ConsumeManifest() {
  std::string manifest_uri = std::move(manifest_.uri);
  auto body = ReadBoundedManifest(download_directory_, manifest_.filename);
  if (!body.has_value()) {
    Erase(&manifest_uri);
    SetFailure("DASH_MANIFEST_READ_FAILED");
    return false;
  }
  auto parsed = ParseDashVodManifest(manifest_uri, *body,
                                     maximum_video_bandwidth_);
  Erase(&*body);
  Erase(&manifest_uri);
  if (!CleanManifest()) {
    SetFailure("DASH_MANIFEST_CLEANUP_FAILED");
    return false;
  }
  if (!parsed.ok()) {
    SetFailure(DashVodErrorCode(parsed.error));
    return false;
  }
  return EnqueuePlan(std::move(*parsed.value));
}

bool DashDownload::EnqueuePlan(DashVodPlan plan) {
  snapshot_.selected_video_bandwidth = plan.video.bandwidth;
  snapshot_.selected_width = plan.video.width;
  snapshot_.selected_height = plan.video.height;
  snapshot_.has_audio = plan.audio.has_value();
  if (!EnqueueTrack(&plan.video, false)) {
    if (plan.audio.has_value()) {
      EraseTrackUris(&*plan.audio);
    }
    SetFailure("DASH_ARTIFACT_ENQUEUE_FAILED");
    return false;
  }
  if (plan.audio.has_value() && !EnqueueTrack(&*plan.audio, true)) {
    SetFailure("DASH_ARTIFACT_ENQUEUE_FAILED");
    return false;
  }
  snapshot_.artifact_count = artifacts_.size();
  snapshot_.completed_artifacts = 0;
  snapshot_.total_bytes = 0;
  snapshot_.completed_bytes = 0;
  snapshot_.bytes_per_second = 0;
  snapshot_.state = DashDownloadState::kDownloadingSegments;
  snapshot_.failure_code.clear();
  return true;
}

bool DashDownload::EnqueueTrack(DashTrackPlan* track, bool audio) {
  std::vector<std::pair<std::string, ArtifactKind>> sources;
  sources.reserve(track->segment_uris.size() + 1);
  sources.emplace_back(
      track->initialization_uri,
      audio ? ArtifactKind::kAudioInitialization
            : ArtifactKind::kVideoInitialization);
  for (const std::string& uri : track->segment_uris) {
    sources.emplace_back(uri, audio ? ArtifactKind::kAudioSegment
                                    : ArtifactKind::kVideoSegment);
  }
  EraseTrackUris(track);
  const auto erase_sources = [&sources]() {
    for (auto& source : sources) {
      Erase(&source.first);
    }
  };
  ScopedFd directory(open(download_directory_.c_str(),
                          O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
  if (directory.value < 0) {
    erase_sources();
    return false;
  }
  for (std::size_t index = 0; index < sources.size(); ++index) {
    const bool initialization = index == 0;
    const std::string filename =
        PartFilename(snapshot_.id, audio, initialization,
                     initialization ? 0 : index - 1);
    if (!DoesNotExistAt(directory.value, filename) ||
        !DoesNotExistAt(directory.value, filename + ".aria2")) {
      erase_sources();
      return false;
    }
    TransferRequest request{TransferSourceKind::kHttp,
                            persistence_,
                            sources[index].first,
                            filename,
                            {},
                            false,
                            request_headers_};
    auto result = backend_->Enqueue(request);
    Erase(&request.source);
    if (!result.ok() || !IsValidAria2Gid(*result.value)) {
      static_cast<void>(RemoveDownloadArtifacts(directory.value, filename));
      erase_sources();
      return false;
    }
    artifacts_.push_back({sources[index].second, filename,
                          std::move(*result.value),
                          Aria2TransferState::kWaiting, 0, 0, 0});
  }
  erase_sources();
  return true;
}

bool DashDownload::RefreshArtifacts() {
  snapshot_.completed_artifacts = 0;
  snapshot_.total_bytes = 0;
  snapshot_.completed_bytes = 0;
  snapshot_.bytes_per_second = 0;
  bool all_complete = true;
  for (ArtifactRecord& artifact : artifacts_) {
    auto result = backend_->TellStatus(artifact.gid);
    if (!result.ok() || result.value->gid != artifact.gid ||
        result.value->state == Aria2TransferState::kUnknown ||
        result.value->state == Aria2TransferState::kError ||
        result.value->state == Aria2TransferState::kRemoved ||
        (result.value->total_bytes != 0 &&
         result.value->completed_bytes > result.value->total_bytes) ||
        result.value->total_bytes > kMaximumDashTrackBytes ||
        result.value->completed_bytes > kMaximumDashTrackBytes) {
      SetFailure("DASH_ARTIFACT_FETCH_FAILED");
      return false;
    }
    artifact.state = result.value->state;
    artifact.total_bytes = result.value->total_bytes;
    artifact.completed_bytes = result.value->completed_bytes;
    artifact.bytes_per_second = result.value->bytes_per_second;
    if (artifact.state == Aria2TransferState::kComplete) {
      ++snapshot_.completed_artifacts;
    } else {
      all_complete = false;
    }
    if (snapshot_.total_bytes >
            kMaximumDashTrackBytes * 2 - artifact.total_bytes ||
        snapshot_.completed_bytes >
            kMaximumDashTrackBytes * 2 - artifact.completed_bytes ||
        snapshot_.bytes_per_second >
            std::numeric_limits<std::uint64_t>::max() -
                artifact.bytes_per_second) {
      SetFailure("DASH_ARTIFACT_SIZE_LIMIT");
      return false;
    }
    snapshot_.total_bytes += artifact.total_bytes;
    snapshot_.completed_bytes += artifact.completed_bytes;
    snapshot_.bytes_per_second += artifact.bytes_per_second;
  }
  snapshot_.failure_code.clear();
  if (all_complete) {
    return AssembleMuxAndPublish();
  }
  return true;
}

bool DashDownload::AssembleTrack(bool audio, std::string_view output_name) {
  ScopedFd directory(open(download_directory_.c_str(),
                          O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
  if (directory.value < 0 || !DoesNotExistAt(directory.value, output_name)) {
    return false;
  }
  const std::string owned_output(output_name);
  ScopedFd output(openat(directory.value, owned_output.c_str(),
                         O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
                         0600));
  if (output.value < 0 || fchmod(output.value, 0600) != 0) {
    static_cast<void>(UnlinkIfPresent(directory.value, output_name));
    return false;
  }
  std::uint64_t total_bytes = 0;
  bool found_initialization = false;
  bool success = true;
  for (const ArtifactRecord& artifact : artifacts_) {
    const bool artifact_audio =
        artifact.kind == ArtifactKind::kAudioInitialization ||
        artifact.kind == ArtifactKind::kAudioSegment;
    if (artifact_audio != audio) {
      continue;
    }
    const bool initialization =
        artifact.kind == ArtifactKind::kVideoInitialization ||
        artifact.kind == ArtifactKind::kAudioInitialization;
    if (initialization) {
      if (found_initialization || total_bytes != 0) {
        success = false;
        break;
      }
      found_initialization = true;
    } else if (!found_initialization) {
      success = false;
      break;
    }
    if (!AppendFile(directory.value, artifact.filename, output.value,
                    &total_bytes)) {
      success = false;
      break;
    }
  }
  success = success && found_initialization && total_bytes > 0 &&
            fsync(output.value) == 0 && fsync(directory.value) == 0;
  if (!success) {
    static_cast<void>(UnlinkIfPresent(directory.value, output_name));
    static_cast<void>(fsync(directory.value));
  }
  return success;
}

bool DashDownload::AssembleMuxAndPublish() {
  snapshot_.state = DashDownloadState::kAssembling;
  snapshot_.bytes_per_second = 0;
  const std::string video_track = TrackFilename(snapshot_.id, false);
  const std::string audio_track = TrackFilename(snapshot_.id, true);
  const std::string mux_output = MuxFilename(snapshot_.id);
  if (!AssembleTrack(false, video_track) ||
      (snapshot_.has_audio && !AssembleTrack(true, audio_track))) {
    SetFailure("DASH_TRACK_ASSEMBLY_FAILED");
    return false;
  }
  if (!CleanArtifacts(false)) {
    SetFailure("DASH_ARTIFACT_CLEANUP_FAILED");
    return false;
  }

  snapshot_.state = DashDownloadState::kMuxing;
  FfmpegMuxRequest request;
  request.executable = ffmpeg_executable_;
  request.download_directory = download_directory_;
  request.video_input_name = video_track;
  if (snapshot_.has_audio) {
    request.audio_input_name = audio_track;
  }
  request.temporary_output_name = mux_output;
  const FfmpegMuxResult muxed = RunFfmpegDashMux(request);
  if (!muxed.success) {
    SetFailure(muxed.error_code.empty() ? "DASH_MUX_FAILED"
                                        : muxed.error_code);
    return false;
  }

  ScopedFd directory(open(download_directory_.c_str(),
                          O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
  if (directory.value < 0 ||
      !PublishNoOverwrite(directory.value, mux_output, snapshot_.output_name)) {
    SetFailure("DASH_PUBLISH_FAILED");
    return false;
  }
  const bool private_tracks_removed =
      UnlinkIfPresent(directory.value, video_track) &&
      UnlinkIfPresent(directory.value, audio_track) &&
      fsync(directory.value) == 0;
  if (!private_tracks_removed) {
    static_cast<void>(
        UnlinkIfPresent(directory.value, snapshot_.output_name));
    static_cast<void>(fsync(directory.value));
    SetFailure("DASH_FINAL_CLEANUP_FAILED");
    return false;
  }
  ClearRequestHeaders();
  snapshot_.state = DashDownloadState::kComplete;
  snapshot_.failure_code.clear();
  return true;
}

bool DashDownload::Pause() {
  if (snapshot_.state == DashDownloadState::kFetchingManifest) {
    auto result = backend_->Pause(manifest_.gid);
    if (!result.ok() || *result.value != manifest_.gid) {
      snapshot_.failure_code = "DASH_CONTROL_FAILED";
      return false;
    }
    manifest_.state = Aria2TransferState::kPaused;
    paused_from_ = DashDownloadState::kFetchingManifest;
  } else if (snapshot_.state == DashDownloadState::kDownloadingSegments) {
    std::vector<ArtifactRecord*> paused;
    for (ArtifactRecord& artifact : artifacts_) {
      if (artifact.state == Aria2TransferState::kComplete ||
          artifact.state == Aria2TransferState::kPaused) {
        continue;
      }
      auto result = backend_->Pause(artifact.gid);
      if (!result.ok() || *result.value != artifact.gid) {
        for (ArtifactRecord* rollback : paused) {
          static_cast<void>(backend_->Unpause(rollback->gid));
          rollback->state = Aria2TransferState::kWaiting;
        }
        snapshot_.failure_code = "DASH_CONTROL_FAILED";
        return false;
      }
      artifact.state = Aria2TransferState::kPaused;
      paused.push_back(&artifact);
    }
    paused_from_ = DashDownloadState::kDownloadingSegments;
  } else {
    return false;
  }
  snapshot_.state = DashDownloadState::kPaused;
  snapshot_.bytes_per_second = 0;
  snapshot_.failure_code.clear();
  return true;
}

bool DashDownload::Resume() {
  if (snapshot_.state != DashDownloadState::kPaused) {
    return false;
  }
  if (paused_from_ == DashDownloadState::kFetchingManifest) {
    auto result = backend_->Unpause(manifest_.gid);
    if (!result.ok() || *result.value != manifest_.gid) {
      snapshot_.failure_code = "DASH_CONTROL_FAILED";
      return false;
    }
    manifest_.state = Aria2TransferState::kWaiting;
  } else if (paused_from_ == DashDownloadState::kDownloadingSegments) {
    std::vector<ArtifactRecord*> resumed;
    for (ArtifactRecord& artifact : artifacts_) {
      if (artifact.state != Aria2TransferState::kPaused) {
        continue;
      }
      auto result = backend_->Unpause(artifact.gid);
      if (!result.ok() || *result.value != artifact.gid) {
        for (ArtifactRecord* rollback : resumed) {
          static_cast<void>(backend_->Pause(rollback->gid));
          rollback->state = Aria2TransferState::kPaused;
        }
        snapshot_.failure_code = "DASH_CONTROL_FAILED";
        return false;
      }
      artifact.state = Aria2TransferState::kWaiting;
      resumed.push_back(&artifact);
    }
  } else {
    return false;
  }
  snapshot_.state = paused_from_;
  paused_from_ = DashDownloadState::kIdle;
  snapshot_.failure_code.clear();
  return true;
}

bool DashDownload::Cancel() {
  if (snapshot_.state == DashDownloadState::kIdle || IsTerminal(snapshot_.state)) {
    return false;
  }
  const bool manifest_clean = CleanManifest();
  const bool artifacts_clean = CleanArtifacts(true);
  CleanPrivateFiles();
  ClearRequestHeaders();
  snapshot_.bytes_per_second = 0;
  if (manifest_clean && artifacts_clean) {
    snapshot_.state = DashDownloadState::kCancelled;
    snapshot_.failure_code.clear();
    return true;
  }
  SetFailure("DASH_CANCEL_INCOMPLETE");
  return false;
}

bool DashDownload::CleanManifest() {
  bool success = true;
  if (!manifest_.gid.empty() && backend_ != nullptr) {
    auto status = backend_->TellStatus(manifest_.gid);
    if (status.ok() && status.value->state == Aria2TransferState::kComplete) {
      auto forgotten = backend_->ForgetDownloadResult(manifest_.gid);
      success = forgotten.ok() && *forgotten.value == "OK";
    } else {
      auto removed = backend_->Remove(manifest_.gid);
      auto forgotten = backend_->ForgetDownloadResult(manifest_.gid);
      const bool already_stopped =
          status.ok() &&
          (status.value->state == Aria2TransferState::kRemoved ||
           status.value->state == Aria2TransferState::kError);
      success = forgotten.ok() && *forgotten.value == "OK" &&
                ((removed.ok() && *removed.value == manifest_.gid) ||
                 already_stopped || !status.ok());
    }
  }
  ScopedFd directory(open(download_directory_.c_str(),
                          O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
  if (directory.value < 0) {
    success = false;
  } else if (!manifest_.filename.empty()) {
    success = RemoveDownloadArtifacts(directory.value, manifest_.filename) &&
              fsync(directory.value) == 0 && success;
  }
  Erase(&manifest_.uri);
  if (success) {
    manifest_.filename.clear();
    manifest_.gid.clear();
  }
  return success;
}

bool DashDownload::CleanArtifacts(bool stop_active) {
  bool success = true;
  ScopedFd directory(open(download_directory_.c_str(),
                          O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
  if (directory.value < 0) {
    success = false;
  }
  for (ArtifactRecord& artifact : artifacts_) {
    bool backend_clean = true;
    if (!artifact.gid.empty() && backend_ != nullptr) {
      auto status = backend_->TellStatus(artifact.gid);
      if (!stop_active && status.ok() &&
          status.value->state != Aria2TransferState::kComplete) {
        backend_clean = false;
      } else if (status.ok() &&
                 status.value->state == Aria2TransferState::kComplete) {
        auto forgotten = backend_->ForgetDownloadResult(artifact.gid);
        backend_clean = forgotten.ok() && *forgotten.value == "OK";
      } else {
        auto removed = backend_->Remove(artifact.gid);
        auto forgotten = backend_->ForgetDownloadResult(artifact.gid);
        const bool already_stopped =
            status.ok() &&
            (status.value->state == Aria2TransferState::kRemoved ||
             status.value->state == Aria2TransferState::kError);
        backend_clean = forgotten.ok() && *forgotten.value == "OK" &&
                        ((removed.ok() && *removed.value == artifact.gid) ||
                         already_stopped || !status.ok());
      }
    }
    success = success && backend_clean;
    if (directory.value >= 0) {
      success = RemoveDownloadArtifacts(directory.value, artifact.filename) &&
                success;
    }
    if (backend_clean) {
      artifact.gid.clear();
    }
  }
  if (directory.value >= 0 && fsync(directory.value) != 0) {
    success = false;
  }
  if (success) {
    artifacts_.clear();
  }
  return success;
}

void DashDownload::CleanPrivateFiles() {
  ScopedFd directory(open(download_directory_.c_str(),
                          O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
  if (directory.value < 0) {
    return;
  }
  static_cast<void>(RemoveDownloadArtifacts(directory.value,
                                             ManifestFilename(snapshot_.id)));
  for (const ArtifactRecord& artifact : artifacts_) {
    static_cast<void>(
        RemoveDownloadArtifacts(directory.value, artifact.filename));
  }
  static_cast<void>(
      UnlinkIfPresent(directory.value, TrackFilename(snapshot_.id, false)));
  static_cast<void>(
      UnlinkIfPresent(directory.value, TrackFilename(snapshot_.id, true)));
  static_cast<void>(
      UnlinkIfPresent(directory.value, MuxFilename(snapshot_.id)));
  static_cast<void>(fsync(directory.value));
}

void DashDownload::ClearRequestHeaders() {
  request_headers_.clear();
  request_headers_.shrink_to_fit();
}

void DashDownload::SetFailure(std::string_view code) {
  static_cast<void>(CleanManifest());
  static_cast<void>(CleanArtifacts(true));
  CleanPrivateFiles();
  ClearRequestHeaders();
  snapshot_.state = DashDownloadState::kFailed;
  snapshot_.bytes_per_second = 0;
  snapshot_.failure_code.assign(code.substr(0, 64));
}

}  // namespace fireball::transfer
