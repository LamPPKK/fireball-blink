#include "fireball/chromium/cosmetic_dom_snapshot.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <limits>
#include <string>
#include <utility>

namespace fireball::chromium {
namespace {

bool IsUtf8Continuation(unsigned char value) {
  return (value & 0xc0U) == 0x80U;
}

bool IsValidUtf8(std::string_view value) {
  for (std::size_t index = 0; index < value.size();) {
    const unsigned char lead = static_cast<unsigned char>(value[index]);
    if (lead <= 0x7fU) {
      ++index;
      continue;
    }
    std::size_t width = 0;
    std::uint32_t codepoint = 0;
    std::uint32_t minimum = 0;
    if ((lead & 0xe0U) == 0xc0U) {
      width = 2;
      codepoint = lead & 0x1fU;
      minimum = 0x80U;
    } else if ((lead & 0xf0U) == 0xe0U) {
      width = 3;
      codepoint = lead & 0x0fU;
      minimum = 0x800U;
    } else if ((lead & 0xf8U) == 0xf0U) {
      width = 4;
      codepoint = lead & 0x07U;
      minimum = 0x10000U;
    } else {
      return false;
    }
    if (index + width > value.size()) {
      return false;
    }
    for (std::size_t offset = 1; offset < width; ++offset) {
      const unsigned char continuation =
          static_cast<unsigned char>(value[index + offset]);
      if (!IsUtf8Continuation(continuation)) {
        return false;
      }
      codepoint = (codepoint << 6U) | (continuation & 0x3fU);
    }
    if (codepoint < minimum || codepoint > 0x10ffffU ||
        (codepoint >= 0xd800U && codepoint <= 0xdfffU)) {
      return false;
    }
    index += width;
  }
  return true;
}

bool IsSafeDomToken(std::string_view value) {
  return !value.empty() && value.size() <= kMaximumCosmeticDomTokenBytes &&
         IsValidUtf8(value) &&
         std::none_of(value.begin(), value.end(), [](char character) {
           const unsigned char byte = static_cast<unsigned char>(character);
           return byte < 0x20U || byte == 0x7fU;
         });
}

bool IsAsciiWhitespace(char character) {
  return character == ' ' || character == '\t' || character == '\n' ||
         character == '\r' || character == '\f';
}

bool AddBoundedTokens(const std::vector<std::string>& values,
                      std::size_t* entry_count,
                      std::size_t* byte_count) {
  if (entry_count == nullptr || byte_count == nullptr ||
      values.size() > kMaximumCosmeticDomEntries - *entry_count) {
    return false;
  }
  *entry_count += values.size();
  for (const std::string& value : values) {
    if (!IsSafeDomToken(value) ||
        value.size() > kMaximumCosmeticDomSnapshotBytes - *byte_count) {
      return false;
    }
    *byte_count += value.size();
  }
  return true;
}

bool AppendWireTokens(const std::vector<std::string>& values,
                      std::vector<std::uint8_t>* payload) {
  if (payload == nullptr) {
    return false;
  }
  for (const std::string& value : values) {
    if (!IsSafeDomToken(value) ||
        value.size() > std::numeric_limits<std::uint16_t>::max() ||
        payload->size() > kMaximumCosmeticDomWireBytes ||
        sizeof(std::uint16_t) + value.size() >
            kMaximumCosmeticDomWireBytes - payload->size()) {
      return false;
    }
    const auto length = static_cast<std::uint16_t>(value.size());
    payload->push_back(static_cast<std::uint8_t>(length & 0xffU));
    payload->push_back(static_cast<std::uint8_t>(length >> 8U));
    payload->insert(payload->end(), value.begin(), value.end());
  }
  return true;
}

bool IsStrictlySorted(const std::vector<std::string>& values) {
  return std::adjacent_find(
             values.begin(), values.end(),
             [](const std::string& left, const std::string& right) {
               return left >= right;
             }) == values.end();
}

bool ReadWireTokens(std::span<const std::uint8_t> payload,
                    std::size_t payload_size,
                    std::uint32_t count,
                    std::size_t* cursor,
                    std::vector<std::string>* destination) {
  if (cursor == nullptr || destination == nullptr) {
    return false;
  }
  destination->reserve(count);
  for (std::uint32_t index = 0; index < count; ++index) {
    if (*cursor > payload_size ||
        sizeof(std::uint16_t) > payload_size - *cursor) {
      return false;
    }
    const std::uint16_t length =
        static_cast<std::uint16_t>(payload[*cursor]) |
        (static_cast<std::uint16_t>(payload[*cursor + 1]) << 8U);
    *cursor += sizeof(std::uint16_t);
    if (length == 0 || length > kMaximumCosmeticDomTokenBytes ||
        *cursor > payload_size || length > payload_size - *cursor) {
      return false;
    }
    std::string token(reinterpret_cast<const char*>(payload.data() + *cursor),
                      length);
    *cursor += length;
    if (!IsSafeDomToken(token) ||
        (!destination->empty() && destination->back() >= token)) {
      return false;
    }
    destination->push_back(std::move(token));
  }
  return true;
}

}  // namespace

bool IsBoundedCosmeticDomSnapshot(const std::vector<std::string>& classes,
                                  const std::vector<std::string>& ids) {
  std::size_t entry_count = 0;
  std::size_t byte_count = 0;
  return AddBoundedTokens(classes, &entry_count, &byte_count) &&
         AddBoundedTokens(ids, &entry_count, &byte_count);
}

std::optional<CosmeticDomWireSnapshot> EncodeCosmeticDomSnapshot(
    const CosmeticDomSnapshot& snapshot) {
  if (snapshot.revision == 0 ||
      !IsBoundedCosmeticDomSnapshot(snapshot.classes, snapshot.ids) ||
      !IsStrictlySorted(snapshot.classes) ||
      !IsStrictlySorted(snapshot.ids) ||
      snapshot.classes.size() > std::numeric_limits<std::uint32_t>::max() ||
      snapshot.ids.size() > std::numeric_limits<std::uint32_t>::max()) {
    return std::nullopt;
  }
  CosmeticDomWireSnapshot wire;
  wire.class_count = static_cast<std::uint32_t>(snapshot.classes.size());
  wire.id_count = static_cast<std::uint32_t>(snapshot.ids.size());
  wire.payload.reserve(kMaximumCosmeticDomWireBytes);
  if (!AppendWireTokens(snapshot.classes, &wire.payload) ||
      !AppendWireTokens(snapshot.ids, &wire.payload)) {
    return std::nullopt;
  }
  wire.payload_size = static_cast<std::uint32_t>(wire.payload.size());
  wire.payload.resize(kMaximumCosmeticDomWireBytes, 0);
  return wire;
}

std::optional<CosmeticDomSnapshot> DecodeCosmeticDomSnapshot(
    std::uint64_t revision,
    std::uint32_t payload_size,
    std::uint32_t class_count,
    std::uint32_t id_count,
    std::span<const std::uint8_t> payload) {
  if (revision == 0 || payload.size() != kMaximumCosmeticDomWireBytes ||
      payload_size > payload.size() ||
      class_count > kMaximumCosmeticDomEntries ||
      id_count > kMaximumCosmeticDomEntries - class_count ||
      !std::all_of(payload.begin() + payload_size, payload.end(),
                   [](std::uint8_t value) { return value == 0; })) {
    return std::nullopt;
  }
  CosmeticDomSnapshot snapshot;
  snapshot.revision = revision;
  std::size_t cursor = 0;
  if (!ReadWireTokens(payload, payload_size, class_count, &cursor,
                      &snapshot.classes) ||
      !ReadWireTokens(payload, payload_size, id_count, &cursor,
                      &snapshot.ids) ||
      cursor != payload_size ||
      !IsBoundedCosmeticDomSnapshot(snapshot.classes, snapshot.ids)) {
    return std::nullopt;
  }
  return snapshot;
}

bool IsZeroedCosmeticDomWirePayload(
    std::span<const std::uint8_t> payload) {
  return payload.size() == kMaximumCosmeticDomWireBytes &&
         std::all_of(payload.begin(), payload.end(),
                     [](std::uint8_t value) { return value == 0; });
}

bool CosmeticDomSnapshotBuilder::AddElement(
    std::string_view id,
    std::string_view class_attribute) {
  if (limit_exceeded_ ||
      scanned_element_count_ == kMaximumCosmeticDomElements) {
    limit_exceeded_ = true;
    return false;
  }
  ++scanned_element_count_;
  if (!AddToken(id, &ids_)) {
    return false;
  }
  if (class_attribute.size() > kMaximumCosmeticDomAttributeBytes) {
    return true;
  }

  std::size_t cursor = 0;
  while (cursor < class_attribute.size()) {
    while (cursor < class_attribute.size() &&
           IsAsciiWhitespace(class_attribute[cursor])) {
      ++cursor;
    }
    const std::size_t start = cursor;
    while (cursor < class_attribute.size() &&
           !IsAsciiWhitespace(class_attribute[cursor])) {
      ++cursor;
    }
    if (start != cursor &&
        !AddToken(class_attribute.substr(start, cursor - start), &classes_)) {
      return false;
    }
  }
  return true;
}

std::optional<CosmeticDomSnapshot> CosmeticDomSnapshotBuilder::Finish(
    std::uint64_t revision) && {
  if (revision == 0 || limit_exceeded_) {
    return std::nullopt;
  }
  CosmeticDomSnapshot snapshot;
  snapshot.revision = revision;
  snapshot.classes.assign(std::make_move_iterator(classes_.begin()),
                          std::make_move_iterator(classes_.end()));
  snapshot.ids.assign(std::make_move_iterator(ids_.begin()),
                      std::make_move_iterator(ids_.end()));
  if (!IsBoundedCosmeticDomSnapshot(snapshot.classes, snapshot.ids)) {
    return std::nullopt;
  }
  return snapshot;
}

bool CosmeticDomSnapshotBuilder::AddToken(
    std::string_view value,
    TokenSet* destination) {
  if (!IsSafeDomToken(value)) {
    return true;
  }
  if (destination == nullptr || destination->contains(value)) {
    return destination != nullptr;
  }
  if (classes_.size() + ids_.size() == kMaximumCosmeticDomEntries ||
      value.size() > kMaximumCosmeticDomSnapshotBytes - token_byte_count_) {
    limit_exceeded_ = true;
    return false;
  }
  destination->emplace(value);
  token_byte_count_ += value.size();
  return true;
}

}  // namespace fireball::chromium
