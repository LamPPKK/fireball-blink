#ifndef FIREBALL_COMPONENTS_TRANSFER_MEDIA_DISCOVERY_H_
#define FIREBALL_COMPONENTS_TRANSFER_MEDIA_DISCOVERY_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "fireball/components/transfer/transfer_types.h"

namespace fireball::transfer {

inline constexpr std::size_t kMaximumMediaCandidatesPerTab = 32;
inline constexpr std::size_t kMaximumMediaCandidates = 256;

struct DiscoveredMedia {
  std::string id;
  std::string tab_id;
  MediaCandidateKind kind;
  std::string display_name;
  std::uint64_t content_length = 0;
  std::uint64_t observed_at_ms = 0;
  bool directly_downloadable = false;
};

// Stores network-observed media candidates in memory only. Public snapshots do
// not contain source URLs. Direct media can be consumed once into a
// TransferRequest; HLS/DASH remain visible but gated until an assembler exists.
class MediaDiscovery final {
 public:
  bool Observe(std::string id,
               std::string tab_id,
               std::string uri,
               std::string_view mime_type,
               std::optional<std::string> display_name,
               std::uint64_t content_length,
               std::uint64_t observed_at_ms);

  std::vector<DiscoveredMedia> SnapshotForTab(std::string_view tab_id) const;
  std::optional<TransferRequest> ConsumeDirect(
      std::string_view id,
      TransferPersistence persistence);
  std::size_t ForgetTab(std::string_view tab_id);
  std::size_t ExpireBefore(std::uint64_t cutoff_ms);
  std::size_t size() const { return records_.size(); }

 private:
  struct Record {
    DiscoveredMedia candidate;
    std::string source_uri;
  };

  void EnforceLimits(std::string_view tab_id);
  void EraseOldest(std::optional<std::string_view> tab_id);

  std::map<std::string, Record, std::less<>> records_;
};

}  // namespace fireball::transfer

#endif  // FIREBALL_COMPONENTS_TRANSFER_MEDIA_DISCOVERY_H_
