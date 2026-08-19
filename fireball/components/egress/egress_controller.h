#ifndef FIREBALL_COMPONENTS_EGRESS_EGRESS_CONTROLLER_H_
#define FIREBALL_COMPONENTS_EGRESS_EGRESS_CONTROLLER_H_

#include <map>
#include <optional>
#include <string>

#include "fireball/browser/domain_model.h"
#include "fireball/components/egress/egress_route.h"

namespace fireball::egress {

// Backends may start a Tor sidecar, probe a preconfigured WARP local proxy and
// apply Chromium's per-profile proxy configuration. Prepare/Verify may not
// alter the current route; Activate is the transaction commit point.
class EgressBackend {
 public:
  virtual ~EgressBackend() = default;

  virtual std::optional<EgressRoute> Prepare(
      const browser::ProfileId& profile_id,
      EgressMode mode,
      bool has_user_consent,
      std::string* error) = 0;
  virtual bool Verify(const browser::ProfileId& profile_id,
                      const EgressRoute& candidate,
                      std::string* error) = 0;
  virtual bool Activate(const browser::ProfileId& profile_id,
                        const EgressRoute& candidate,
                        std::string* error) = 0;
  virtual void Rollback(const browser::ProfileId& profile_id,
                        const EgressRoute& candidate) = 0;
  virtual void Retire(const browser::ProfileId& profile_id,
                      const EgressRoute& previous) = 0;
};

class EgressController final {
 public:
  explicit EgressController(EgressBackend* backend);

  bool AddProfile(browser::ProfileId profile_id);
  bool RemoveProfile(const browser::ProfileId& profile_id);

  // Mode changes require an idle profile because existing sockets cannot be
  // proven to have migrated. Non-direct modes additionally require an explicit
  // user action recorded by the default-deny network policy.
  bool Switch(const browser::ProfileId& profile_id,
              EgressMode mode,
              bool profile_is_idle,
              bool has_user_consent,
              std::string* error);

  const EgressRoute* GetRoute(const browser::ProfileId& profile_id) const;

 private:
  EgressBackend* backend_;
  std::map<browser::ProfileId, EgressRoute> routes_;
};

}  // namespace fireball::egress

#endif  // FIREBALL_COMPONENTS_EGRESS_EGRESS_CONTROLLER_H_
