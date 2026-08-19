#include "fireball/components/adblock/cosmetic_evaluator.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fireball::adblock {
namespace {

constexpr std::size_t kMaximumCosmeticJsonBytes = 1024 * 1024;
constexpr std::size_t kMaximumCosmeticEntries = 4096;
constexpr std::size_t kMaximumGenericSelectors = 8192;
constexpr std::size_t kMaximumSelectorBytes = 4096;
constexpr std::size_t kMaximumProceduralActionBytes = 8192;
constexpr std::size_t kMaximumExceptionBytes = 256;
constexpr std::size_t kMaximumInjectedScriptBytes = 256 * 1024;
constexpr std::size_t kMaximumDomJsonBytes = 256 * 1024;
constexpr std::size_t kMaximumDomEntries = 4096;
constexpr std::size_t kMaximumDomTokenBytes = 256;
constexpr std::size_t kMaximumUrlBytes = 8192;

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

void AppendUtf8(std::uint32_t codepoint, std::string* output) {
  if (codepoint <= 0x7fU) {
    output->push_back(static_cast<char>(codepoint));
  } else if (codepoint <= 0x7ffU) {
    output->push_back(static_cast<char>(0xc0U | (codepoint >> 6U)));
    output->push_back(static_cast<char>(0x80U | (codepoint & 0x3fU)));
  } else if (codepoint <= 0xffffU) {
    output->push_back(static_cast<char>(0xe0U | (codepoint >> 12U)));
    output->push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3fU)));
    output->push_back(static_cast<char>(0x80U | (codepoint & 0x3fU)));
  } else {
    output->push_back(static_cast<char>(0xf0U | (codepoint >> 18U)));
    output->push_back(static_cast<char>(0x80U | ((codepoint >> 12U) & 0x3fU)));
    output->push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3fU)));
    output->push_back(static_cast<char>(0x80U | (codepoint & 0x3fU)));
  }
}

int HexValue(char value) {
  if (value >= '0' && value <= '9') {
    return value - '0';
  }
  if (value >= 'a' && value <= 'f') {
    return value - 'a' + 10;
  }
  if (value >= 'A' && value <= 'F') {
    return value - 'A' + 10;
  }
  return -1;
}

class JsonReader final {
 public:
  explicit JsonReader(std::string_view input) : input_(input) {}

  bool Consume(char expected) {
    if (position_ >= input_.size() || input_[position_] != expected) {
      return false;
    }
    ++position_;
    return true;
  }

  bool AtEnd() const { return position_ == input_.size(); }

  std::optional<bool> ParseBoolean() {
    if (input_.substr(position_).starts_with("true")) {
      position_ += 4;
      return true;
    }
    if (input_.substr(position_).starts_with("false")) {
      position_ += 5;
      return false;
    }
    return std::nullopt;
  }

  std::optional<std::string> ParseString(std::size_t maximum_bytes) {
    if (!Consume('"')) {
      return std::nullopt;
    }
    std::string output;
    while (position_ < input_.size()) {
      const unsigned char byte =
          static_cast<unsigned char>(input_[position_++]);
      if (byte == '"') {
        if (output.size() > maximum_bytes || !IsValidUtf8(output)) {
          return std::nullopt;
        }
        return output;
      }
      if (byte < 0x20U) {
        return std::nullopt;
      }
      if (byte != '\\') {
        output.push_back(static_cast<char>(byte));
      } else {
        if (position_ >= input_.size()) {
          return std::nullopt;
        }
        const char escaped = input_[position_++];
        switch (escaped) {
          case '"':
          case '\\':
          case '/':
            output.push_back(escaped);
            break;
          case 'b':
            output.push_back('\b');
            break;
          case 'f':
            output.push_back('\f');
            break;
          case 'n':
            output.push_back('\n');
            break;
          case 'r':
            output.push_back('\r');
            break;
          case 't':
            output.push_back('\t');
            break;
          case 'u': {
            auto code_unit = ParseHexCodeUnit();
            if (!code_unit.has_value()) {
              return std::nullopt;
            }
            std::uint32_t codepoint = *code_unit;
            if (codepoint >= 0xd800U && codepoint <= 0xdbffU) {
              if (!Consume('\\') || !Consume('u')) {
                return std::nullopt;
              }
              auto low = ParseHexCodeUnit();
              if (!low.has_value() || *low < 0xdc00U || *low > 0xdfffU) {
                return std::nullopt;
              }
              codepoint =
                  0x10000U + ((codepoint - 0xd800U) << 10U) + (*low - 0xdc00U);
            } else if (codepoint >= 0xdc00U && codepoint <= 0xdfffU) {
              return std::nullopt;
            }
            AppendUtf8(codepoint, &output);
            break;
          }
          default:
            return std::nullopt;
        }
      }
      if (output.size() > maximum_bytes) {
        return std::nullopt;
      }
    }
    return std::nullopt;
  }

