#ifndef FIREBALL_COMPONENTS_TRANSFER_ARIA2_RPC_CLIENT_H_
#define FIREBALL_COMPONENTS_TRANSFER_ARIA2_RPC_CLIENT_H_

#include <cstdint>
#include <optional>
#include <string>

#include "fireball/components/transfer/transfer_types.h"

namespace fireball::transfer {

template <typename T>
struct Aria2RpcResult {
  std::optional<T> value;
  std::string error;

  bool ok() const { return value.has_value(); }
};

enum class Aria2TransferState {
  kWaiting,
  kPaused,
  kActive,
  kComplete,
  kError,
  kRemoved,
  kUnknown,
};

struct Aria2TransferStatus {
  std::string gid;
  Aria2TransferState state = Aria2TransferState::kUnknown;
  std::uint64_t total_bytes = 0;
  std::uint64_t completed_bytes = 0;
  std::uint64_t bytes_per_second = 0;
  std::string error_code;
  std::string error_message;
};

// A deliberately small JSON-RPC 2.0 client. It can connect only to IPv4
// loopback and caps every response, so browser-provided URIs never become an
// arbitrary RPC/network primitive.
class Aria2RpcClient final {
 public:
  Aria2RpcClient(std::uint16_t port,
                 std::string secret,
                 TransferPersistence persistence);
  ~Aria2RpcClient();

  Aria2RpcClient(const Aria2RpcClient&) = delete;
  Aria2RpcClient& operator=(const Aria2RpcClient&) = delete;

  bool IsConfigurationValid() const;

  Aria2RpcResult<std::string> GetVersion();
  Aria2RpcResult<std::string> Enqueue(const TransferRequest& request);
  Aria2RpcResult<Aria2TransferStatus> TellStatus(std::string_view gid);
  Aria2RpcResult<std::string> Pause(std::string_view gid);
  Aria2RpcResult<std::string> Unpause(std::string_view gid);
  Aria2RpcResult<std::string> Remove(std::string_view gid);
  Aria2RpcResult<std::string> ForceShutdown();

 private:
  Aria2RpcResult<std::string> CallForString(std::string_view method,
                                           std::string params_json);
  Aria2RpcResult<std::string> Call(std::string_view method,
                                  std::string params_json);

  std::uint16_t port_;
  std::string secret_;
  TransferPersistence persistence_;
  std::uint64_t next_request_id_ = 1;
};

bool IsValidAria2Secret(std::string_view secret);
bool IsValidAria2Gid(std::string_view gid);

}  // namespace fireball::transfer

#endif  // FIREBALL_COMPONENTS_TRANSFER_ARIA2_RPC_CLIENT_H_
