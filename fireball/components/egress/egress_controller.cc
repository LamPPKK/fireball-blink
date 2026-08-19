#include "fireball/components/egress/egress_controller.h"

#include "fireball/components/privacy/network_audit.h"

#include <string>
#include <utility>

namespace fireball::egress {

EgressController::EgressController(EgressBackend* backend) : backend_(backend) {}

bool EgressController::AddProfile(browser::ProfileId profile_id) {
  return routes_.emplace(std::move(profile_id), MakeDirectRoute()).second;
}

bool EgressController::RemoveProfile(const browser::ProfileId& profile_id) {
  const auto route = routes_.find(profile_id);
  if (route == routes_.end()) {
    return false;
  }
  if (backend_ != nullptr) {
    backend_->Retire(profile_id, route->second);
  }
  routes_.erase(route);
  return true;
}

bool EgressController::Switch(const browser::ProfileId& profile_id,
                              EgressMode mode,
                              bool profile_is_idle,
                              bool has_user_consent,
                              std::string* error) {
  if (error == nullptr) {
    return false;
  }
  error->clear();
  const auto current = routes_.find(profile_id);
  if (current == routes_.end()) {
    *error = "unknown profile";
    return false;
  }
  if (current->second.mode == mode) {
    return true;
  }
  if (!profile_is_idle) {
    *error = "burn or close the active profile session before changing egress";
    return false;
  }
  if (mode != EgressMode::kDirect &&
      !privacy::IsNetworkRequestAllowed(
          NetworkPolicyOwner(mode), privacy::NetworkPhase::kPostStartup,
          has_user_consent)) {
    *error = "egress activation requires an explicit user action";
    return false;
  }
  if (backend_ == nullptr) {
    *error = "egress backend is unavailable";
    return false;
  }

  auto candidate =
      backend_->Prepare(profile_id, mode, has_user_consent, error);
  if (!candidate.has_value() || candidate->mode != mode ||
      !IsValidRoute(*candidate)) {
    if (candidate.has_value()) {
      backend_->Rollback(profile_id, *candidate);
    }
    if (error->empty()) {
      *error = "egress backend produced an invalid route";
    }
    return false;
  }
  if (!backend_->Verify(profile_id, *candidate, error) ||
      !backend_->Activate(profile_id, *candidate, error)) {
    backend_->Rollback(profile_id, *candidate);
    if (error->empty()) {
      *error = "egress transaction failed";
    }
    return false;
  }

  EgressRoute previous = current->second;
  current->second = *candidate;
  backend_->Retire(profile_id, previous);
  return true;
}

const EgressRoute* EgressController::GetRoute(
    const browser::ProfileId& profile_id) const {
  const auto route = routes_.find(profile_id);
  return route == routes_.end() ? nullptr : &route->second;
}

}  // namespace fireball::egress
