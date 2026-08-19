#include "fireball/components/transfer/dash_vod.h"

#include "fireball/components/transfer/transfer_types.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fireball::transfer {
namespace {

constexpr std::size_t kMaximumXmlDepth = 32;
constexpr std::size_t kMaximumXmlNodes = 8192;
constexpr std::size_t kMaximumXmlAttributes = 64;
constexpr std::size_t kMaximumXmlNameBytes = 128;
constexpr std::size_t kMaximumXmlTextBytes = 16 * 1024;
constexpr std::uint64_t kMaximumDashTimescale = 1'000'000'000;

struct XmlNode {
  std::string name;
  std::map<std::string, std::string, std::less<>> attributes;
  std::string text;
  std::vector<XmlNode> children;
};

bool IsValidUtf8(std::string_view input) {
  std::size_t index = 0;
  while (index < input.size()) {
    const unsigned char first = static_cast<unsigned char>(input[index]);
    if (first < 0x80) {
      if (first == 0) {
        return false;
      }
      ++index;
      continue;
    }
    std::size_t length = 0;
    std::uint32_t codepoint = 0;
    std::uint32_t minimum = 0;
    if ((first & 0xe0) == 0xc0) {
      length = 2;
      codepoint = first & 0x1f;
      minimum = 0x80;
    } else if ((first & 0xf0) == 0xe0) {
      length = 3;
      codepoint = first & 0x0f;
      minimum = 0x800;
    } else if ((first & 0xf8) == 0xf0) {
      length = 4;
      codepoint = first & 0x07;
      minimum = 0x10000;
    } else {
      return false;
    }
    if (index + length > input.size()) {
      return false;
    }
    for (std::size_t offset = 1; offset < length; ++offset) {
      const unsigned char continuation =
          static_cast<unsigned char>(input[index + offset]);
      if ((continuation & 0xc0) != 0x80) {
        return false;
      }
      codepoint = (codepoint << 6) | (continuation & 0x3f);
    }
    if (codepoint < minimum || codepoint > 0x10ffff ||
        (codepoint >= 0xd800 && codepoint <= 0xdfff)) {
      return false;
    }
    index += length;
  }
  return true;
}

bool IsXmlNameStart(char value) {
  const unsigned char byte = static_cast<unsigned char>(value);
  return std::isalpha(byte) || value == '_' || value == ':';
}

bool IsXmlNameByte(char value) {
  const unsigned char byte = static_cast<unsigned char>(value);
  return std::isalnum(byte) || value == '_' || value == ':' || value == '-' ||
         value == '.';
}

bool IsXmlCodepoint(std::uint32_t value) {
  return value == 0x09 || value == 0x0a || value == 0x0d ||
         (value >= 0x20 && value <= 0xd7ff) ||
         (value >= 0xe000 && value <= 0xfffd) ||
         (value >= 0x10000 && value <= 0x10ffff);
}

bool AppendUtf8(std::uint32_t value, std::string* output) {
  if (!IsXmlCodepoint(value)) {
    return false;
  }
  if (value <= 0x7f) {
    output->push_back(static_cast<char>(value));
  } else if (value <= 0x7ff) {
    output->push_back(static_cast<char>(0xc0 | (value >> 6)));
    output->push_back(static_cast<char>(0x80 | (value & 0x3f)));
  } else if (value <= 0xffff) {
    output->push_back(static_cast<char>(0xe0 | (value >> 12)));
    output->push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
    output->push_back(static_cast<char>(0x80 | (value & 0x3f)));
  } else {
    output->push_back(static_cast<char>(0xf0 | (value >> 18)));
    output->push_back(static_cast<char>(0x80 | ((value >> 12) & 0x3f)));
    output->push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
    output->push_back(static_cast<char>(0x80 | (value & 0x3f)));
  }
  return true;
}

bool DecodeXmlText(std::string_view input, std::string* output) {
  output->clear();
  output->reserve(input.size());
  for (std::size_t index = 0; index < input.size();) {
    if (input[index] != '&') {
      output->push_back(input[index++]);
      continue;
    }
    const std::size_t end = input.find(';', index + 1);
    if (end == std::string_view::npos || end - index > 12) {
      return false;
    }
    const std::string_view entity = input.substr(index + 1, end - index - 1);
    if (entity == "amp") {
      output->push_back('&');
    } else if (entity == "lt") {
      output->push_back('<');
    } else if (entity == "gt") {
      output->push_back('>');
    } else if (entity == "quot") {
      output->push_back('"');
    } else if (entity == "apos") {
      output->push_back('\'');
    } else if (entity.starts_with("#")) {
      int base = 10;
      std::string_view digits = entity.substr(1);
      if (digits.starts_with("x") || digits.starts_with("X")) {
        base = 16;
        digits.remove_prefix(1);
      }
      if (digits.empty()) {
        return false;
      }
      std::uint32_t codepoint = 0;
      const auto result = std::from_chars(digits.data(),
                                          digits.data() + digits.size(),
                                          codepoint, base);
      if (result.ec != std::errc() || result.ptr != digits.data() + digits.size() ||
          !AppendUtf8(codepoint, output)) {
        return false;
      }
    } else {
      return false;
    }
    if (output->size() > kMaximumXmlTextBytes) {
      return false;
    }
    index = end + 1;
  }
  return output->size() <= kMaximumXmlTextBytes;
}

class ClosedXmlParser final {
 public:
  explicit ClosedXmlParser(std::string_view input) : input_(input) {}

