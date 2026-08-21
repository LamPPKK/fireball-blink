#include "fireball/chromium/cosmetic_dom_snapshot.h"

#include <cassert>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

int main() {
  using fireball::chromium::CosmeticDomSnapshotBuilder;
  using fireball::chromium::DecodeCosmeticDomSnapshot;
  using fireball::chromium::EncodeCosmeticDomSnapshot;
  using fireball::chromium::kMaximumCosmeticDomAttributeBytes;
  using fireball::chromium::kMaximumCosmeticDomElements;
  using fireball::chromium::kMaximumCosmeticDomEntries;
  using fireball::chromium::kMaximumCosmeticDomTokenBytes;

  CosmeticDomSnapshotBuilder builder;
  assert(builder.AddElement("hero", "sponsor advert sponsor"));
  assert(builder.AddElement("sidebar", "advert\talpha\nbeta"));
  assert(builder.AddElement("", ""));
  auto snapshot = std::move(builder).Finish(7);
  assert(snapshot.has_value());
  assert(snapshot->revision == 7);
  assert((snapshot->classes ==
          std::vector<std::string>{"advert", "alpha", "beta", "sponsor"}));
  assert((snapshot->ids == std::vector<std::string>{"hero", "sidebar"}));

  auto wire = EncodeCosmeticDomSnapshot(*snapshot);
  assert(wire.has_value());
  assert(wire->payload.size() ==
         fireball::chromium::kMaximumCosmeticDomWireBytes);
  auto decoded = DecodeCosmeticDomSnapshot(
      snapshot->revision, wire->payload_size, wire->class_count, wire->id_count,
      wire->payload);
  assert(decoded.has_value());
  assert(decoded->revision == snapshot->revision);
  assert(decoded->classes == snapshot->classes);
  assert(decoded->ids == snapshot->ids);

  auto malformed_wire = *wire;
  malformed_wire.payload[malformed_wire.payload_size] = 1;
  assert(!DecodeCosmeticDomSnapshot(
      snapshot->revision, malformed_wire.payload_size,
      malformed_wire.class_count, malformed_wire.id_count,
      malformed_wire.payload));

  auto unsorted = *snapshot;
  unsorted.classes = {"zeta", "alpha"};
  assert(!EncodeCosmeticDomSnapshot(unsorted).has_value());
  auto duplicate = *snapshot;
  duplicate.ids = {"hero", "hero"};
  assert(!EncodeCosmeticDomSnapshot(duplicate).has_value());

  malformed_wire = *wire;
  malformed_wire.payload[0] = 0;
  malformed_wire.payload[1] = 0;
  assert(!DecodeCosmeticDomSnapshot(
      snapshot->revision, malformed_wire.payload_size,
      malformed_wire.class_count, malformed_wire.id_count,
      malformed_wire.payload));

  CosmeticDomSnapshotBuilder skipped;
  assert(skipped.AddElement(std::string("bad") + static_cast<char>(0x01) +
                                "id",
                            std::string(kMaximumCosmeticDomAttributeBytes + 1,
                                        'x')));
  assert(skipped.AddElement(
      std::string(kMaximumCosmeticDomTokenBytes + 1, 'y'),
      std::string("bad") + static_cast<char>(0x7f) + "class"));
  assert(skipped.AddElement(std::string("bad\xff", 4), "valid"));
  auto skipped_snapshot = std::move(skipped).Finish(1);
  assert(skipped_snapshot.has_value());
  assert((skipped_snapshot->classes == std::vector<std::string>{"valid"}));
  assert(skipped_snapshot->ids.empty());

  CosmeticDomSnapshotBuilder entry_limit;
  for (std::size_t index = 0; index < kMaximumCosmeticDomEntries; ++index) {
    assert(entry_limit.AddElement("id-" + std::to_string(index), ""));
  }
  assert(!entry_limit.AddElement("one-too-many", ""));
  assert(entry_limit.limit_exceeded());
  assert(!std::move(entry_limit).Finish(2).has_value());

  CosmeticDomSnapshotBuilder element_limit;
  for (std::size_t index = 0; index < kMaximumCosmeticDomElements; ++index) {
    assert(element_limit.AddElement("same", "same"));
  }
  assert(!element_limit.AddElement("same", "same"));
  assert(element_limit.limit_exceeded());

  CosmeticDomSnapshotBuilder invalid_revision;
  assert(invalid_revision.AddElement("hero", "advert"));
  assert(!std::move(invalid_revision).Finish(0).has_value());
  return 0;
}
