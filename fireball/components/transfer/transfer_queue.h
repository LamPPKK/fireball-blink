#ifndef FIREBALL_COMPONENTS_TRANSFER_TRANSFER_QUEUE_H_
#define FIREBALL_COMPONENTS_TRANSFER_TRANSFER_QUEUE_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "fireball/components/transfer/aria2_rpc_client.h"
#include "fireball/components/transfer/transfer_types.h"

namespace fireball::transfer {

enum class TransferState {
  kQueued,
  kActive,
  kPaused,
  kComplete,
  kFailed,
  kCancelled,
};

struct TransferItem {
  std::string id;
  std::string display_name;
  TransferSourceKind source_kind;
  TransferPersistence persistence;
  MediaCandidateKind media_kind;
  TransferState state = TransferState::kQueued;
  std::uint64_t total_bytes = 0;
  std::uint64_t completed_bytes = 0;
  std::uint64_t bytes_per_second = 0;
  std::string failure_code;
};

bool IsTerminalTransferState(TransferState state);

// Product-facing queue over one storage-bound aria2 backend. Requests are sent
// to the backend synchronously and are never retained, so signed media URLs,
// magnet URIs and uploaded metainfo do not become history in this model.
class TransferQueue final {
 public:
  TransferQueue(TransferBackend* backend, TransferPersistence persistence);

  bool Enqueue(std::string id,
               const TransferRequest& request,
               std::string display_name,
               MediaCandidateKind media_kind = MediaCandidateKind::kNone);
  bool Refresh(std::string_view id);
  std::size_t RefreshAll();
  bool Pause(std::string_view id);
  bool Resume(std::string_view id);
  bool Cancel(std::string_view id);
  bool ForgetFinished(std::string_view id);

  const TransferItem* Find(std::string_view id) const;
  std::vector<TransferItem> Snapshot() const;
  const std::string& last_control_error() const { return last_control_error_; }

 private:
  struct Record {
    TransferItem item;
    std::string gid;
  };

  bool ApplyControlResult(Record* record,
                          Aria2RpcResult<std::string> result,
                          TransferState next_state);
  void SetControlError(std::string_view error);

  TransferBackend* backend_;
  TransferPersistence persistence_;
  std::map<std::string, Record, std::less<>> records_;
  std::vector<std::string> order_;
  std::string last_control_error_;
};

}  // namespace fireball::transfer

#endif  // FIREBALL_COMPONENTS_TRANSFER_TRANSFER_QUEUE_H_
