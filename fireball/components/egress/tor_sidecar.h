#ifndef FIREBALL_COMPONENTS_EGRESS_TOR_SIDECAR_H_
#define FIREBALL_COMPONENTS_EGRESS_TOR_SIDECAR_H_

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

#include "fireball/browser/domain_model.h"
#include "fireball/components/egress/egress_route.h"

namespace fireball::egress {

struct TorSidecarConfig {
  std::filesystem::path executable;
  std::filesystem::path runtime_directory;
  browser::ProfileId profile_id;
  std::uint16_t socks5_port = 0;
  std::uint16_t http_connect_port = 0;
  bool has_user_consent = false;
};

class TorSidecar final {
 public:
  static std::unique_ptr<TorSidecar> Launch(const TorSidecarConfig& config,
                                            std::string* error);
  ~TorSidecar();

  TorSidecar(const TorSidecar&) = delete;
  TorSidecar& operator=(const TorSidecar&) = delete;

  const EgressRoute& route() const { return route_; }
  int process_id_for_testing() const { return process_id_; }
  const std::filesystem::path& data_directory_for_testing() const {
    return data_directory_;
  }
  void Stop();

 private:
  TorSidecar(int process_id,
             EgressRoute route,
             std::filesystem::path config_path,
             std::filesystem::path data_directory);

  int process_id_;
  EgressRoute route_;
  std::filesystem::path config_path_;
  std::filesystem::path data_directory_;
};

bool ValidateTorSidecarConfig(const TorSidecarConfig& config,
                              std::string* error);

}  // namespace fireball::egress

#endif  // FIREBALL_COMPONENTS_EGRESS_TOR_SIDECAR_H_
