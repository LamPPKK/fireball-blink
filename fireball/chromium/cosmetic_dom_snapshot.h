#ifndef FIREBALL_CHROMIUM_COSMETIC_DOM_SNAPSHOT_H_
#define FIREBALL_CHROMIUM_COSMETIC_DOM_SNAPSHOT_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fireball::chromium {

inline constexpr std::size_t kMaximumCosmeticDomEntries = 4096;
inline constexpr std::size_t kMaximumCosmeticDomTokenBytes = 256;
inline constexpr std::size_t kMaximumCosmeticDomSnapshotBytes = 256 * 1024;
inline constexpr std::size_t kMaximumCosmeticDomElements = 50'000;
inline constexpr std::size_t kMaximumCosmeticDomAttributeBytes = 4096;
inline constexpr std::size_t kMaximumCosmeticDomAttributeCodeUnits = 4096;
inline constexpr std::size_t kMaximumCosmeticDomWireBytes =
    kMaximumCosmeticDomSnapshotBytes +
    (kMaximumCosmeticDomEntries * sizeof(std::uint16_t));

// Internal renderer-to-browser DTO. It is consumed by the policy bridge and
// must never be exposed through diagnostics or result callbacks.
struct CosmeticDomSnapshot {
  std::uint64_t revision = 0;
  std::vector<std::string> classes;
  std::vector<std::string> ids;
};

// Canonical fixed-capacity Mojo representation. Each token is prefixed by one
// little-endian uint16 length; the unused tail is zeroed. The fixed-size Mojom
// array bounds browser allocation before the renderer response is dispatched.
struct CosmeticDomWireSnapshot {
  std::uint32_t payload_size = 0;
  std::uint32_t class_count = 0;
  std::uint32_t id_count = 0;
  std::vector<std::uint8_t> payload;
};

bool IsBoundedCosmeticDomSnapshot(const std::vector<std::string>& classes,
                                  const std::vector<std::string>& ids);
std::optional<CosmeticDomWireSnapshot> EncodeCosmeticDomSnapshot(
    const CosmeticDomSnapshot& snapshot);
std::optional<CosmeticDomSnapshot> DecodeCosmeticDomSnapshot(
    std::uint64_t revision,
    std::uint32_t payload_size,
    std::uint32_t class_count,
    std::uint32_t id_count,
    std::span<const std::uint8_t> payload);
bool IsZeroedCosmeticDomWirePayload(std::span<const std::uint8_t> payload);

// Chromium-independent builder for the renderer's light-DOM scan. Invalid or
// oversized individual values are skipped; exhausting a global quota rejects
// the whole snapshot so a partial page view is never mistaken for complete.
class CosmeticDomSnapshotBuilder final {
 public:
  bool AddElement(std::string_view id, std::string_view class_attribute);
  std::optional<CosmeticDomSnapshot> Finish(std::uint64_t revision) &&;

  bool limit_exceeded() const { return limit_exceeded_; }
  std::size_t scanned_element_count() const { return scanned_element_count_; }

 private:
  using TokenSet = std::set<std::string, std::less<>>;

  bool AddToken(std::string_view value, TokenSet* destination);

  TokenSet classes_;
  TokenSet ids_;
  std::size_t scanned_element_count_ = 0;
  std::size_t token_byte_count_ = 0;
  bool limit_exceeded_ = false;
};

}  // namespace fireball::chromium

#endif  // FIREBALL_CHROMIUM_COSMETIC_DOM_SNAPSHOT_H_
