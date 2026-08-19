#include "fireball/components/transfer/hls_download.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <optional>
#include <string>
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

bool IsTerminal(HlsDownloadState state) {
  return state == HlsDownloadState::kComplete ||
         state == HlsDownloadState::kFailed ||
         state == HlsDownloadState::kCancelled;
}

bool IsSafeDownloadDirectory(const std::filesystem::path& path) {
  if (!path.is_absolute()) {
    return false;
  }
  struct stat status {};
  return lstat(path.c_str(), &status) == 0 && S_ISDIR(status.st_mode) &&
         status.st_uid == getuid() && (status.st_mode & 0022) == 0;
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

bool RemoveManifestArtifacts(int directory, std::string_view filename) {
  const bool manifest_removed = UnlinkIfPresent(directory, filename);
  const bool control_removed =
      UnlinkIfPresent(directory, std::string(filename) + ".aria2");
  return manifest_removed && control_removed;
}

std::string ManifestFilename(std::string_view id, bool variant) {
  return ".fireball-hls-" + std::string(id) +
         (variant ? "-variant.m3u8" : "-entry.m3u8");
}

bool IsMpegTsOutput(std::string_view name) {
  std::string lowered(name);
  std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                 [](char character) {
                   return static_cast<char>(std::tolower(
                       static_cast<unsigned char>(character)));
                 });
  return lowered.ends_with(".ts");
}

