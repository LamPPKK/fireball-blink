#include "fireball/components/transfer/transfer_queue.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <utility>

namespace fireball::transfer {
namespace {

constexpr std::size_t kMaximumQueueItems = 512;
constexpr std::size_t kMaximumControlErrorBytes = 256;

std::string SanitizeControlError(std::string_view error) {
  std::string sanitized;
  sanitized.reserve(std::min(error.size(), kMaximumControlErrorBytes));
  for (const unsigned char character : error) {
    if (sanitized.size() == kMaximumControlErrorBytes) {
      break;
    }
    if (character >= 0x20 && character != 0x7f) {
      sanitized.push_back(static_cast<char>(character));
    }
  }
  std::string lowered = sanitized;
  std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](char value) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
  });
  if (lowered.find("http://") != std::string::npos ||
      lowered.find("https://") != std::string::npos ||
      lowered.find("magnet:?") != std::string::npos ||
      lowered.find("token=") != std::string::npos ||
      lowered.find("secret") != std::string::npos) {
    return "transfer backend request failed";
  }
  return sanitized.empty() ? "transfer backend request failed" : sanitized;
}

std::optional<TransferState> MapBackendState(Aria2TransferState state) {
  switch (state) {
    case Aria2TransferState::kWaiting:
      return TransferState::kQueued;
    case Aria2TransferState::kPaused:
      return TransferState::kPaused;
    case Aria2TransferState::kActive:
      return TransferState::kActive;
    case Aria2TransferState::kComplete:
      return TransferState::kComplete;
    case Aria2TransferState::kError:
      return TransferState::kFailed;
    case Aria2TransferState::kRemoved:
      return TransferState::kCancelled;
    case Aria2TransferState::kUnknown:
      return std::nullopt;
  }
  return std::nullopt;
}

bool IsSafeBackendFailureCode(std::string_view code) {
  return !code.empty() && code.size() <= 4 &&
         std::all_of(code.begin(), code.end(), [](char value) {
           return value >= '0' && value <= '9';
         });
}

}  // namespace

bool IsTerminalTransferState(TransferState state) {
  return state == TransferState::kComplete || state == TransferState::kFailed ||
         state == TransferState::kCancelled;
}

TransferQueue::TransferQueue(TransferBackend* backend,
                             TransferPersistence persistence)
    : backend_(backend), persistence_(persistence) {}

bool TransferQueue::Enqueue(std::string id,
                            const TransferRequest& request,
                            std::string display_name,
                            MediaCandidateKind media_kind) {
  if (backend_ == nullptr || !IsCanonicalTransferId(id) ||
      request.persistence != persistence_ || records_.contains(id) ||
      records_.size() >= kMaximumQueueItems ||
      !IsSafeOutputName(display_name) || !IsValidTransferRequest(request)) {
    SetControlError("invalid transfer queue request");
    return false;
  }
  auto result = backend_->Enqueue(request);
  if (!result.ok() || !IsValidAria2Gid(*result.value)) {
    SetControlError(result.ok() ? "transfer backend returned an invalid id"
                                : result.error);
    return false;
  }
  const bool duplicate_gid = std::any_of(
      records_.begin(), records_.end(), [&result](const auto& entry) {
        return entry.second.gid == *result.value;
      });
  if (duplicate_gid) {
    SetControlError("transfer backend returned a duplicate id");
    return false;
  }

  TransferItem item{std::move(id),
                    std::move(display_name),
                    request.source_kind,
                    request.persistence,
                    media_kind,
                    TransferState::kQueued,
                    0,
                    0,
                    0,
                    {}};
  const std::string key = item.id;
  records_.emplace(key, Record{std::move(item), std::move(*result.value)});
  order_.push_back(key);
  last_control_error_.clear();
  return true;
}