  std::optional<XmlNode> Parse() {
    if (input_.empty() || input_.size() > kMaximumDashManifestBytes ||
        !IsValidUtf8(input_)) {
      return std::nullopt;
    }
    if (input_.starts_with("\xef\xbb\xbf")) {
      position_ = 3;
    }
    SkipSpace();
    if (StartsWith("<?xml")) {
      const std::size_t end = input_.find("?>", position_ + 5);
      if (end == std::string_view::npos) {
        return std::nullopt;
      }
      position_ = end + 2;
    }
    if (!SkipTrivia()) {
      return std::nullopt;
    }
    auto root = ParseElement(0);
    if (!root.has_value() || !SkipTrivia() || position_ != input_.size()) {
      return std::nullopt;
    }
    return root;
  }

 private:
  bool StartsWith(std::string_view value) const {
    return input_.substr(position_).starts_with(value);
  }

  void SkipSpace() {
    while (position_ < input_.size() &&
           (input_[position_] == ' ' || input_[position_] == '\t' ||
            input_[position_] == '\r' || input_[position_] == '\n')) {
      ++position_;
    }
  }

  bool SkipComment() {
    if (!StartsWith("<!--")) {
      return false;
    }
    const std::size_t end = input_.find("-->", position_ + 4);
    if (end == std::string_view::npos ||
        input_.substr(position_ + 4, end - position_ - 4).find("--") !=
            std::string_view::npos) {
      position_ = input_.size();
      return false;
    }
    position_ = end + 3;
    return true;
  }

  bool SkipTrivia() {
    while (true) {
      SkipSpace();
      if (!StartsWith("<!--")) {
        return true;
      }
      if (!SkipComment()) {
        return false;
      }
    }
  }

  std::optional<std::string> ParseName() {
    if (position_ >= input_.size() || !IsXmlNameStart(input_[position_])) {
      return std::nullopt;
    }
    const std::size_t start = position_++;
    while (position_ < input_.size() && IsXmlNameByte(input_[position_])) {
      ++position_;
    }
    if (position_ - start > kMaximumXmlNameBytes) {
      return std::nullopt;
    }
    return std::string(input_.substr(start, position_ - start));
  }

  std::optional<std::string> ParseAttributeValue() {
    if (position_ >= input_.size() ||
        (input_[position_] != '"' && input_[position_] != '\'')) {
      return std::nullopt;
    }
    const char quote = input_[position_++];
    const std::size_t start = position_;
    while (position_ < input_.size() && input_[position_] != quote) {
      const unsigned char byte = static_cast<unsigned char>(input_[position_]);
      if (input_[position_] == '<' || byte < 0x20) {
        return std::nullopt;
      }
      ++position_;
    }
    if (position_ >= input_.size()) {
      return std::nullopt;
    }
    std::string decoded;
    if (!DecodeXmlText(input_.substr(start, position_ - start), &decoded)) {
      return std::nullopt;
    }
    ++position_;
    return decoded;
  }

  std::optional<XmlNode> ParseElement(std::size_t depth) {
    if (depth >= kMaximumXmlDepth || position_ >= input_.size() ||
        input_[position_] != '<' || StartsWith("</") || StartsWith("<!") ||
        StartsWith("<?") || ++node_count_ > kMaximumXmlNodes) {
      return std::nullopt;
    }
    ++position_;
    auto name = ParseName();
    if (!name.has_value()) {
      return std::nullopt;
    }
    XmlNode node;
    node.name = std::move(*name);

    while (true) {
      SkipSpace();
      if (StartsWith("/>")) {
        position_ += 2;
        return node;
      }
      if (StartsWith(">")) {
        ++position_;
        break;
      }
      if (node.attributes.size() >= kMaximumXmlAttributes) {
        return std::nullopt;
      }
      auto attribute_name = ParseName();
      if (!attribute_name.has_value()) {
        return std::nullopt;
      }
      SkipSpace();
      if (position_ >= input_.size() || input_[position_++] != '=') {
        return std::nullopt;
      }
      SkipSpace();
      auto value = ParseAttributeValue();
      if (!value.has_value() ||
          !node.attributes.emplace(std::move(*attribute_name),
                                   std::move(*value)).second) {
        return std::nullopt;
      }
    }

    while (position_ < input_.size()) {
      if (StartsWith("</")) {
        position_ += 2;
        auto closing_name = ParseName();
        SkipSpace();
        if (!closing_name.has_value() || *closing_name != node.name ||
            position_ >= input_.size() || input_[position_++] != '>') {
          return std::nullopt;
        }
        return node;
      }
      if (StartsWith("<!--")) {
        if (!SkipComment()) {
          return std::nullopt;
        }
        continue;
      }
      if (input_[position_] == '<') {
        auto child = ParseElement(depth + 1);
        if (!child.has_value()) {
          return std::nullopt;
        }
        node.children.push_back(std::move(*child));
        continue;
      }
      const std::size_t end = input_.find('<', position_);
      if (end == std::string_view::npos) {
        return std::nullopt;
      }
      std::string decoded;
      if (!DecodeXmlText(input_.substr(position_, end - position_), &decoded) ||
          node.text.size() + decoded.size() > kMaximumXmlTextBytes) {
        return std::nullopt;
      }
      node.text += decoded;
      position_ = end;
    }
    return std::nullopt;
  }

