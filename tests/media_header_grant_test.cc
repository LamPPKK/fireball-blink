#include "fireball/components/transfer/media_header_grant.h"
#include "fireball/components/transfer/transfer_types.h"

#include <cassert>
#include <cstdio>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

using fireball::transfer::TransferRequestHeader;
using fireball::transfer::TransferRequestHeaderKind;

std::vector<TransferRequestHeader> ValidHeaders() {
  return {
      {TransferRequestHeaderKind::kAuthorization, "Bearer short-lived-token"},
      {TransferRequestHeaderKind::kCookie, "session=private; mode=video"},
      {TransferRequestHeaderKind::kOrigin, "https://media.example.test"},
      {TransferRequestHeaderKind::kReferer,
       "https://media.example.test/watch?id=7"},
      {TransferRequestHeaderKind::kUserAgent, "Fireball/0.1"},
  };
}

std::string GrantId(unsigned int index) {
  char value[37] = {};
  std::snprintf(value, sizeof(value), "91000000-0000-4000-8000-%012x", index);
  return value;
}

}  // namespace

int main() {
  using fireball::transfer::IsValidTransferRequest;
  using fireball::transfer::IsValidTransferRequestHeaders;
  using fireball::transfer::MakeUriTransferRequest;
  using fireball::transfer::MediaHeaderGrantStore;
  using fireball::transfer::TransferPersistence;

  fireball::transfer::SensitiveHeaderValue move_source("move-only-secret");
  fireball::transfer::SensitiveHeaderValue move_target(std::move(move_source));
  assert(move_source.view().empty());
  assert(move_target.view() == "move-only-secret");

  auto headers = ValidHeaders();
  assert(IsValidTransferRequestHeaders(headers));
  auto duplicated = ValidHeaders();
  duplicated.insert(duplicated.begin() + 1,
                    {TransferRequestHeaderKind::kAuthorization, "Basic abc"});
  assert(!IsValidTransferRequestHeaders(duplicated));
  auto unsorted = ValidHeaders();
  std::swap(unsorted[0], unsorted[1]);
  assert(!IsValidTransferRequestHeaders(unsorted));
  assert(!IsValidTransferRequestHeaders(std::vector<TransferRequestHeader>{
      {TransferRequestHeaderKind::kCookie, "session=bad\r\nX-Evil: yes"}}));
  assert(!IsValidTransferRequestHeaders(std::vector<TransferRequestHeader>{
      {TransferRequestHeaderKind::kOrigin,
       "https://media.example.test/not-an-origin"}}));
  assert(!IsValidTransferRequestHeaders(std::vector<TransferRequestHeader>{
      {TransferRequestHeaderKind::kReferer, "file:///private"}}));
  assert(!IsValidTransferRequestHeaders(std::vector<TransferRequestHeader>{
      {TransferRequestHeaderKind::kCookie, std::string(9000, 'x')}}));

  auto authenticated = MakeUriTransferRequest(
      "https://media.example.test/protected.mp4",
      TransferPersistence::kPersistent, "protected.mp4", ValidHeaders());
  assert(authenticated.has_value() && IsValidTransferRequest(*authenticated));
  auto magnet_with_headers = MakeUriTransferRequest(
      "magnet:?xt=urn:btih:0123456789abcdef0123456789abcdef01234567",
      TransferPersistence::kPersistent, std::nullopt, ValidHeaders());
  assert(!magnet_with_headers.has_value());

  constexpr char kProfile[] = "90000000-0000-4000-8000-000000000001";
  constexpr char kOtherProfile[] = "90000000-0000-4000-8000-000000000002";
  constexpr char kTab[] = "90000000-0000-4000-8000-000000000003";
  constexpr char kOtherTab[] = "90000000-0000-4000-8000-000000000004";
  constexpr char kCandidate[] = "90000000-0000-4000-8000-000000000005";
  constexpr char kOtherCandidate[] =
      "90000000-0000-4000-8000-000000000006";

  MediaHeaderGrantStore grants;
  assert(grants.Mint(GrantId(1), kProfile, kTab, kCandidate, ValidHeaders(),
                     1'000, 61'000));
  assert(grants.size() == 1);
  assert(!grants.Consume(GrantId(1), kOtherProfile, kTab, kCandidate, 2'000)
              .has_value());
  assert(!grants.Consume(GrantId(1), kProfile, kOtherTab, kCandidate, 2'000)
              .has_value());
  assert(!grants.Consume(GrantId(1), kProfile, kTab, kOtherCandidate, 2'000)
              .has_value());
  auto consumed = grants.Consume(GrantId(1), kProfile, kTab, kCandidate, 2'000);
  assert(consumed.has_value() && consumed->size() == ValidHeaders().size());
  assert(grants.size() == 0);
  assert(!grants.Consume(GrantId(1), kProfile, kTab, kCandidate, 2'001)
              .has_value());

  assert(grants.Mint(GrantId(2), kProfile, kTab, kCandidate, ValidHeaders(),
                     5'000, 5'001));
  assert(!grants.Consume(GrantId(2), kProfile, kTab, kCandidate, 5'001)
              .has_value());
  assert(grants.size() == 0);
  assert(!grants.Mint(GrantId(3), kProfile, kTab, kCandidate, ValidHeaders(),
                      10'000, 70'001));
  assert(!grants.Mint("not-a-uuid", kProfile, kTab, kCandidate,
                      ValidHeaders(), 10'000, 11'000));

  assert(grants.Mint(GrantId(4), kProfile, kTab, kCandidate, ValidHeaders(),
                     10'000, 20'000));
  assert(grants.Mint(GrantId(5), kOtherProfile, kOtherTab, kOtherCandidate,
                     ValidHeaders(), 10'000, 20'000));
  assert(grants.RevokeTab(kTab) == 1);
  assert(grants.RevokeProfile(kOtherProfile) == 1);
  assert(grants.size() == 0);

  for (std::size_t index = 0;
       index < fireball::transfer::kMaximumMediaHeaderGrants; ++index) {
    assert(grants.Mint(GrantId(static_cast<unsigned int>(index + 10)),
                       kProfile, kTab, kCandidate, ValidHeaders(), 30'000,
                       40'000));
  }
  assert(!grants.Mint(GrantId(999), kProfile, kTab, kCandidate,
                      ValidHeaders(), 30'000, 40'000));
  assert(grants.ExpireAt(40'000) ==
         fireball::transfer::kMaximumMediaHeaderGrants);
  assert(grants.size() == 0);
  return 0;
}