bool TransferQueue::Refresh(std::string_view id) {
  auto record = records_.find(id);
  if (backend_ == nullptr || record == records_.end()) {
    SetControlError("unknown transfer");
    return false;
  }
  if (IsTerminalTransferState(record->second.item.state)) {
    last_control_error_.clear();
    return true;
  }
  auto status = backend_->TellStatus(record->second.gid);
  if (!status.ok()) {
    SetControlError(status.error);
    return false;
  }
  const auto state = MapBackendState(status.value->state);
  if (!state.has_value() || status.value->gid != record->second.gid ||
      (status.value->total_bytes != 0 &&
       status.value->completed_bytes > status.value->total_bytes)) {
    SetControlError("transfer backend returned malformed status");
    return false;
  }

  TransferItem& item = record->second.item;
  item.state = *state;
  item.total_bytes = status.value->total_bytes;
  item.completed_bytes = status.value->completed_bytes;
  item.bytes_per_second = status.value->bytes_per_second;
  item.failure_code.clear();
  if (*state == TransferState::kFailed) {
    item.failure_code =
        IsSafeBackendFailureCode(status.value->error_code)
            ? "ARIA2_" + status.value->error_code
            : "TRANSFER_FAILED";
  }
  last_control_error_.clear();
  return true;
}

std::size_t TransferQueue::RefreshAll() {
  std::size_t refreshed = 0;
  std::string last_error;
  last_control_error_.clear();
  for (const std::string& id : order_) {
    if (Refresh(id)) {
      ++refreshed;
    } else {
      last_error = last_control_error_;
    }
  }
  if (!last_error.empty()) {
    last_control_error_ = std::move(last_error);
  }
  return refreshed;
}

bool TransferQueue::ApplyControlResult(Record* record,
                                       Aria2RpcResult<std::string> result,
                                       TransferState next_state) {
  if (record == nullptr || !result.ok() || *result.value != record->gid) {
    SetControlError(result.ok() ? "transfer backend response id mismatch"
                                : result.error);
    return false;
  }
  record->item.state = next_state;
  record->item.bytes_per_second = 0;
  last_control_error_.clear();
  return true;
}

bool TransferQueue::Pause(std::string_view id) {
  auto record = records_.find(id);
  if (backend_ == nullptr || record == records_.end() ||
      (record->second.item.state != TransferState::kQueued &&
       record->second.item.state != TransferState::kActive)) {
    SetControlError("transfer cannot be paused in its current state");
    return false;
  }
  return ApplyControlResult(&record->second,
                            backend_->Pause(record->second.gid),
                            TransferState::kPaused);
}

bool TransferQueue::Resume(std::string_view id) {
  auto record = records_.find(id);
  if (backend_ == nullptr || record == records_.end() ||
      record->second.item.state != TransferState::kPaused) {
    SetControlError("transfer cannot be resumed in its current state");
    return false;
  }
  return ApplyControlResult(&record->second,
                            backend_->Unpause(record->second.gid),
                            TransferState::kQueued);
}

bool TransferQueue::Cancel(std::string_view id) {
  auto record = records_.find(id);
  if (backend_ == nullptr || record == records_.end() ||
      IsTerminalTransferState(record->second.item.state)) {
    SetControlError("transfer cannot be cancelled in its current state");
    return false;
  }
  return ApplyControlResult(&record->second,
                            backend_->Remove(record->second.gid),
                            TransferState::kCancelled);
}

bool TransferQueue::ForgetFinished(std::string_view id) {
  auto record = records_.find(id);
  if (backend_ == nullptr || record == records_.end() ||
      !IsTerminalTransferState(record->second.item.state)) {
    SetControlError("only a finished transfer can be forgotten");
    return false;
  }
  auto result = backend_->ForgetDownloadResult(record->second.gid);
  if (!result.ok() || *result.value != "OK") {
    SetControlError(result.ok() ? "transfer backend did not forget result"
                                : result.error);
    return false;
  }
  const std::string key(record->first);
  records_.erase(record);
  std::erase(order_, key);
  last_control_error_.clear();
  return true;
}

const TransferItem* TransferQueue::Find(std::string_view id) const {
  auto record = records_.find(id);
  return record == records_.end() ? nullptr : &record->second.item;
}

std::vector<TransferItem> TransferQueue::Snapshot() const {
  std::vector<TransferItem> snapshot;
  snapshot.reserve(order_.size());
  for (const std::string& id : order_) {
    auto record = records_.find(id);
    if (record != records_.end()) {
      snapshot.push_back(record->second.item);
    }
  }
  return snapshot;
}

void TransferQueue::SetControlError(std::string_view error) {
  last_control_error_ = SanitizeControlError(error);
}

}  // namespace fireball::transfer