  std::string_view input_;
  std::size_t position_ = 0;
  std::size_t node_count_ = 0;
};

std::string_view LocalName(std::string_view name) {
  const std::size_t colon = name.rfind(':');
  return colon == std::string_view::npos ? name : name.substr(colon + 1);
}

const std::string* Attribute(const XmlNode& node, std::string_view name) {
  const auto attribute = node.attributes.find(name);
  return attribute == node.attributes.end() ? nullptr : &attribute->second;
}

std::vector<const XmlNode*> Children(const XmlNode& node,
                                     std::string_view local_name) {
  std::vector<const XmlNode*> matches;
  for (const XmlNode& child : node.children) {
    if (LocalName(child.name) == local_name) {
      matches.push_back(&child);
    }
  }
  return matches;
}

bool HasDescendant(const XmlNode& node, std::string_view local_name) {
  if (LocalName(node.name) == local_name) {
    return true;
  }
  return std::any_of(node.children.begin(), node.children.end(),
                     [local_name](const XmlNode& child) {
                       return HasDescendant(child, local_name);
                     });
}

bool HasXlink(const XmlNode& node) {
  if (std::any_of(node.attributes.begin(), node.attributes.end(),
                  [](const auto& attribute) {
                    return attribute.first.starts_with("xlink:");
                  })) {
    return true;
  }
  return std::any_of(node.children.begin(), node.children.end(), HasXlink);
}

std::string_view Trim(std::string_view value) {
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
    value.remove_prefix(1);
  }
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
    value.remove_suffix(1);
  }
  return value;
}

template <typename T>
std::optional<T> ParseUnsigned(std::string_view value) {
  if (value.empty() || value.front() == '+' || value.front() == '-') {
    return std::nullopt;
  }
  T parsed = 0;
  const auto result =
      std::from_chars(value.data(), value.data() + value.size(), parsed);
  if (result.ec != std::errc() || result.ptr != value.data() + value.size()) {
    return std::nullopt;
  }
  return parsed;
}

std::optional<std::uint64_t> ParseDurationMs(std::string_view value) {
  if (!value.starts_with("PT")) {
    return std::nullopt;
  }
  value.remove_prefix(2);
  if (value.empty()) {
    return std::nullopt;
  }
  std::uint64_t total_ms = 0;
  int last_unit = 0;
  bool found = false;
  while (!value.empty()) {
    const std::size_t unit_position = value.find_first_of("HMS");
    if (unit_position == std::string_view::npos || unit_position == 0) {
      return std::nullopt;
    }
    const char unit = value[unit_position];
    const int unit_order = unit == 'H' ? 1 : (unit == 'M' ? 2 : 3);
    if (unit_order <= last_unit) {
      return std::nullopt;
    }
    const std::string_view number = value.substr(0, unit_position);
    std::uint64_t milliseconds = 0;
    if (unit == 'S') {
      const std::size_t decimal = number.find('.');
      const std::string_view whole = number.substr(0, decimal);
      auto seconds = ParseUnsigned<std::uint64_t>(whole);
      if (!seconds.has_value() || *seconds > kMaximumDashDurationMs / 1000) {
        return std::nullopt;
      }
      milliseconds = *seconds * 1000;
      if (decimal != std::string_view::npos) {
        const std::string_view fraction = number.substr(decimal + 1);
        if (fraction.empty() || fraction.size() > 9 ||
            !std::all_of(fraction.begin(), fraction.end(), [](char value) {
              return value >= '0' && value <= '9';
            })) {
          return std::nullopt;
        }
        auto fraction_value = ParseUnsigned<std::uint64_t>(fraction);
        if (!fraction_value.has_value()) {
          return std::nullopt;
        }
        if (fraction.size() <= 3) {
          for (std::size_t index = fraction.size(); index < 3; ++index) {
            *fraction_value *= 10;
          }
          milliseconds += *fraction_value;
        } else {
          auto leading = ParseUnsigned<std::uint64_t>(fraction.substr(0, 3));
          if (!leading.has_value()) {
            return std::nullopt;
          }
          milliseconds += *leading;
          if (std::any_of(fraction.begin() + 3, fraction.end(),
                          [](char value) { return value != '0'; })) {
            ++milliseconds;
          }
        }
      }
    } else {
      auto amount = ParseUnsigned<std::uint64_t>(number);
      const std::uint64_t multiplier = unit == 'H' ? 3'600'000 : 60'000;
      if (!amount.has_value() ||
          *amount > kMaximumDashDurationMs / multiplier) {
        return std::nullopt;
      }
      milliseconds = *amount * multiplier;
    }
    if (total_ms > kMaximumDashDurationMs - milliseconds) {
      return std::nullopt;
    }
    total_ms += milliseconds;
    found = true;
    last_unit = unit_order;
    value.remove_prefix(unit_position + 1);
  }
  return found && total_ms > 0 && total_ms <= kMaximumDashDurationMs
             ? std::optional<std::uint64_t>(total_ms)
             : std::nullopt;
}

