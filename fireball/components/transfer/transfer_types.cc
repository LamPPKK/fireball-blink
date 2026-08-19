#include "fireball/components/transfer/transfer_types.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <string>
#include <utility>

namespace fireball::transfer {
namespace {

bool IsAsciiControlOrSpace(unsigned char value) {
  return value <= 0x20 || value == 0x7f;
}

bool HasUnsafeUriByte(std::string_view value) {
  return std::any_of(value.begin(), value.end(), [](char character) {
    return IsAsciiControlOrSpace(static_cast<unsigned char>(character));
  });
}

bool IsHex(char character) {
  return (character >= '0' && character <= '9') ||
         (character >= 'a' && character <= 'f') ||
         (character >= 'A' && character <= 'F');
}

bool IsBase32(char character) {
  const char upper = static_cast<char>(
      std::toupper(static_cast<unsigned char>(character)));
  return (upper >= 'A' && upper <= 'Z') || (upper >= '2' && upper <= '7');
}

bool IsValidBtih(std::string_view value) {
  if (value.size() == 40) {
    return std::all_of(value.begin(), value.end(), IsHex);
  }
  if (value.size() == 32) {
    return std::all_of(value.begin(), value.end(), IsBase32);
  }
  return false;
}

std::string_view WithoutQueryOrFragment(std::string_view uri) {
  const std::size_t boundary = uri.find_first_of("?#");
  return uri.substr(0, boundary);
}

std::string LowerAscii(std::string_view value) {
  std::string lowered(value);
  std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](char value) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
  });
  return lowered;
}

bool EndsWith(std::string_view value, std::string_view suffix) {
  return value.size() >= suffix.size() &&
         value.substr(value.size() - suffix.size()) == suffix;
}

bool IsSafeHeaderValue(std::string_view value) {
  return !value.empty() && value.size() <= kMaximumTransferHeaderValueBytes &&
         value.front() != ' ' && value.back() != ' ' &&
         std::all_of(value.begin(), value.end(), [](char character) {
           const unsigned char byte = static_cast<unsigned char>(character);
           return byte >= 0x20 && byte <= 0x7e;
         });
}

bool IsOrigin(std::string_view value) {
  if (!IsSafeHttpDownloadUri(value)) {
    return false;
  }
  const std::size_t authority_start = value.starts_with("https://") ? 8 : 7;
  return value.find_first_of("/?#", authority_start) == std::string_view::npos;
}

}  // namespace

SensitiveHeaderValue::SensitiveHeaderValue() = default;

SensitiveHeaderValue::SensitiveHeaderValue(std::string value)
    : value_(std::move(value)) {}

SensitiveHeaderValue::~SensitiveHeaderValue() {
  Erase();
}

SensitiveHeaderValue::SensitiveHeaderValue(const SensitiveHeaderValue& other)
    : value_(other.value_) {}

SensitiveHeaderValue& SensitiveHeaderValue::operator=(
    const SensitiveHeaderValue& other) {
  if (this != &other) {
    Erase();
    value_ = other.value_;
  }
  return *this;
}

SensitiveHeaderValue::SensitiveHeaderValue(SensitiveHeaderValue&& other)
    : value_(other.value_) {
  other.Erase();
}

SensitiveHeaderValue& SensitiveHeaderValue::operator=(
    SensitiveHeaderValue&& other) {
  if (this != &other) {
    Erase();
    value_ = other.value_;
    other.Erase();
  }
  return *this;
}

void SensitiveHeaderValue::Erase() {
  volatile char* bytes = value_.empty() ? nullptr : value_.data();
  for (std::size_t index = 0; index < value_.size(); ++index) {
    bytes[index] = '\0';
  }
  value_.clear();
}

TransferRequestHeader::TransferRequestHeader(TransferRequestHeaderKind kind,
                                             std::string value)
    : kind(kind), value(std::move(value)) {}

