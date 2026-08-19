#ifndef FIREBALL_COMPONENTS_EGRESS_EGRESS_VERIFICATION_H_
#define FIREBALL_COMPONENTS_EGRESS_EGRESS_VERIFICATION_H_

#include <cstdint>
#include <string>
#include <string_view>

#include "fireball/components/egress/egress_route.h"

namespace fireball::egress {

enum class ProviderAttestation {
  kNone,
  kWarp,
  kTor,
};

// Collected synchronously by the trusted Chromium runtime delegate. Raw probe
// responses and DNS names never cross this boundary. The native policy still
// validates that the evidence matches the exact candidate route before commit.
struct EgressVerificationEvidence {
  EgressMode observed_mode = EgressMode::kDirect;
  std::uint16_t observed_proxy_port = 0;
  bool public_ip_probe_used_candidate_route = false;
  bool dns_probe_used_candidate_route = false;
  bool remote_dns_confirmed = false;
  bool local_dns_observed = false;
  bool direct_fallback_observed = false;
  ProviderAttestation provider_attestation = ProviderAttestation::kNone;
  std::uint32_t successful_probe_count = 0;
  std::string public_ip;
};

enum class EgressVerificationCode {
  kAccepted,
  kCollectionFailed,
  kInvalidRoute,
  kModeMismatch,
  kInsufficientProbes,
  kInvalidPublicIp,
  kProxyMismatch,
  kCandidateRouteBypassed,
  kRemoteDnsUnproven,
  kLocalDnsLeak,
  kDirectFallback,
  kProviderMismatch,
};

struct EgressVerificationResult {
  EgressVerificationCode code = EgressVerificationCode::kInvalidRoute;

  bool accepted() const { return code == EgressVerificationCode::kAccepted; }
};

bool IsPublicInternetAddress(std::string_view address);
std::string_view EgressVerificationCodeName(EgressVerificationCode code);
EgressVerificationResult ValidateEgressEvidence(
    const EgressRoute& candidate,
    const EgressVerificationEvidence& evidence);

}  // namespace fireball::egress

#endif  // FIREBALL_COMPONENTS_EGRESS_EGRESS_VERIFICATION_H_