void RemoveLastPathSegment(std::string* output) {
  const std::size_t slash = output->rfind('/');
  if (slash == std::string::npos) {
    output->clear();
  } else {
    output->erase(slash);
  }
}

std::string RemoveDotSegments(std::string input) {
  std::string output;
  output.reserve(input.size());
  while (!input.empty()) {
    if (input.starts_with("../")) {
      input.erase(0, 3);
    } else if (input.starts_with("./")) {
      input.erase(0, 2);
    } else if (input.starts_with("/./")) {
      input.erase(0, 2);
    } else if (input == "/.") {
      input = "/";
    } else if (input.starts_with("/../")) {
      input.erase(0, 3);
      RemoveLastPathSegment(&output);
    } else if (input == "/..") {
      input = "/";
      RemoveLastPathSegment(&output);
    } else if (input == "." || input == "..") {
      input.clear();
    } else {
      const std::size_t next =
          input.front() == '/' ? input.find('/', 1) : input.find('/');
      const std::size_t length =
          next == std::string::npos ? input.size() : next;
      output.append(input, 0, length);
      input.erase(0, length);
    }
  }
  return output;
}

std::optional<std::string> ResolveUri(std::string_view base,
                                      std::string_view reference) {
  reference = Trim(reference);
  if (!IsSafeHttpDownloadUri(base) || reference.empty() ||
      reference.size() > kMaximumUriBytes ||
      reference.find_first_of("\r\n\t\\#") != std::string_view::npos) {
    return std::nullopt;
  }
  if (reference.starts_with("http://") || reference.starts_with("https://")) {
    return IsSafeHttpDownloadUri(reference)
               ? std::optional<std::string>(reference)
               : std::nullopt;
  }
  const std::size_t scheme_end = base.find("://");
  const std::size_t authority_start = scheme_end + 3;
  const std::size_t authority_end = base.find_first_of("/?#", authority_start);
  const std::string_view origin =
      base.substr(0, authority_end == std::string_view::npos ? base.size()
                                                             : authority_end);
  const std::string_view scheme = base.substr(0, scheme_end);
  if (reference.starts_with("//")) {
    const std::string result = std::string(scheme) + ":" +
                               std::string(reference);
    return IsSafeHttpDownloadUri(result)
               ? std::optional<std::string>(result)
               : std::nullopt;
  }
  const std::size_t query = reference.find('?');
  const std::string_view reference_path = reference.substr(0, query);
  const std::string_view reference_query =
      query == std::string_view::npos ? std::string_view()
                                      : reference.substr(query);
  if (reference_path.empty()) {
    return std::nullopt;
  }
  std::string combined;
  if (reference_path.front() == '/') {
    combined.assign(reference_path);
  } else {
    std::string_view base_path = authority_end == std::string_view::npos
                                     ? std::string_view("/")
                                     : base.substr(authority_end);
    base_path = base_path.substr(0, base_path.find_first_of("?#"));
    const std::size_t slash = base_path.rfind('/');
    combined = std::string(base_path.substr(0, slash + 1));
    combined += reference_path;
  }
  std::string normalized = RemoveDotSegments(std::move(combined));
  if (normalized.empty() || normalized.front() != '/') {
    normalized.insert(normalized.begin(), '/');
  }
  const std::string result = std::string(origin) + normalized +
                             std::string(reference_query);
  return IsSafeHttpDownloadUri(result) ? std::optional<std::string>(result)
                                       : std::nullopt;
}

std::optional<std::string> ApplyBaseUrl(std::string base,
                                        const XmlNode& node) {
  const auto base_urls = Children(node, "BaseURL");
  if (base_urls.size() > 1) {
    return std::nullopt;
  }
  if (base_urls.empty()) {
    return base;
  }
  if (!base_urls.front()->children.empty()) {
    return std::nullopt;
  }
  return ResolveUri(base, base_urls.front()->text);
}

bool IsSafeRepresentationId(std::string_view value) {
  return !value.empty() && value.size() <= 128 &&
         std::all_of(value.begin(), value.end(), [](char character) {
           const unsigned char byte = static_cast<unsigned char>(character);
           return std::isalnum(byte) || character == '-' || character == '_' ||
                  character == '.';
         });
}

bool IsSupportedCodec(DashTrackKind kind, std::string_view value) {
  if (value.find(',') != std::string_view::npos) {
    return false;
  }
  const std::string_view first = Trim(value);
  if (kind == DashTrackKind::kVideo) {
    return first.starts_with("avc1") || first.starts_with("avc3") ||
           first.starts_with("hev1") || first.starts_with("hvc1") ||
           first.starts_with("av01") || first.starts_with("vp09");
  }
  return first.starts_with("mp4a") || first.starts_with("opus") ||
         first.starts_with("ac-3") || first.starts_with("ec-3");
}