  std::optional<std::vector<std::string>> ParseStringArray(
      std::size_t maximum_count, std::size_t maximum_entry_bytes) {
    if (!Consume('[')) {
      return std::nullopt;
    }
    std::vector<std::string> values;
    if (Consume(']')) {
      return values;
    }
    while (true) {
      if (values.size() == maximum_count) {
        return std::nullopt;
      }
      auto value = ParseString(maximum_entry_bytes);
      if (!value.has_value() || value->empty() ||
          value->find('\0') != std::string::npos) {
        return std::nullopt;
      }
      values.push_back(std::move(*value));
      if (Consume(']')) {
        return values;
      }
      if (!Consume(',')) {
        return std::nullopt;
      }
    }
  }

 private:
  std::optional<std::uint32_t> ParseHexCodeUnit() {
    if (position_ + 4 > input_.size()) {
      return std::nullopt;
    }
    std::uint32_t value = 0;
    for (std::size_t index = 0; index < 4; ++index) {
      const int digit = HexValue(input_[position_++]);
      if (digit < 0) {
        return std::nullopt;
      }
      value = (value << 4U) | static_cast<std::uint32_t>(digit);
    }
    return value;
  }

  std::string_view input_;
  std::size_t position_ = 0;
};

bool IsSortedUnique(const std::vector<std::string>& values) {
  return std::adjacent_find(
             values.begin(), values.end(),
             [](const std::string& left, const std::string& right) {
               return left >= right;
             }) == values.end();
}

bool IsSafeUrlInput(std::string_view url) {
  if (url.empty() || url.size() > kMaximumUrlBytes ||
      !(url.starts_with("https://") || url.starts_with("http://"))) {
    return false;
  }
  return IsValidUtf8(url) &&
         std::none_of(url.begin(), url.end(), [](char character) {
           const unsigned char value = static_cast<unsigned char>(character);
           return value <= 0x20U || value == 0x7fU || character == '\\';
         });
}

bool IsSafeDomValue(std::string_view value) {
  return !value.empty() && value.size() <= kMaximumDomTokenBytes &&
         IsValidUtf8(value) &&
         std::none_of(value.begin(), value.end(), [](char character) {
           const unsigned char byte = static_cast<unsigned char>(character);
           return byte < 0x20U || byte == 0x7fU;
         });
}

void AppendJsonString(std::string_view value, std::string* output) {
  constexpr std::array<char, 16> kHex = {'0', '1', '2', '3', '4', '5',
                                         '6', '7', '8', '9', 'a', 'b',
                                         'c', 'd', 'e', 'f'};
  output->push_back('"');
  for (const unsigned char byte : value) {
    if (byte == '"' || byte == '\\') {
      output->push_back('\\');
      output->push_back(static_cast<char>(byte));
    } else if (byte < 0x20U) {
      output->append("\\u00");
      output->push_back(kHex[byte >> 4U]);
      output->push_back(kHex[byte & 0x0fU]);
    } else {
      output->push_back(static_cast<char>(byte));
    }
  }
  output->push_back('"');
}

std::optional<std::string> SerializeDomValues(
    const std::vector<std::string>& values) {
  if (values.size() > kMaximumDomEntries) {
    return std::nullopt;
  }
  std::string json = "[";
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (!IsSafeDomValue(values[index])) {
      return std::nullopt;
    }
    if (index != 0) {
      json.push_back(',');
    }
    AppendJsonString(values[index], &json);
    if (json.size() > kMaximumDomJsonBytes) {
      return std::nullopt;
    }
  }
  json.push_back(']');
  if (json.size() > kMaximumDomJsonBytes) {
    return std::nullopt;
  }
  return json;
}

struct ConsumedFfiString {
  bool valid = false;
  std::string value;
};

ConsumedFfiString ConsumeFfiString(char* pointer) {
  if (pointer == nullptr) {
    return {};
  }
  const std::size_t length = strnlen(pointer, kMaximumCosmeticJsonBytes + 1);
  ConsumedFfiString result;
  if (length <= kMaximumCosmeticJsonBytes) {
    result.valid = true;
    result.value.assign(pointer, length);
  }
  fireball_adblock_string_destroy(pointer);
  return result;
}

const std::uint8_t* Bytes(std::string_view value) {
  return reinterpret_cast<const std::uint8_t*>(value.data());
}

}  // namespace

