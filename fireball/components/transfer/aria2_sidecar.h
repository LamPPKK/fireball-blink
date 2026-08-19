#ifndef FIREBALL_COMPONENTS_TRANSFER_ARIA2_SIDECAR_H_
#define FIREBALL_COMPONENTS_TRANSFER_ARIA2_SIDECAR_H_

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "fireball/components/transfer/aria2_rpc_client.h"

namespace fireball::transfer {

struct Aria2SidecarConfig {
  std::filesystem::path executable;
  std::filesystem::path downloads_directory;
  std::filesystem::path runtime_directory;
  TransferPersistence persistence = TransferPersistence::kPersistent;
  bool has_user_consent = false;
  std::uint16_t rpc_port = 0;
  int maximum_concurrent_downloads = 3;
  int connections_per_download = 4;
  std::optional<std::string> outbound_http_proxy;
  bool allow_peer_to_peer = true;
};

// Owns one foreground aria2 process for one Fireball storage boundary. The RPC
// endpoint is loopback-only. Its random secret is written to a private runtime
// config (never argv) and that file is unlinked as soon as aria2 is ready.
class Aria2Sidecar final {
 public:
  static std::unique_ptr<Aria2Sidecar> Launch(
      const Aria2SidecarConfig& config,
      std::string* error);

  ~Aria2Sidecar();

  Aria2Sidecar(const Aria2Sidecar&) = delete;
  Aria2Sidecar& operator=(const Aria2Sidecar&) = delete;

  Aria2RpcClient& rpc() { return rpc_; }
  int process_id_for_testing() const { return process_id_; }
  void Stop();

 private:
  Aria2Sidecar(int process_id,
               std::uint16_t rpc_port,
               std::string secret,
               TransferPersistence persistence,
               bool allow_peer_to_peer,
               std::filesystem::path private_config_path);

  int process_id_;
  Aria2RpcClient rpc_;
  std::filesystem::path private_config_path_;
};

bool ValidateAria2SidecarConfig(const Aria2SidecarConfig& config,
                                std::string* error);

}  // namespace fireball::transfer

#endif  // FIREBALL_COMPONENTS_TRANSFER_ARIA2_SIDECAR_H_