bool IsSafeHttpDownloadUri(std::string_view uri) {
  if (uri.empty() || uri.size() > kMaximumUriBytes || HasUnsafeUriByte(uri)) {
    return false;
  }

  std::size_t authority_start = 0;
  if (uri.starts_with("https://")) {
    authority_start = 8;
  } else if (uri.starts_with("http://")) {
    authority_start = 7;
  } else {
    return false;
  }

  const std::size_t authority_end = uri.find_first_of("/?#", authority_start);
  const std::string_view authority = uri.substr(
      authority_start, authority_end == std::string_view::npos
                           ? std::string_view::npos
                           : authority_end - authority_start);
  if (authority.empty() || authority.find('@') != std::string_view::npos ||
      authority.find('\\') != std::string_view::npos) {
    return false;
  }

  std::string_view host;
  std::string_view port;
  const bool bracketed_host = authority.front() == '[';
  if (bracketed_host) {
    const std::size_t closing = authority.find(']');
    if (closing == std::string_view::npos || closing == 1 ||
        (closing + 1 < authority.size() && authority[closing + 1] != ':')) {
      return false;
    }
    host = authority.substr(1, closing - 1);
    if (!std::all_of(host.begin(), host.end(), [](char character) {
          return IsHex(character) || character == ':' || character == '.';
        })) {
      return false;
    }
    if (closing + 1 < authority.size()) {
      port = authority.substr(closing + 2);
    }
  } else if (authority.front() == '.' || authority.back() == '.' ||
             authority.front() == ':' || authority.find("..") != std::string_view::npos) {
    return false;
  } else {
    const std::size_t colon = authority.rfind(':');
    if (colon != std::string_view::npos) {
      if (authority.find(':') != colon || colon == 0) {
        return false;
      }
      host = authority.substr(0, colon);
      port = authority.substr(colon + 1);
    } else {
      host = authority;
    }
    if (!std::all_of(host.begin(), host.end(), [](char character) {
          return std::isalnum(static_cast<unsigned char>(character)) ||
                 character == '.' || character == '-';
        })) {
      return false;
    }
  }
  if (!bracketed_host &&
      (host.empty() || host.front() == '.' || host.back() == '.' ||
       host.find("..") != std::string_view::npos)) {
    return false;
  }
  if (!port.empty()) {
    unsigned int numeric_port = 0;
    for (char digit : port) {
      if (digit < '0' || digit > '9') {
        return false;
      }
      numeric_port = numeric_port * 10 + static_cast<unsigned int>(digit - '0');
      if (numeric_port > 65535) {
        return false;
      }
    }
    if (numeric_port == 0) {
      return false;
    }
  } else if (authority.back() == ':') {
    return false;
  }
  return true;
}

bool IsSafeMagnetUri(std::string_view uri) {
  if (!uri.starts_with("magnet:?") || uri.size() > kMaximumUriBytes ||
      HasUnsafeUriByte(uri) || uri.find('#') != std::string_view::npos) {
    return false;
  }

  bool found_btih = false;
  std::string_view query = uri.substr(8);
  while (!query.empty()) {
    const std::size_t separator = query.find('&');
    const std::string_view parameter = query.substr(0, separator);
    const std::size_t equals = parameter.find('=');
    const std::string_view name = parameter.substr(0, equals);
    const std::string_view value =
        equals == std::string_view::npos ? std::string_view()
                                         : parameter.substr(equals + 1);
    if (name == "xt") {
      constexpr std::string_view kBtihPrefix = "urn:btih:";
      if (found_btih || !value.starts_with(kBtihPrefix) ||
          !IsValidBtih(value.substr(kBtihPrefix.size()))) {
        return false;
      }
      found_btih = true;
    }
    if (separator == std::string_view::npos) {
      break;
    }
    query.remove_prefix(separator + 1);
  }
  return found_btih;
}

bool IsPlausibleTorrentMetainfo(std::span<const std::uint8_t> metainfo) {
  if (metainfo.size() < 4 || metainfo.size() > kMaximumTorrentBytes ||
      metainfo.front() != static_cast<std::uint8_t>('d') ||
      metainfo.back() != static_cast<std::uint8_t>('e')) {
    return false;
  }

  constexpr std::string_view kInfoMarker = "4:info";
  const auto marker = std::search(
      metainfo.begin(), metainfo.end(), kInfoMarker.begin(), kInfoMarker.end(),
      [](std::uint8_t left, char right) {
        return left == static_cast<std::uint8_t>(right);
      });
  return marker != metainfo.end();
}

bool IsSafeOutputName(std::string_view name) {
  if (name.empty() || name.size() > 240 || name == "." || name == ".." ||
      name.front() == ' ' || name.back() == ' ') {
    return false;
  }
  return std::none_of(name.begin(), name.end(), [](char character) {
    const unsigned char value = static_cast<unsigned char>(character);
    return value < 0x20 || value == 0x7f || character == '/' ||
           character == '\\' || character == ':';
  });
}

bool IsCanonicalTransferId(std::string_view id) {
  if (id.size() != 36 || id == "00000000-0000-0000-0000-000000000000") {
    return false;
  }
  for (std::size_t index = 0; index < id.size(); ++index) {
    const bool separator =
        index == 8 || index == 13 || index == 18 || index == 23;
    if (separator) {
      if (id[index] != '-') {
        return false;
      }
      continue;
    }
    const char character = id[index];
    if (!((character >= '0' && character <= '9') ||
          (character >= 'a' && character <= 'f'))) {
      return false;
    }
  }
  return true;
}

std::string_view TransferRequestHeaderName(TransferRequestHeaderKind kind) {
  switch (kind) {
    case TransferRequestHeaderKind::kAuthorization:
      return "Authorization";
    case TransferRequestHeaderKind::kCookie:
      return "Cookie";
    case TransferRequestHeaderKind::kOrigin:
      return "Origin";
    case TransferRequestHeaderKind::kReferer:
      return "Referer";
    case TransferRequestHeaderKind::kUserAgent:
      return "User-Agent";
  }
  return {};
}