namespace internal {

std::optional<CosmeticEvaluation> ParseCosmeticEvaluationJson(
    std::string_view json) {
  if (json.empty() || json.size() > kMaximumCosmeticJsonBytes ||
      !IsValidUtf8(json)) {
    return std::nullopt;
  }
  JsonReader reader(json);
  if (!reader.Consume('{')) {
    return std::nullopt;
  }
  CosmeticEvaluation output;
  output.status = CosmeticEvaluationStatus::kOk;
  std::uint32_t seen = 0;
  constexpr std::uint32_t kAllFields = (1U << 5U) - 1U;
  while (true) {
    if (reader.Consume('}')) {
      break;
    }
    auto key = reader.ParseString(32);
    if (!key.has_value() || !reader.Consume(':')) {
      return std::nullopt;
    }
    std::uint32_t field = 0;
    if (*key == "hide_selectors") {
      field = 1U << 0U;
      auto values = reader.ParseStringArray(kMaximumCosmeticEntries,
                                            kMaximumSelectorBytes);
      if (!values.has_value() || !IsSortedUnique(*values)) {
        return std::nullopt;
      }
      output.hide_selectors = std::move(*values);
    } else if (*key == "procedural_actions") {
      field = 1U << 1U;
      auto values = reader.ParseStringArray(kMaximumCosmeticEntries,
                                            kMaximumProceduralActionBytes);
      if (!values.has_value() || !IsSortedUnique(*values)) {
        return std::nullopt;
      }
      output.procedural_action_count = values->size();
    } else if (*key == "exceptions") {
      field = 1U << 2U;
      auto values = reader.ParseStringArray(kMaximumCosmeticEntries,
                                            kMaximumExceptionBytes);
      if (!values.has_value() || !IsSortedUnique(*values)) {
        return std::nullopt;
      }
      output.exceptions = std::move(*values);
    } else if (*key == "injected_script") {
      field = 1U << 3U;
      auto value = reader.ParseString(kMaximumInjectedScriptBytes);
      if (!value.has_value() || value->find('\0') != std::string::npos) {
        return std::nullopt;
      }
      output.has_injected_script = !value->empty();
    } else if (*key == "generichide") {
      field = 1U << 4U;
      auto value = reader.ParseBoolean();
      if (!value.has_value()) {
        return std::nullopt;
      }
      output.generic_hiding_disabled = *value;
    } else {
      return std::nullopt;
    }
    if ((seen & field) != 0) {
      return std::nullopt;
    }
    seen |= field;
    if (reader.Consume('}')) {
      break;
    }
    if (!reader.Consume(',')) {
      return std::nullopt;
    }
  }
  if (seen != kAllFields || !reader.AtEnd()) {
    return std::nullopt;
  }
  return output;
}

std::optional<std::vector<std::string>> ParseSelectorArrayJson(
    std::string_view json) {
  if (json.empty() || json.size() > kMaximumCosmeticJsonBytes ||
      !IsValidUtf8(json)) {
    return std::nullopt;
  }
  JsonReader reader(json);
  auto selectors =
      reader.ParseStringArray(kMaximumGenericSelectors, kMaximumSelectorBytes);
  if (!selectors.has_value() || !reader.AtEnd() ||
      !IsSortedUnique(*selectors)) {
    return std::nullopt;
  }
  return selectors;
}

}  // namespace internal

FfiCosmeticEvaluator::FfiCosmeticEvaluator(const FireballAdblockEngine* engine)
    : engine_(engine) {}

CosmeticEvaluation FfiCosmeticEvaluator::EvaluatePage(std::string_view url) {
  if (engine_ == nullptr) {
    return {};
  }
  if (!IsSafeUrlInput(url)) {
    CosmeticEvaluation result;
    result.status = CosmeticEvaluationStatus::kInvalidInput;
    return result;
  }
  ConsumedFfiString json = ConsumeFfiString(
      fireball_adblock_cosmetic_resources(engine_, Bytes(url), url.size()));
  if (!json.valid) {
    return {};
  }
  auto parsed = internal::ParseCosmeticEvaluationJson(json.value);
  return parsed.has_value() ? std::move(*parsed) : CosmeticEvaluation{};
}

GenericSelectorEvaluation FfiCosmeticEvaluator::EvaluateGenericSelectors(
    const std::vector<std::string>& classes,
    const std::vector<std::string>& ids,
    const std::vector<std::string>& exceptions) {
  if (engine_ == nullptr) {
    return {};
  }
  auto classes_json = SerializeDomValues(classes);
  auto ids_json = SerializeDomValues(ids);
  auto exceptions_json = SerializeDomValues(exceptions);
  if (!classes_json.has_value() || !ids_json.has_value() ||
      !exceptions_json.has_value()) {
    GenericSelectorEvaluation result;
    result.status = CosmeticEvaluationStatus::kInvalidInput;
    return result;
  }
  ConsumedFfiString json = ConsumeFfiString(fireball_adblock_hidden_selectors(
      engine_, Bytes(*classes_json), classes_json->size(), Bytes(*ids_json),
      ids_json->size(), Bytes(*exceptions_json), exceptions_json->size()));
  if (!json.valid) {
    return {};
  }
  auto selectors = internal::ParseSelectorArrayJson(json.value);
  if (!selectors.has_value()) {
    return {};
  }
  return {CosmeticEvaluationStatus::kOk, std::move(*selectors)};
}

}  // namespace fireball::adblock
