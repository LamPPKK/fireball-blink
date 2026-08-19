#include "fireball/components/egress/egress_route.h"
#include "fireball/components/egress/egress_verification.h"

#include <cassert>
#include <string_view>

namespace {

using fireball::egress::EgressMode;
using fireball::egress::EgressRoute;
using fireball::egress::EgressVerificationCode;
using fireball::egress::EgressVerificationEvidence;
using fireball::egress::ProviderAttestation;

EgressVerificationEvidence EvidenceFor(const EgressRoute& route) {
  EgressVerificationEvidence evidence;
  evidence.observed_mode = route.mode;
  evidence.public_ip_probe_used_candidate_route = true;
  evidence.public_ip = "1.1.1.1";
  evidence.successful_probe_count = 1;
  if (route.proxy.has_value()) {
    evidence.observed_proxy_port = route.proxy->browser_socks5;
    evidence.dns_probe_used_candidate_route = true;
    evidence.remote_dns_confirmed = true;
    evidence.provider_attestation =
        route.mode == EgressMode::kWarp ? ProviderAttestation::kWarp
                                       : ProviderAttestation::kTor;
    evidence.successful_probe_count = 2;
  }
  return evidence;
}

void ExpectCode(const EgressRoute& route,
                const EgressVerificationEvidence& evidence,
                EgressVerificationCode expected) {
  const auto result = fireball::egress::ValidateEgressEvidence(route, evidence);
  assert(result.code == expected);
  assert(result.accepted() ==
         (expected == EgressVerificationCode::kAccepted));
  assert(!fireball::egress::EgressVerificationCodeName(result.code).empty());
}

}  // namespace

int main() {
  using fireball::egress::IsPublicInternetAddress;
  using fireball::egress::MakeDirectRoute;
  using fireball::egress::MakeTorRoute;
  using fireball::egress::MakeWarpRoute;

  assert(IsPublicInternetAddress("1.1.1.1"));
  assert(IsPublicInternetAddress("2606:4700:4700::1111"));
  assert(IsPublicInternetAddress("::ffff:8.8.8.8"));
  for (std::string_view address : {
           "",
           "not-an-ip",
           "0.0.0.0",
           "10.0.0.1",
           "100.64.0.1",
           "127.0.0.1",
           "169.254.1.1",
           "172.16.0.1",
           "192.168.1.1",
           "192.0.2.1",
           "198.18.0.1",
           "198.51.100.1",
           "203.0.113.1",
           "224.0.0.1",
           "::",
           "::1",
           "fc00::1",
           "fe80::1",
           "ff02::1",
           "2001:db8::1",
           "::ffff:192.168.1.1",
       }) {
    assert(!IsPublicInternetAddress(address));
  }

  const EgressRoute direct = MakeDirectRoute();
  const EgressRoute warp = *MakeWarpRoute(40000);
  const EgressRoute tor = *MakeTorRoute(39050, 39051);
  EgressRoute invalid = warp;
  invalid.proxy.reset();
  ExpectCode(invalid, EvidenceFor(warp),
             EgressVerificationCode::kInvalidRoute);
  ExpectCode(direct, EvidenceFor(direct), EgressVerificationCode::kAccepted);
  ExpectCode(warp, EvidenceFor(warp), EgressVerificationCode::kAccepted);
  auto valid_tor = EvidenceFor(tor);
  valid_tor.public_ip = "2606:4700:4700::1111";
  ExpectCode(tor, valid_tor, EgressVerificationCode::kAccepted);

  auto evidence = EvidenceFor(warp);
  evidence.observed_mode = EgressMode::kTor;
  ExpectCode(warp, evidence, EgressVerificationCode::kModeMismatch);
  evidence = EvidenceFor(warp);
  evidence.successful_probe_count = 1;
  ExpectCode(warp, evidence, EgressVerificationCode::kInsufficientProbes);
  evidence = EvidenceFor(warp);
  evidence.public_ip = "192.168.1.1";
  ExpectCode(warp, evidence, EgressVerificationCode::kInvalidPublicIp);
  evidence = EvidenceFor(warp);
  evidence.observed_proxy_port = 40001;
  ExpectCode(warp, evidence, EgressVerificationCode::kProxyMismatch);
  evidence = EvidenceFor(warp);
  evidence.public_ip_probe_used_candidate_route = false;
  ExpectCode(warp, evidence,
             EgressVerificationCode::kCandidateRouteBypassed);
  evidence = EvidenceFor(warp);
  evidence.dns_probe_used_candidate_route = false;
  ExpectCode(warp, evidence,
             EgressVerificationCode::kCandidateRouteBypassed);
  evidence = EvidenceFor(warp);
  evidence.remote_dns_confirmed = false;
  ExpectCode(warp, evidence, EgressVerificationCode::kRemoteDnsUnproven);
  evidence = EvidenceFor(warp);
  evidence.local_dns_observed = true;
  ExpectCode(warp, evidence, EgressVerificationCode::kLocalDnsLeak);
  evidence = EvidenceFor(warp);
  evidence.direct_fallback_observed = true;
  ExpectCode(warp, evidence, EgressVerificationCode::kDirectFallback);
  evidence = EvidenceFor(warp);
  evidence.provider_attestation = ProviderAttestation::kTor;
  ExpectCode(warp, evidence, EgressVerificationCode::kProviderMismatch);

  evidence = EvidenceFor(direct);
  evidence.observed_proxy_port = 40000;
  ExpectCode(direct, evidence, EgressVerificationCode::kProxyMismatch);
  evidence = EvidenceFor(direct);
  evidence.provider_attestation = ProviderAttestation::kWarp;
  ExpectCode(direct, evidence, EgressVerificationCode::kProviderMismatch);
  return 0;
}