bool IsFmp4Uri(std::string_view uri) {
  const std::size_t query = uri.find_first_of("?#");
  std::string path(uri.substr(0, query));
  std::transform(path.begin(), path.end(), path.begin(), [](char value) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
  });
  return path.ends_with(".mp4") || path.ends_with(".m4s") ||
         path.ends_with(".cmfv") || path.ends_with(".cmfa");
}

struct SegmentTemplateInfo {
  std::string initialization;
  std::string media;
  std::uint64_t timescale = 1;
  std::uint64_t duration = 0;
  std::uint64_t start_number = 1;
  const XmlNode* timeline = nullptr;
};

bool ApplyTemplateNode(const XmlNode& node, SegmentTemplateInfo* output) {
  if (const std::string* value = Attribute(node, "initialization")) {
    output->initialization = *value;
  }
  if (const std::string* value = Attribute(node, "media")) {
    output->media = *value;
  }
  if (const std::string* value = Attribute(node, "timescale")) {
    auto parsed = ParseUnsigned<std::uint64_t>(*value);
    if (!parsed.has_value() || *parsed == 0 ||
        *parsed > kMaximumDashTimescale) {
      return false;
    }
    output->timescale = *parsed;
  }
  if (const std::string* value = Attribute(node, "duration")) {
    auto parsed = ParseUnsigned<std::uint64_t>(*value);
    if (!parsed.has_value() || *parsed == 0) {
      return false;
    }
    output->duration = *parsed;
  }
  if (const std::string* value = Attribute(node, "startNumber")) {
    auto parsed = ParseUnsigned<std::uint64_t>(*value);
    if (!parsed.has_value() || *parsed == 0) {
      return false;
    }
    output->start_number = *parsed;
  }
  const auto timelines = Children(node, "SegmentTimeline");
  if (timelines.size() > 1 ||
      std::any_of(node.children.begin(), node.children.end(),
                  [](const XmlNode& child) {
                    return LocalName(child.name) != "SegmentTimeline";
                  })) {
    return false;
  }
  if (!timelines.empty()) {
    output->timeline = timelines.front();
  }
  return true;
}

std::optional<SegmentTemplateInfo> ResolveTemplate(
    const XmlNode& adaptation,
    const XmlNode& representation) {
  const auto parent_templates = Children(adaptation, "SegmentTemplate");
  const auto child_templates = Children(representation, "SegmentTemplate");
  if (parent_templates.size() > 1 || child_templates.size() > 1 ||
      (parent_templates.empty() && child_templates.empty())) {
    return std::nullopt;
  }
  SegmentTemplateInfo info;
  if (!parent_templates.empty() &&
      !ApplyTemplateNode(*parent_templates.front(), &info)) {
    return std::nullopt;
  }
  if (!child_templates.empty() &&
      !ApplyTemplateNode(*child_templates.front(), &info)) {
    return std::nullopt;
  }
  if (info.initialization.empty() || info.media.empty() ||
      (info.duration == 0) == (info.timeline == nullptr)) {
    return std::nullopt;
  }
  return info;
}

std::optional<std::string> ExpandTemplate(
    std::string_view pattern,
    std::string_view representation_id,
    std::uint64_t bandwidth,
    std::optional<std::uint64_t> number,
    std::optional<std::uint64_t> time,
    bool* used_segment_variable) {
  std::string output;
  output.reserve(pattern.size() + 32);
  *used_segment_variable = false;
  for (std::size_t index = 0; index < pattern.size();) {
    if (pattern[index] != '$') {
      output.push_back(pattern[index++]);
      continue;
    }
    if (index + 1 < pattern.size() && pattern[index + 1] == '$') {
      output.push_back('$');
      index += 2;
      continue;
    }
    const std::size_t end = pattern.find('$', index + 1);
    if (end == std::string_view::npos) {
      return std::nullopt;
    }
    const std::string_view token = pattern.substr(index + 1, end - index - 1);
    if (token == "RepresentationID") {
      output += representation_id;
    } else {
      std::string_view name = token;
      std::size_t width = 0;
      const std::size_t percent = token.find('%');
      if (percent != std::string_view::npos) {
        name = token.substr(0, percent);
        std::string_view format = token.substr(percent + 1);
        if (!format.ends_with('d')) {
          return std::nullopt;
        }
        format.remove_suffix(1);
        if (!format.empty()) {
          if (format.starts_with('0')) {
            format.remove_prefix(1);
          }
          auto parsed_width = ParseUnsigned<std::size_t>(format);
          if (!parsed_width.has_value() || *parsed_width == 0 ||
              *parsed_width > 9) {
            return std::nullopt;
          }
          width = *parsed_width;
        }
      }
      std::optional<std::uint64_t> numeric;
      if (name == "Bandwidth") {
        numeric = bandwidth;
      } else if (name == "Number") {
        numeric = number;
        *used_segment_variable = true;
      } else if (name == "Time") {
        numeric = time;
        *used_segment_variable = true;
      } else {
        return std::nullopt;
      }
      if (!numeric.has_value()) {
        return std::nullopt;
      }
      std::string digits = std::to_string(*numeric);
      if (digits.size() < width) {
        output.append(width - digits.size(), '0');
      }
      output += digits;
    }
    if (output.size() > kMaximumUriBytes) {
      return std::nullopt;
    }
    index = end + 1;
  }
  return output;
}