std::optional<std::string> ReadBoundedManifest(
    const std::filesystem::path& directory_path,
    std::string_view filename) {
  ScopedFd directory(open(directory_path.c_str(),
                          O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
  if (directory.value < 0) {
    return std::nullopt;
  }
  ScopedFd input(openat(directory.value, std::string(filename).c_str(),
                        O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
  struct stat status {};
  if (input.value < 0 || fstat(input.value, &status) != 0 ||
      !S_ISREG(status.st_mode) || status.st_uid != getuid() ||
      status.st_nlink != 1 || status.st_size <= 0 ||
      static_cast<std::uint64_t>(status.st_size) >
          kMaximumHlsManifestBytes) {
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
      std::fill(body.begin(), body.end(), '\0');
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
    std::fill(body.begin(), body.end(), '\0');
    return std::nullopt;
  }
  return std::move(body);
}

std::string_view ParseFailureCode(HlsVodError first, HlsVodError second) {
  if (first == HlsVodError::kLimitExceeded ||
      second == HlsVodError::kLimitExceeded) {
    return HlsVodErrorCode(HlsVodError::kLimitExceeded);
  }
  if (first == HlsVodError::kUnsafeUri || second == HlsVodError::kUnsafeUri) {
    return HlsVodErrorCode(HlsVodError::kUnsafeUri);
  }
  if (first == HlsVodError::kInvalidManifest ||
      second == HlsVodError::kInvalidManifest) {
    return HlsVodErrorCode(HlsVodError::kInvalidManifest);
  }
  return HlsVodErrorCode(HlsVodError::kUnsupportedManifest);
}

void Erase(std::string* value) {
  std::fill(value->begin(), value->end(), '\0');
  value->clear();
}

}  // namespace

HlsDownload::HlsDownload(TransferBackend* backend,
                         TransferPersistence persistence,
                         std::string id,
                         std::filesystem::path download_directory,
                         std::uint64_t maximum_bandwidth)
    : backend_(backend),
      persistence_(persistence),
      download_directory_(std::move(download_directory)),
      maximum_bandwidth_(maximum_bandwidth) {
  snapshot_.id = std::move(id);
}

HlsDownload::~HlsDownload() {
  if (!IsTerminal(snapshot_.state) &&
      snapshot_.state != HlsDownloadState::kIdle) {
    BestEffortClean();
  }
}

bool HlsDownload::Start(std::string manifest_uri, std::string output_name) {
  if (backend_ == nullptr || snapshot_.state != HlsDownloadState::kIdle ||
      !IsCanonicalTransferId(snapshot_.id) ||
      !IsSafeDownloadDirectory(download_directory_) ||
      !IsSafeHttpDownloadUri(manifest_uri) ||
      manifest_uri.find('#') != std::string::npos ||
      !IsSafeOutputName(output_name) || !IsMpegTsOutput(output_name)) {
    Erase(&manifest_uri);
    SetFailure("HLS_INVALID_JOB");
    return false;
  }
  ScopedFd directory(open(download_directory_.c_str(),
                          O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
  if (directory.value < 0 || !DoesNotExistAt(directory.value, output_name)) {
    Erase(&manifest_uri);
    SetFailure("HLS_OUTPUT_CONFLICT");
    return false;
  }
  snapshot_.output_name = std::move(output_name);
  if (!EnqueueManifest(ManifestKind::kEntry, std::move(manifest_uri))) {
    SetFailure("HLS_MANIFEST_ENQUEUE_FAILED");
    return false;
  }
  snapshot_.state = HlsDownloadState::kFetchingManifest;
  snapshot_.failure_code.clear();
  return true;
}

bool HlsDownload::EnqueueManifest(ManifestKind kind, std::string uri) {
  const std::string filename = ManifestFilename(
      snapshot_.id, kind == ManifestKind::kVariant);
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
                          false};
  auto result = backend_->Enqueue(request);
  Erase(&request.source);
  if (!result.ok() || !IsValidAria2Gid(*result.value)) {
    static_cast<void>(RemoveManifestArtifacts(directory.value, filename));
    Erase(&uri);
    return false;
  }
  manifest_ = {kind, std::move(uri), filename, std::move(*result.value),
               Aria2TransferState::kWaiting};
  return true;
}

bool HlsDownload::Refresh() {
  if (snapshot_.state == HlsDownloadState::kFetchingManifest ||
      snapshot_.state == HlsDownloadState::kFetchingVariant) {
    return RefreshManifest();
  }
  if (snapshot_.state != HlsDownloadState::kDownloadingSegments &&
      snapshot_.state != HlsDownloadState::kAssembling) {
    return false;
  }
  if (segment_session_ == nullptr) {
    SetFailure("HLS_INTERNAL_STATE");
    return false;
  }
  const bool refreshed = segment_session_->Refresh();
  SyncSegmentSnapshot();
  return refreshed;
}

bool HlsDownload::RefreshManifest() {
  auto result = backend_->TellStatus(manifest_.gid);
  if (!result.ok() || result.value->gid != manifest_.gid ||
      result.value->state == Aria2TransferState::kUnknown ||
      result.value->state == Aria2TransferState::kError ||
      result.value->state == Aria2TransferState::kRemoved ||
      (result.value->total_bytes != 0 &&
       result.value->completed_bytes > result.value->total_bytes) ||
      result.value->total_bytes > kMaximumHlsManifestBytes ||
      result.value->completed_bytes > kMaximumHlsManifestBytes) {
    BestEffortClean();
    SetFailure(result.ok() &&
                       (result.value->total_bytes > kMaximumHlsManifestBytes ||
                        result.value->completed_bytes >
                            kMaximumHlsManifestBytes)
                   ? "HLS_MANIFEST_SIZE_LIMIT"
                   : "HLS_MANIFEST_FETCH_FAILED");
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
    paused_from_ = snapshot_.state;
    snapshot_.state = HlsDownloadState::kPaused;
  }
  return true;
}

bool HlsDownload::ConsumeManifest() {
  const ManifestKind kind = manifest_.kind;
  std::string manifest_uri = std::move(manifest_.uri);
  auto body = ReadBoundedManifest(download_directory_, manifest_.filename);
  const std::string gid = manifest_.gid;
  const std::string filename = manifest_.filename;
  auto forgotten = backend_->ForgetDownloadResult(gid);
  ScopedFd directory(open(download_directory_.c_str(),
                          O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
  const bool result_cleaned = forgotten.ok() && *forgotten.value == "OK";
  const bool artifacts_cleaned =
      directory.value >= 0 &&
      RemoveManifestArtifacts(directory.value, filename) &&
      fsync(directory.value) == 0;
  const bool cleaned = result_cleaned && artifacts_cleaned;
  if (!body.has_value() || !cleaned) {
    if (body.has_value()) {
      Erase(&*body);
    }
    Erase(&manifest_uri);
    BestEffortClean();
    SetFailure(body.has_value() ? "HLS_MANIFEST_CLEANUP_FAILED"
                                : "HLS_MANIFEST_READ_FAILED");
    return false;
  }
  manifest_ = {};
  ++snapshot_.manifest_fetches_completed;
  snapshot_.total_bytes = 0;
  snapshot_.completed_bytes = 0;
  snapshot_.bytes_per_second = 0;

  if (kind == ManifestKind::kVariant) {
    auto parsed = ParseHlsVodPlaylist(manifest_uri, *body);
    Erase(&*body);
    Erase(&manifest_uri);
    if (!parsed.ok()) {
      SetFailure(HlsVodErrorCode(parsed.error));
      return false;
    }
    return StartSegments(std::move(*parsed.value));
  }

  auto media = ParseHlsVodPlaylist(manifest_uri, *body);
  if (media.ok()) {
    Erase(&*body);
    Erase(&manifest_uri);
    return StartSegments(std::move(*media.value));
  }
  auto master = ParseHlsMasterPlaylist(manifest_uri, *body);
  Erase(&*body);
  Erase(&manifest_uri);
  if (!master.ok()) {
    SetFailure(ParseFailureCode(media.error, master.error));
    return false;
  }
  auto selected = SelectHlsVariant(*master.value, maximum_bandwidth_);
  if (!selected.has_value()) {
    SetFailure("HLS_VARIANT_UNAVAILABLE");
    return false;
  }
  snapshot_.selected_bandwidth = selected->bandwidth;
  snapshot_.selected_width = selected->width;
  snapshot_.selected_height = selected->height;
  std::string selected_uri = std::move(selected->uri);
  for (HlsVariant& variant : master.value->variants) {
    Erase(&variant.uri);
  }
  if (!EnqueueManifest(ManifestKind::kVariant, std::move(selected_uri))) {
    SetFailure("HLS_VARIANT_ENQUEUE_FAILED");
    return false;
  }
  snapshot_.state = HlsDownloadState::kFetchingVariant;
  return true;
}

bool HlsDownload::StartSegments(HlsVodPlan plan) {
  segment_session_ = std::make_unique<HlsVodSession>(
      backend_, persistence_, snapshot_.id, download_directory_);
  if (!segment_session_->Start(std::move(plan), snapshot_.output_name)) {
    const std::string code = segment_session_->snapshot().failure_code;
    segment_session_.reset();
    SetFailure(code.empty() ? "HLS_SEGMENT_START_FAILED" : code);
    return false;
  }
  snapshot_.state = HlsDownloadState::kDownloadingSegments;
  SyncSegmentSnapshot();
  return true;
}

void HlsDownload::SyncSegmentSnapshot() {
  if (segment_session_ == nullptr) {
    return;
  }
  const HlsVodJobSnapshot& child = segment_session_->snapshot();
  snapshot_.segment_count = child.segment_count;
  snapshot_.completed_segments = child.completed_segments;
  snapshot_.total_bytes = child.total_bytes;
  snapshot_.completed_bytes = child.completed_bytes;
  snapshot_.bytes_per_second = child.bytes_per_second;
  snapshot_.failure_code = child.failure_code;
  switch (child.state) {
    case HlsVodJobState::kQueued:
    case HlsVodJobState::kActive:
      snapshot_.state = HlsDownloadState::kDownloadingSegments;
      break;
    case HlsVodJobState::kPaused:
      paused_from_ = HlsDownloadState::kDownloadingSegments;
      snapshot_.state = HlsDownloadState::kPaused;
      break;
    case HlsVodJobState::kAssembling:
      snapshot_.state = HlsDownloadState::kAssembling;
      break;
    case HlsVodJobState::kComplete:
      snapshot_.state = HlsDownloadState::kComplete;
      snapshot_.failure_code.clear();
      break;
    case HlsVodJobState::kFailed:
      SetFailure(child.failure_code.empty() ? "HLS_SEGMENT_FAILED"
                                            : child.failure_code);
      break;
    case HlsVodJobState::kCancelled:
      snapshot_.state = HlsDownloadState::kCancelled;
      snapshot_.failure_code.clear();
      break;
    case HlsVodJobState::kIdle:
      SetFailure("HLS_INTERNAL_STATE");
      break;
  }
}

bool HlsDownload::Pause() {
  if (snapshot_.state == HlsDownloadState::kFetchingManifest ||
      snapshot_.state == HlsDownloadState::kFetchingVariant) {
    const HlsDownloadState previous = snapshot_.state;
    auto result = backend_->Pause(manifest_.gid);
    if (!result.ok() || *result.value != manifest_.gid) {
      snapshot_.failure_code = "HLS_CONTROL_FAILED";
      return false;
    }
    manifest_.state = Aria2TransferState::kPaused;
    paused_from_ = previous;
    snapshot_.state = HlsDownloadState::kPaused;
    snapshot_.bytes_per_second = 0;
    snapshot_.failure_code.clear();
    return true;
  }
  if (snapshot_.state != HlsDownloadState::kDownloadingSegments ||
      segment_session_ == nullptr) {
    return false;
  }
  if (!segment_session_->Pause()) {
    snapshot_.failure_code = segment_session_->snapshot().failure_code;
    return false;
  }
  SyncSegmentSnapshot();
  return true;
}

bool HlsDownload::Resume() {
  if (snapshot_.state != HlsDownloadState::kPaused) {
    return false;
  }
  if (paused_from_ == HlsDownloadState::kFetchingManifest ||
      paused_from_ == HlsDownloadState::kFetchingVariant) {
    auto result = backend_->Unpause(manifest_.gid);
    if (!result.ok() || *result.value != manifest_.gid) {
      snapshot_.failure_code = "HLS_CONTROL_FAILED";
      return false;
    }
    manifest_.state = Aria2TransferState::kWaiting;
    snapshot_.state = paused_from_;
    paused_from_ = HlsDownloadState::kIdle;
    snapshot_.failure_code.clear();
    return true;
  }
  if (paused_from_ != HlsDownloadState::kDownloadingSegments ||
      segment_session_ == nullptr) {
    return false;
  }
  if (!segment_session_->Resume()) {
    snapshot_.failure_code = segment_session_->snapshot().failure_code;
    return false;
  }
  paused_from_ = HlsDownloadState::kIdle;
  SyncSegmentSnapshot();
  return true;
}

bool HlsDownload::Cancel() {
  if (snapshot_.state == HlsDownloadState::kIdle ||
      IsTerminal(snapshot_.state)) {
    return false;
  }
  bool success = true;
  if (segment_session_ != nullptr) {
    success = segment_session_->Cancel();
  } else {
    success = CleanManifestFetch();
  }
  snapshot_.bytes_per_second = 0;
  if (success) {
    snapshot_.state = HlsDownloadState::kCancelled;
    snapshot_.failure_code.clear();
  } else {
    SetFailure("HLS_CANCEL_INCOMPLETE");
  }
  return success;
}

bool HlsDownload::CleanManifestFetch() {
  bool success = true;
  if (!manifest_.gid.empty() && backend_ != nullptr) {
    bool result_removed = false;
    auto status = backend_->TellStatus(manifest_.gid);
    if (status.ok() && status.value->state == Aria2TransferState::kComplete) {
      auto forgotten = backend_->ForgetDownloadResult(manifest_.gid);
      result_removed = forgotten.ok() && *forgotten.value == "OK";
    } else {
      auto removed = backend_->Remove(manifest_.gid);
      const bool remove_ok =
          removed.ok() && *removed.value == manifest_.gid;
      auto forgotten = backend_->ForgetDownloadResult(manifest_.gid);
      const bool forget_ok = forgotten.ok() && *forgotten.value == "OK";
      const bool already_stopped =
          status.ok() &&
          (status.value->state == Aria2TransferState::kRemoved ||
           status.value->state == Aria2TransferState::kError);
      result_removed =
          forget_ok && (remove_ok || already_stopped || !status.ok());
    }
    success = success && result_removed;
  }
  ScopedFd directory(open(download_directory_.c_str(),
                          O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
  if (directory.value < 0) {
    success = false;
  } else if (!manifest_.filename.empty()) {
    success = RemoveManifestArtifacts(directory.value, manifest_.filename) &&
              fsync(directory.value) == 0 && success;
  }
  Erase(&manifest_.uri);
  manifest_.filename.clear();
  manifest_.gid.clear();
  return success;
}

void HlsDownload::BestEffortClean() {
  static_cast<void>(CleanManifestFetch());
  segment_session_.reset();
  snapshot_.bytes_per_second = 0;
}

void HlsDownload::SetFailure(std::string_view code) {
  snapshot_.state = HlsDownloadState::kFailed;
  snapshot_.bytes_per_second = 0;
  snapshot_.failure_code.assign(code.substr(0, 64));
}

}  // namespace fireball::transfer
