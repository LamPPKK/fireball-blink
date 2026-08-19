#include "fireball/components/egress/egress_verification.h"

#include <arpa/inet.h>
#include <netinet/in.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

namespace fireball::egress {
namespace {

bool IsPublicIpv4(const std::array<std::uint8_t, 4>& address) {
  const std::uint8_t first = address[0];
  const std::uint8_t second = address[1];
  const std::uint8_t third = address[2];
  if (first == 0 || first == 10 || first == 127 || first >= 224) {
    return false;
  }
  if (first == 100 && second >= 64 && second <= 127) {
    return false;
  }
  if (first == 169 && second == 254) {
    return false;
  }
  if (first == 172 && second >= 16 && second <= 31) {
    return false;
  }
  if (first == 192 &&
      ((second == 0 && (third == 0 || third == 2)) || second == 168 ||
       (second == 88 && third == 99))) {
    return false;
  }
  if (first == 198 &&
      (second == 18 || second == 19 || (second == 51 && third == 100))) {
    return false;
  }
  if (first == 203 && second == 0 && third == 113) {
    return false;
  }
  return true;
}

bool IsPublicIpv6(const std::array<std::uint8_t, 16>& address) {
  constexpr std::array<std::uint8_t, 12> kIpv4MappedPrefix = {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};
  if (std::equal(kIpv4MappedPrefix.begin(), kIpv4MappedPrefix.end(),
                 address.begin())) {
    return IsPublicIpv4(
        {address[12], address[13], address[14], address[15]});
  }
  // Globally routable unicast currently occupies 2000::/3. Reject the
  // documentation prefix even though it sits inside that range.
  if ((address[0] & 0xe0) != 0x20) {
    return false;
  }
  return !(address[0] == 0x20 && address[1] == 0x01 && address[2] == 0x0d &&
           address[3] == 0xb8);
}

ProviderAttestation ExpectedAttestation(EgressMode mode) {
  switch (mode) {
    case EgressMode::kWarp:
      return ProviderAttestation::kWarp;
    case EgressMode::kTor:
      return ProviderAttestation::kTor;
    case EgressMode::kDirect:
      return ProviderAttestation::kNone;
  }
  return ProviderAttestation::kNone;
}

}  // namespace

bool IsPublicInternetAddress(std::string_view address) {
  if (address.empty() || address.size() > INET6_ADDRSTRLEN) {
    return false;
  }
  std::array<char, INET6_ADDRSTRLEN + 1> input{};
  std::memcpy(input.data(), address.data(), address.size());

  std::array<std::uint8_t, 4> ipv4{};
  if (inet_pton(AF_INET, input.data(), ipv4.data()) == 1) {
    return IsPublicIpv4(ipv4);
  }
  std::array<std::uint8_t, 16> ipv6{};
  return inet_pton(AF_INET6, input.data(), ipv6.data()) == 1 &&
         IsPublicIpv6(ipv6);
}

std::string_view EgressVerificationCodeName(EgressVerificationCode code) {
  switch (code) {
    case EgressVerificationCode::kAccepted:
      return "egress.verification.accepted";
    case EgressVerificationCode::kCollectionFailed:
      return "egress.verification.collection_failed";
    case EgressVerificationCode::kInvalidRoute:
      return "egress.verification.invalid_route";
    case EgressVerificationCode::kModeMismatch:
      return "egress.verification.mode_mismatch";
    case EgressVerificationCode::kInsufficientProbes:
      return "egress.verification.insufficient_probes";
    case EgressVerificationCode::kInvalidPublicIp:
      return "egress.verification.invalid_public_ip";
    case EgressVerificationCode::kProxyMismatch:
      return "egress.verification.proxy_mismatch";
    case EgressVerificationCode::kCandidateRouteBypassed:
      return "egress.verification.candidate_route_bypassed";
    case EgressVerificationCode::kRemoteDnsUnproven:
      return "egress.verification.remote_dns_unproven";
    case EgressVerificationCode::kLocalDnsLeak:
      return "egress.verification.local_dns_leak";
    case EgressVerificationCode::kDirectFallback:
      return "egress.verification.direct_fallback";
    case EgressVerificationCode::kProviderMismatch:
      return "egress.verification.provider_mismatch";
  }
  return "egress.verification.unknown";
}

EgressVerificationResult ValidateEgressEvidence(
    const EgressRoute& candidate,
    const EgressVerificationEvidence& evidence) {
  if (!IsValidRoute(candidate)) {
    return {EgressVerificationCode::kInvalidRoute};
  }
  if (evidence.observed_mode != candidate.mode) {
    return {EgressVerificationCode::kModeMismatch};
  }
  if (!IsPublicInternetAddress(evidence.public_ip)) {
    return {EgressVerificationCode::kInvalidPublicIp};
  }
  if (evidence.direct_fallback_observed) {
    return {EgressVerificationCode::kDirectFallback};
  }
  if (evidence.provider_attestation != ExpectedAttestation(candidate.mode)) {
    return {EgressVerificationCode::kProviderMismatch};
  }
  if (candidate.mode == EgressMode::kDirect) {
    if (evidence.successful_probe_count < 1) {
      return {EgressVerificationCode::kInsufficientProbes};
    }
    if (evidence.observed_proxy_port != 0) {
      return {EgressVerificationCode::kProxyMismatch};
    }
    if (!evidence.public_ip_probe_used_candidate_route) {
      return {EgressVerificationCode::kCandidateRouteBypassed};
    }
    return {EgressVerificationCode::kAccepted};
  }

  if (evidence.successful_probe_count < 2) {
    return {EgressVerificationCode::kInsufficientProbes};
  }
  if (!candidate.proxy.has_value() ||
      evidence.observed_proxy_port != candidate.proxy->browser_socks5) {
    return {EgressVerificationCode::kProxyMismatch};
  }
  if (!evidence.public_ip_probe_used_candidate_route ||
      !evidence.dns_probe_used_candidate_route) {
    return {EgressVerificationCode::kCandidateRouteBypassed};
  }
  if (!evidence.remote_dns_confirmed) {
    return {EgressVerificationCode::kRemoteDnsUnproven};
  }
  if (evidence.local_dns_observed) {
    return {EgressVerificationCode::kLocalDnsLeak};
  }
  return {EgressVerificationCode::kAccepted};
}

}  // namespace fireball::egress