struct SegmentPoint {
  std::uint64_t number = 0;
  std::uint64_t time = 0;
  std::uint64_t duration = 0;
};

std::optional<std::vector<SegmentPoint>> ExpandTimeline(
    const SegmentTemplateInfo& info) {
  std::vector<SegmentPoint> points;
  std::uint64_t current_time = 0;
  std::uint64_t number = info.start_number;
  const auto entries = Children(*info.timeline, "S");
  if (entries.empty() || entries.size() > kMaximumDashSegmentsPerTrack ||
      std::any_of(info.timeline->children.begin(),
                  info.timeline->children.end(), [](const XmlNode& child) {
                    return LocalName(child.name) != "S";
                  })) {
    return std::nullopt;
  }
  for (const XmlNode* entry : entries) {
    const std::string* duration_value = Attribute(*entry, "d");
    auto duration = duration_value == nullptr
                        ? std::nullopt
                        : ParseUnsigned<std::uint64_t>(*duration_value);
    if (!duration.has_value() || *duration == 0) {
      return std::nullopt;
    }
    if (const std::string* time_value = Attribute(*entry, "t")) {
      auto parsed_time = ParseUnsigned<std::uint64_t>(*time_value);
      if (!parsed_time.has_value()) {
        return std::nullopt;
      }
      current_time = *parsed_time;
    }
    std::uint64_t repeats = 0;
    if (const std::string* repeat_value = Attribute(*entry, "r")) {
      auto parsed_repeat = ParseUnsigned<std::uint64_t>(*repeat_value);
      if (!parsed_repeat.has_value()) {
        return std::nullopt;
      }
      repeats = *parsed_repeat;
    }
    if (repeats >= kMaximumDashSegmentsPerTrack ||
        points.size() + repeats + 1 > kMaximumDashSegmentsPerTrack) {
      return std::nullopt;
    }
    for (std::uint64_t repeat = 0; repeat <= repeats; ++repeat) {
      points.push_back({number, current_time, *duration});
      if (number == std::numeric_limits<std::uint64_t>::max() ||
          current_time > std::numeric_limits<std::uint64_t>::max() -
                             *duration) {
        return std::nullopt;
      }
      ++number;
      current_time += *duration;
    }
  }
  return points;
}

std::optional<std::vector<SegmentPoint>> ExpandDurationTemplate(
    const SegmentTemplateInfo& info,
    std::uint64_t presentation_duration_ms) {
  if (presentation_duration_ms >
      std::numeric_limits<std::uint64_t>::max() / info.timescale) {
    return std::nullopt;
  }
  const std::uint64_t scaled = presentation_duration_ms * info.timescale;
  const std::uint64_t total_units = (scaled + 999) / 1000;
  if (total_units == 0 ||
      total_units > std::numeric_limits<std::uint64_t>::max() -
                        (info.duration - 1)) {
    return std::nullopt;
  }
  const std::uint64_t count =
      (total_units + info.duration - 1) / info.duration;
  if (count == 0 || count > kMaximumDashSegmentsPerTrack ||
      info.start_number > std::numeric_limits<std::uint64_t>::max() - count) {
    return std::nullopt;
  }
  std::vector<SegmentPoint> points;
  points.reserve(static_cast<std::size_t>(count));
  for (std::uint64_t index = 0; index < count; ++index) {
    if (index > std::numeric_limits<std::uint64_t>::max() / info.duration) {
      return std::nullopt;
    }
    points.push_back(
        {info.start_number + index, index * info.duration, info.duration});
  }
  return points;
}

struct TrackBuildResult {
  std::optional<DashTrackPlan> track;
  DashVodError error = DashVodError::kInvalidManifest;
};

