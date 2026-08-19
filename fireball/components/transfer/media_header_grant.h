#ifndef FIREBALL_COMPONENTS_TRANSFER_MEDIA_HEADER_GRANT_H_
#define FIREBALL_COMPONENTS_TRANSFER_MEDIA_HEADER_GRANT_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "fireball/components/transfer/transfer_types.h"

namespace fireball::transfer {

inline constexpr std::uint64_t kMaximumMediaHeaderGrantLifetimeMs = 60'000;
inline constexpr std::size_t kMaximumMediaHeaderGrants = 128;

// In-memory, one-time capability store. Grant IDs, Profile/Tab/candidate IDs
// are stable UUID strings. A successful consume removes the record; expiry,
// Profile deletion and Tab closure destroy sensitive values in place.
class MediaHeaderGrantStore final {
 public:
  MediaHeaderGrantStore() = default;
  ~MediaHeaderGrantStore();

  MediaHeaderGrantStore(const MediaHeaderGrantStore&) = delete;
  MediaHeaderGrantStore& operator=(const MediaHeaderGrantStore&) = delete;

  bool Mint(std::string grant_id,
            std::string profile_id,
            std::string tab_id,
            std::string candidate_id,
            std::vector<TransferRequestHeader> headers,
            std::uint64_t now_ms,
            std::uint64_t expires_at_ms);
  std::optional<std::vector<TransferRequestHeader>> Consume(
      std::string_view grant_id,
      std::string_view profile_id,
      std::string_view tab_id,
      std::string_view candidate_id,
      std::uint64_t now_ms);

  std::size_t RevokeProfile(std::string_view profile_id);
  std::size_t RevokeTab(std::string_view tab_id);
  std::size_t ExpireAt(std::uint64_t now_ms);
  std::size_t size() const { return records_.size(); }

 private:
  struct Record {
    std::string profile_id;
    std::string tab_id;
    std::string candidate_id;
    std::vector<TransferRequestHeader> headers;
    std::uint64_t expires_at_ms = 0;
  };

  std::map<std::string, Record, std::less<>> records_;
};

}  // namespace fireball::transfer

#endif  // FIREBALL_COMPONENTS_TRANSFER_MEDIA_HEADER_GRANT_H_