bool IsValidTransferRequestHeaders(
    std::span<const TransferRequestHeader> headers) {
  if (headers.size() > kMaximumTransferRequestHeaders) {
    return false;
  }
  std::size_t total = 0;
  std::optional<TransferRequestHeaderKind> previous;
  for (const TransferRequestHeader& header : headers) {
    const std::string_view name = TransferRequestHeaderName(header.kind);
    const std::string_view value = header.value.view();
    if (name.empty() || !IsSafeHeaderValue(value) ||
        (previous.has_value() && *previous >= header.kind) ||
        value.size() > std::numeric_limits<std::size_t>::max() - name.size() -
                           2 ||
        total > kMaximumTransferHeaderBytes - name.size() - 2 - value.size()) {
      return false;
    }
    if (header.kind == TransferRequestHeaderKind::kOrigin &&
        !IsOrigin(value)) {
      return false;
    }
    if (header.kind == TransferRequestHeaderKind::kReferer &&
        !IsSafeHttpDownloadUri(value)) {
      return false;
    }
    total += name.size() + 2 + value.size();
    previous = header.kind;
  }
  return true;
}

bool IsValidTransferRequest(const TransferRequest& request) {
  if (request.output_name.has_value() &&
      !IsSafeOutputName(*request.output_name)) {
    return false;
  }
  switch (request.source_kind) {
    case TransferSourceKind::kHttp:
      return IsSafeHttpDownloadUri(request.source) &&
             request.torrent_metainfo.empty() &&
             IsValidTransferRequestHeaders(request.request_headers);
    case TransferSourceKind::kMagnet:
      return IsSafeMagnetUri(request.source) &&
             request.torrent_metainfo.empty() &&
             request.request_headers.empty();
    case TransferSourceKind::kTorrentMetainfo:
      return request.source.empty() &&
             IsPlausibleTorrentMetainfo(request.torrent_metainfo) &&
             request.request_headers.empty();
  }
  return false;
}

std::optional<TransferRequest> MakeUriTransferRequest(
    std::string uri,
    TransferPersistence persistence,
    std::optional<std::string> output_name,
    std::vector<TransferRequestHeader> request_headers) {
  TransferSourceKind kind;
  if (IsSafeHttpDownloadUri(uri)) {
    kind = TransferSourceKind::kHttp;
  } else if (IsSafeMagnetUri(uri)) {
    kind = TransferSourceKind::kMagnet;
  } else {
    return std::nullopt;
  }
  if (output_name.has_value() && !IsSafeOutputName(*output_name)) {
    return std::nullopt;
  }
  if (kind != TransferSourceKind::kHttp && !request_headers.empty()) {
    return std::nullopt;
  }
  TransferRequest request{kind, persistence, std::move(uri),
                          std::move(output_name), {}, true,
                          std::move(request_headers)};
  return IsValidTransferRequest(request)
             ? std::optional<TransferRequest>(std::move(request))
             : std::nullopt;
}

std::optional<TransferRequest> MakeTorrentTransferRequest(
    std::vector<std::uint8_t> metainfo,
    TransferPersistence persistence,
    std::optional<std::string> output_name) {
  if (!IsPlausibleTorrentMetainfo(metainfo) ||
      (output_name.has_value() && !IsSafeOutputName(*output_name))) {
    return std::nullopt;
  }
  TransferRequest request{TransferSourceKind::kTorrentMetainfo, persistence,
                          {}, std::move(output_name), std::move(metainfo), true,
                          {}};
  return IsValidTransferRequest(request)
             ? std::optional<TransferRequest>(std::move(request))
             : std::nullopt;
}

MediaCandidateKind ClassifyMediaCandidate(std::string_view uri,
                                          std::string_view mime_type) {
  if (!IsSafeHttpDownloadUri(uri)) {
    return MediaCandidateKind::kNone;
  }
  const std::string mime = LowerAscii(mime_type.substr(0, mime_type.find(';')));
  const std::string path = LowerAscii(WithoutQueryOrFragment(uri));
  if (mime == "application/vnd.apple.mpegurl" || mime == "application/x-mpegurl" ||
      EndsWith(path, ".m3u8")) {
    return MediaCandidateKind::kHlsManifest;
  }
  if (mime == "application/dash+xml" || EndsWith(path, ".mpd")) {
    return MediaCandidateKind::kDashManifest;
  }
  if (mime.starts_with("video/") || EndsWith(path, ".mp4") ||
      EndsWith(path, ".webm") || EndsWith(path, ".mkv")) {
    return MediaCandidateKind::kDirectVideo;
  }
  if (mime.starts_with("audio/") || EndsWith(path, ".mp3") ||
      EndsWith(path, ".m4a") || EndsWith(path, ".flac") ||
      EndsWith(path, ".ogg")) {
    return MediaCandidateKind::kDirectAudio;
  }
  return MediaCandidateKind::kNone;
}

}  // namespace fireball::transfer