TrackBuildResult BuildTrack(const XmlNode& adaptation,
                            const XmlNode& representation,
                            DashTrackKind kind,
                            std::string base,
                            std::uint64_t presentation_duration_ms) {
  auto representation_base = ApplyBaseUrl(std::move(base), representation);
  if (!representation_base.has_value()) {
    return {std::nullopt, DashVodError::kUnsafeUri};
  }
  const std::string* id = Attribute(representation, "id");
  const std::string* bandwidth_value = Attribute(representation, "bandwidth");
  auto bandwidth = bandwidth_value == nullptr
                       ? std::nullopt
                       : ParseUnsigned<std::uint64_t>(*bandwidth_value);
  if (id == nullptr || !IsSafeRepresentationId(*id) ||
      !bandwidth.has_value() || *bandwidth == 0) {
    return {std::nullopt, DashVodError::kInvalidManifest};
  }
  const std::string* mime = Attribute(representation, "mimeType");
  if (mime == nullptr) {
    mime = Attribute(adaptation, "mimeType");
  }
  const std::string expected_mime =
      kind == DashTrackKind::kVideo ? "video/mp4" : "audio/mp4";
  if (mime == nullptr || *mime != expected_mime) {
    return {std::nullopt, DashVodError::kUnsupportedManifest};
  }
  const std::string* codecs = Attribute(representation, "codecs");
  if (codecs == nullptr) {
    codecs = Attribute(adaptation, "codecs");
  }
  if (codecs == nullptr || codecs->size() > 256 ||
      !IsSupportedCodec(kind, *codecs)) {
    return {std::nullopt, DashVodError::kUnsupportedManifest};
  }
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  if (kind == DashTrackKind::kVideo) {
    const std::string* width_value = Attribute(representation, "width");
    const std::string* height_value = Attribute(representation, "height");
    if (width_value == nullptr) {
      width_value = Attribute(adaptation, "width");
    }
    if (height_value == nullptr) {
      height_value = Attribute(adaptation, "height");
    }
    auto parsed_width = width_value == nullptr
                            ? std::nullopt
                            : ParseUnsigned<std::uint32_t>(*width_value);
    auto parsed_height = height_value == nullptr
                             ? std::nullopt
                             : ParseUnsigned<std::uint32_t>(*height_value);
    if (!parsed_width.has_value() || !parsed_height.has_value() ||
        *parsed_width == 0 || *parsed_height == 0 || *parsed_width > 16384 ||
        *parsed_height > 16384) {
      return {std::nullopt, DashVodError::kInvalidManifest};
    }
    width = *parsed_width;
    height = *parsed_height;
  }

  auto info = ResolveTemplate(adaptation, representation);
  if (!info.has_value()) {
    return {std::nullopt, DashVodError::kUnsupportedManifest};
  }
  bool initialization_used_segment_variable = false;
  auto initialization_reference = ExpandTemplate(
      info->initialization, *id, *bandwidth, std::nullopt, std::nullopt,
      &initialization_used_segment_variable);
  if (!initialization_reference.has_value() ||
      initialization_used_segment_variable) {
    return {std::nullopt, DashVodError::kInvalidManifest};
  }
  auto initialization_uri =
      ResolveUri(*representation_base, *initialization_reference);
  if (!initialization_uri.has_value() || !IsFmp4Uri(*initialization_uri)) {
    return {std::nullopt, DashVodError::kUnsafeUri};
  }
  auto points = info->timeline == nullptr
                    ? ExpandDurationTemplate(*info, presentation_duration_ms)
                    : ExpandTimeline(*info);
  if (!points.has_value()) {
    return {std::nullopt, DashVodError::kLimitExceeded};
  }
  const std::uint64_t maximum_track_units =
      kMaximumDashDurationMs * info->timescale / 1000;
  if (std::any_of(points->begin(), points->end(),
                  [maximum_track_units](const SegmentPoint& point) {
                    return point.time > maximum_track_units ||
                           point.duration > maximum_track_units - point.time;
                  })) {
    return {std::nullopt, DashVodError::kLimitExceeded};
  }
  DashTrackPlan track;
  track.kind = kind;
  track.representation_id = *id;
  track.mime_type = *mime;
  track.codecs = *codecs;
  track.bandwidth = *bandwidth;
  track.width = width;
  track.height = height;
  track.initialization_uri = std::move(*initialization_uri);
  track.segment_uris.reserve(points->size());
  for (const SegmentPoint& point : *points) {
    bool used_segment_variable = false;
    auto segment_reference = ExpandTemplate(
        info->media, *id, *bandwidth, point.number, point.time,
        &used_segment_variable);
    if (!segment_reference.has_value() || !used_segment_variable) {
      return {std::nullopt, DashVodError::kInvalidManifest};
    }
    auto segment_uri = ResolveUri(*representation_base, *segment_reference);
    if (!segment_uri.has_value() || !IsFmp4Uri(*segment_uri)) {
      return {std::nullopt, DashVodError::kUnsafeUri};
    }
    track.segment_uris.push_back(std::move(*segment_uri));
  }
  return {std::move(track), DashVodError::kNone};
}

std::optional<DashTrackKind> AdaptationKind(const XmlNode& adaptation) {
  if (const std::string* content_type = Attribute(adaptation, "contentType")) {
    if (*content_type == "video") {
      return DashTrackKind::kVideo;
    }
    if (*content_type == "audio") {
      return DashTrackKind::kAudio;
    }
  }
  if (const std::string* mime = Attribute(adaptation, "mimeType")) {
    if (*mime == "video/mp4") {
      return DashTrackKind::kVideo;
    }
    if (*mime == "audio/mp4") {
      return DashTrackKind::kAudio;
    }
  }
  return std::nullopt;
}

const DashTrackPlan* SelectVideo(const std::vector<DashTrackPlan>& tracks,
                                 std::uint64_t maximum_bandwidth) {
  if (tracks.empty()) {
    return nullptr;
  }
  const DashTrackPlan* selected = nullptr;
  for (const DashTrackPlan& track : tracks) {
    if (maximum_bandwidth != 0 && track.bandwidth > maximum_bandwidth) {
      continue;
    }
    if (selected == nullptr || track.bandwidth > selected->bandwidth ||
        (track.bandwidth == selected->bandwidth &&
         track.representation_id < selected->representation_id)) {
      selected = &track;
    }
  }
  if (selected != nullptr) {
    return selected;
  }
  return &*std::min_element(
      tracks.begin(), tracks.end(), [](const auto& left, const auto& right) {
        return left.bandwidth != right.bandwidth
                   ? left.bandwidth < right.bandwidth
                   : left.representation_id < right.representation_id;
      });
}

