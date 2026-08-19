#include "fireball/components/transfer/media_discovery.h"

#include <algorithm>
#include <utility>

namespace fireball::transfer {
namespace {

std::string DefaultDisplayName(MediaCandidateKind kind) {
  switch (kind) {
    case MediaCandidateKind::kDirectAudio:
      return "Fireball audio";
    case MediaCandidateKind::kDirectVideo:
      return "Fireball video";
    case MediaCandidateKind::kHlsManifest:
      return "HLS stream";
    case MediaCandidateKind::kDashManifest:
      return "DASH stream";
    case MediaCandidateKind::kNone:
      return "Media";
  }
  return "Media";
}

bool IsDirect(MediaCandidateKind kind) {
  return kind == MediaCandidateKind::kDirectAudio ||
         kind == MediaCandidateKind::kDirectVideo;
}

}  // namespace

bool MediaDiscovery::Observe(std::string id,
                             std::string tab_id,
                             std::string uri,
                             std::string_view mime_type,
                             std::optional<std::string> display_name,
                             std::uint64_t content_length,
                             std::uint64_t observed_at_ms) {
  const MediaCandidateKind kind = ClassifyMediaCandidate(uri, mime_type);
  if (!IsCanonicalTransferId(id) || !IsCanonicalTransferId(tab_id) ||
      kind == MediaCandidateKind::kNone || observed_at_ms == 0 ||
      (display_name.has_value() && !IsSafeOutputName(*display_name))) {
    return false;
  }

  const auto update = [&](Record* record) {
    if (observed_at_ms >= record->candidate.observed_at_ms) {
      record->candidate.content_length = content_length;
      record->candidate.observed_at_ms = observed_at_ms;
      if (display_name.has_value()) {
        record->candidate.display_name = std::move(*display_name);
      }
    }
    return true;
  };

  auto existing_id = records_.find(id);
  if (existing_id != records_.end()) {
    Record& record = existing_id->second;
    if (record.candidate.tab_id != tab_id || record.source_uri != uri ||
        record.candidate.kind != kind) {
      return false;
    }
    return update(&record);
  }

  for (auto& entry : records_) {
    Record& record = entry.second;
    if (record.candidate.tab_id == tab_id && record.source_uri == uri &&
        record.candidate.kind == kind) {
      return update(&record);
    }
  }
  DiscoveredMedia candidate{
      std::move(id),
      std::move(tab_id),
      kind,
      display_name.value_or(DefaultDisplayName(kind)),
      content_length,
      observed_at_ms,
      IsDirect(kind),
  };
  const std::string key = candidate.id;
  const std::string owner = candidate.tab_id;
  records_.emplace(key, Record{std::move(candidate), std::move(uri)});
  EnforceLimits(owner);
  return true;
}

std::vector<DiscoveredMedia> MediaDiscovery::SnapshotForTab(
    std::string_view tab_id) const {
  std::vector<DiscoveredMedia> result;
  for (const auto& entry : records_) {
    const Record& record = entry.second;
    if (record.candidate.tab_id == tab_id) {
      result.push_back(record.candidate);
    }
  }
  std::sort(result.begin(), result.end(),
            [](const DiscoveredMedia& left, const DiscoveredMedia& right) {
              if (left.observed_at_ms != right.observed_at_ms) {
                return left.observed_at_ms > right.observed_at_ms;
              }
              return left.id < right.id;
            });
  return result;
}

std::optional<TransferRequest> MediaDiscovery::ConsumeDirect(
    std::string_view id,
    TransferPersistence persistence) {
  auto record = records_.find(id);
  if (record == records_.end() || !record->second.candidate.directly_downloadable) {
    return std::nullopt;
  }
  auto request = MakeUriTransferRequest(
      record->second.source_uri, persistence,
      std::optional<std::string>(record->second.candidate.display_name));
  if (!request.has_value()) {
    return std::nullopt;
  }
  records_.erase(record);
  return request;
}

std::size_t MediaDiscovery::ForgetTab(std::string_view tab_id) {
  return std::erase_if(records_, [tab_id](const auto& entry) {
    return entry.second.candidate.tab_id == tab_id;
  });
}

std::size_t MediaDiscovery::ExpireBefore(std::uint64_t cutoff_ms) {
  return std::erase_if(records_, [cutoff_ms](const auto& entry) {
    return entry.second.candidate.observed_at_ms < cutoff_ms;
  });
}

void MediaDiscovery::EnforceLimits(std::string_view tab_id) {
  while (std::count_if(records_.begin(), records_.end(),
                       [tab_id](const auto& entry) {
                         return entry.second.candidate.tab_id == tab_id;
                       }) > static_cast<std::ptrdiff_t>(
                                kMaximumMediaCandidatesPerTab)) {
    EraseOldest(tab_id);
  }
  while (records_.size() > kMaximumMediaCandidates) {
    EraseOldest(std::nullopt);
  }
}

void MediaDiscovery::EraseOldest(std::optional<std::string_view> tab_id) {
  auto oldest = records_.end();
  for (auto record = records_.begin(); record != records_.end(); ++record) {
    if (tab_id.has_value() && record->second.candidate.tab_id != *tab_id) {
      continue;
    }
    if (oldest == records_.end() ||
        record->second.candidate.observed_at_ms <
            oldest->second.candidate.observed_at_ms ||
        (record->second.candidate.observed_at_ms ==
             oldest->second.candidate.observed_at_ms &&
         record->first < oldest->first)) {
      oldest = record;
    }
  }
  if (oldest != records_.end()) {
    records_.erase(oldest);
  }
}

}  // namespace fireball::transfer
