#include "fireball/components/transfer/media_header_grant.h"

#include <utility>

namespace fireball::transfer {

MediaHeaderGrantStore::~MediaHeaderGrantStore() = default;

bool MediaHeaderGrantStore::Mint(
    std::string grant_id,
    std::string profile_id,
    std::string tab_id,
    std::string candidate_id,
    std::vector<TransferRequestHeader> headers,
    std::uint64_t now_ms,
    std::uint64_t expires_at_ms) {
  if (!IsCanonicalTransferId(grant_id) ||
      !IsCanonicalTransferId(profile_id) || !IsCanonicalTransferId(tab_id) ||
      !IsCanonicalTransferId(candidate_id) || headers.empty() ||
      !IsValidTransferRequestHeaders(headers) || now_ms == 0 ||
      expires_at_ms <= now_ms ||
      expires_at_ms - now_ms > kMaximumMediaHeaderGrantLifetimeMs ||
      records_.size() >= kMaximumMediaHeaderGrants ||
      records_.contains(grant_id)) {
    return false;
  }
  return records_
      .emplace(std::move(grant_id),
               Record{std::move(profile_id), std::move(tab_id),
                      std::move(candidate_id), std::move(headers),
                      expires_at_ms})
      .second;
}

std::optional<std::vector<TransferRequestHeader>>
MediaHeaderGrantStore::Consume(std::string_view grant_id,
                               std::string_view profile_id,
                               std::string_view tab_id,
                               std::string_view candidate_id,
                               std::uint64_t now_ms) {
  auto record = records_.find(grant_id);
  if (record == records_.end()) {
    return std::nullopt;
  }
  if (now_ms == 0 || record->second.expires_at_ms <= now_ms) {
    records_.erase(record);
    return std::nullopt;
  }
  if (record->second.profile_id != profile_id ||
      record->second.tab_id != tab_id ||
      record->second.candidate_id != candidate_id) {
    return std::nullopt;
  }
  std::vector<TransferRequestHeader> headers =
      std::move(record->second.headers);
  records_.erase(record);
  return headers;
}

std::size_t MediaHeaderGrantStore::RevokeProfile(
    std::string_view profile_id) {
  return std::erase_if(records_, [profile_id](const auto& entry) {
    return entry.second.profile_id == profile_id;
  });
}

std::size_t MediaHeaderGrantStore::RevokeTab(std::string_view tab_id) {
  return std::erase_if(records_, [tab_id](const auto& entry) {
    return entry.second.tab_id == tab_id;
  });
}

std::size_t MediaHeaderGrantStore::ExpireAt(std::uint64_t now_ms) {
  return std::erase_if(records_, [now_ms](const auto& entry) {
    return entry.second.expires_at_ms <= now_ms;
  });
}

}  // namespace fireball::transfer