const DashTrackPlan* SelectAudio(const std::vector<DashTrackPlan>& tracks) {
  if (tracks.empty()) {
    return nullptr;
  }
  return &*std::max_element(
      tracks.begin(), tracks.end(), [](const auto& left, const auto& right) {
        return left.bandwidth != right.bandwidth
                   ? left.bandwidth < right.bandwidth
                   : left.representation_id > right.representation_id;
      });
}

}  // namespace

std::string_view DashVodErrorCode(DashVodError error) {
  switch (error) {
    case DashVodError::kNone:
      return "DASH_OK";
    case DashVodError::kInvalidManifest:
      return "DASH_INVALID_MANIFEST";
    case DashVodError::kUnsupportedManifest:
      return "DASH_UNSUPPORTED_MANIFEST";
    case DashVodError::kUnsafeUri:
      return "DASH_UNSAFE_URI";
    case DashVodError::kLimitExceeded:
      return "DASH_LIMIT_EXCEEDED";
  }
  return "DASH_INVALID_MANIFEST";
}

DashParseResult<DashVodPlan> ParseDashVodManifest(
    std::string_view manifest_uri,
    std::string_view manifest_body,
    std::uint64_t maximum_video_bandwidth) {
  if (!IsSafeHttpDownloadUri(manifest_uri) ||
      manifest_uri.find('#') != std::string_view::npos) {
    return {std::nullopt, DashVodError::kUnsafeUri};
  }
  ClosedXmlParser parser(manifest_body);
  auto root = parser.Parse();
  if (!root.has_value() || LocalName(root->name) != "MPD") {
    return {std::nullopt, DashVodError::kInvalidManifest};
  }
  if (HasXlink(*root) || HasDescendant(*root, "ContentProtection") ||
      HasDescendant(*root, "SegmentBase") ||
      HasDescendant(*root, "SegmentList") ||
      HasDescendant(*root, "EventStream")) {
    return {std::nullopt, DashVodError::kUnsupportedManifest};
  }
  const std::string* type = Attribute(*root, "type");
  if (type != nullptr && *type != "static") {
    return {std::nullopt, DashVodError::kUnsupportedManifest};
  }
  if (Attribute(*root, "minimumUpdatePeriod") != nullptr ||
      Attribute(*root, "timeShiftBufferDepth") != nullptr ||
      Attribute(*root, "availabilityStartTime") != nullptr) {
    return {std::nullopt, DashVodError::kUnsupportedManifest};
  }
  const auto periods = Children(*root, "Period");
  if (periods.size() != 1) {
    return {std::nullopt, DashVodError::kUnsupportedManifest};
  }
  const std::string* duration_value = Attribute(*root, "mediaPresentationDuration");
  if (duration_value == nullptr) {
    duration_value = Attribute(*periods.front(), "duration");
  }
  auto duration = duration_value == nullptr
                      ? std::nullopt
                      : ParseDurationMs(*duration_value);
  if (!duration.has_value()) {
    return {std::nullopt, DashVodError::kInvalidManifest};
  }

  auto mpd_base = ApplyBaseUrl(std::string(manifest_uri), *root);
  auto period_base = mpd_base.has_value()
                         ? ApplyBaseUrl(std::move(*mpd_base), *periods.front())
                         : std::nullopt;
  if (!period_base.has_value()) {
    return {std::nullopt, DashVodError::kUnsafeUri};
  }

  std::vector<DashTrackPlan> videos;
  std::vector<DashTrackPlan> audios;
  std::size_t representation_count = 0;
  for (const XmlNode* adaptation : Children(*periods.front(), "AdaptationSet")) {
    const auto adaptation_kind = AdaptationKind(*adaptation);
    auto adaptation_base = ApplyBaseUrl(*period_base, *adaptation);
    if (!adaptation_base.has_value()) {
      return {std::nullopt, DashVodError::kUnsafeUri};
    }
    const auto representations = Children(*adaptation, "Representation");
    if (representations.empty() ||
        representation_count + representations.size() >
            kMaximumDashRepresentations) {
      return {std::nullopt, DashVodError::kLimitExceeded};
    }
    representation_count += representations.size();
    for (const XmlNode* representation : representations) {
      auto kind = adaptation_kind;
      if (!kind.has_value()) {
        kind = AdaptationKind(*representation);
      }
      if (!kind.has_value()) {
        continue;
      }
      TrackBuildResult built = BuildTrack(
          *adaptation, *representation, *kind, *adaptation_base, *duration);
      if (!built.track.has_value()) {
        return {std::nullopt, built.error};
      }
      (*kind == DashTrackKind::kVideo ? videos : audios)
          .push_back(std::move(*built.track));
    }
  }
  const DashTrackPlan* selected_video =
      SelectVideo(videos, maximum_video_bandwidth);
  if (selected_video == nullptr) {
    return {std::nullopt, DashVodError::kUnsupportedManifest};
  }
  DashVodPlan plan;
  plan.video = *selected_video;
  if (const DashTrackPlan* selected_audio = SelectAudio(audios)) {
    plan.audio = *selected_audio;
  }
  plan.duration_ms = *duration;
  return {std::move(plan), DashVodError::kNone};
}

}  // namespace fireball::transfer
