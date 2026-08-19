#ifndef FIREBALL_COMPONENTS_EGRESS_RUNTIME_BACKEND_H_
#define FIREBALL_COMPONENTS_EGRESS_RUNTIME_BACKEND_H_

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>

#include "fireball/browser/domain_model.h"
#include "fireball/components/egress/egress_controller.h"
#include "fireball/components/egress/egress_route.h"
#include "fireball/components/egress/tor_sidecar.h"

namespace fireball::egress {

// Chromium supplies this delegate at the product boundary. A route cannot
// commit until both the public-IP/DNS check and per-profile proxy application
// succeed.
class RuntimeEgressDelegate {
 public:
  virtual ~RuntimeEgressDelegate() = default;

  virtual std::optional<LoopbackProxyPorts> AllocateTorPorts(
      const browser::ProfileId& profile_id,
      std::string* error) = 0;
  virtual bool VerifyPublicIpAndDns(const browser::ProfileId& profile_id,
                                    const EgressRoute& candidate,
                                    std::string* error) = 0;
  virtual bool ApplyChromiumProxyRules(
      const browser::ProfileId& profile_id,
      std::string proxy_rules,
      std::string* error) = 0;
};

struct RuntimeEgressConfig {
  std::filesystem::path tor_executable;
  std::filesystem::path tor_runtime_directory;
  std::uint16_t warp_local_proxy_port = 40000;
  std::chrono::milliseconds readiness_timeout = std::chrono::seconds(2);
  RuntimeEgressDelegate* delegate = nullptr;
};

// Production lifecycle implementation behind EgressController. Candidate Tor
// processes stay in prepared_tor_ until Chromium commits the route. Rollback
// destroys prepared state; Retire destroys the previous active Tor process.
class RuntimeEgressBackend final : public EgressBackend {
 public:
  explicit RuntimeEgressBackend(RuntimeEgressConfig config);
  ~RuntimeEgressBackend() override;

  RuntimeEgressBackend(const RuntimeEgressBackend&) = delete;
  RuntimeEgressBackend& operator=(const RuntimeEgressBackend&) = delete;

  std::optional<EgressRoute> Prepare(
      const browser::ProfileId& profile_id,
      EgressMode mode,
      bool has_user_consent,
      std::string* error) override;
  bool Verify(const browser::ProfileId& profile_id,
              const EgressRoute& candidate,
              std::string* error) override;
  bool Activate(const browser::ProfileId& profile_id,
                const EgressRoute& candidate,
                std::string* error) override;
  void Rollback(const browser::ProfileId& profile_id,
                const EgressRoute& candidate) override;
  void Retire(const browser::ProfileId& profile_id,
              const EgressRoute& previous) override;

 private:
  RuntimeEgressConfig config_;
  std::map<browser::ProfileId, std::unique_ptr<TorSidecar>> prepared_tor_;
  std::map<browser::ProfileId, std::unique_ptr<TorSidecar>> active_tor_;
};

}  // namespace fireball::egress

#endif  // FIREBALL_COMPONENTS_EGRESS_RUNTIME_BACKEND_H_
