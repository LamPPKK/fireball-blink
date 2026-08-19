#include "fireball/components/transfer/aria2_rpc_client.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cctype>
#include <cstring>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace fireball::transfer {
namespace {

constexpr std::size_t kMaximumRpcResponseBytes = 1024 * 1024;
constexpr std::chrono::seconds kRpcTimeout(2);

void SecureErase(std::string* value) {
  if (value == nullptr) {
    return;
  }
  volatile char* bytes = value->empty() ? nullptr : value->data();
  for (std::size_t index = 0; index < value->size(); ++index) {
    bytes[index] = 0;
  }
  value->clear();
}

class ScopedStringErase final {
 public:
  explicit ScopedStringErase(std::string* value) : value_(value) {}
  ~ScopedStringErase() { SecureErase(value_); }

  ScopedStringErase(const ScopedStringErase&) = delete;
  ScopedStringErase& operator=(const ScopedStringErase&) = delete;

 private:
  std::string* value_;
};

class ScopedSocket final {
 public:
  explicit ScopedSocket(int descriptor) : descriptor_(descriptor) {}
  ~ScopedSocket() {
    if (descriptor_ >= 0) {
      close(descriptor_);
    }
  }

  ScopedSocket(const ScopedSocket&) = delete;
  ScopedSocket& operator=(const ScopedSocket&) = delete;

  int get() const { return descriptor_; }

 private:
  int descriptor_;
};

std::string JsonQuote(std::string_view value) {
  constexpr char kHex[] = "0123456789abcdef";
  std::string output;
  output.reserve(value.size() + 2);
  output.push_back('"');
  for (const unsigned char character : value) {
    switch (character) {
      case '"':
        output += "\\\"";
        break;
      case '\\':
        output += "\\\\";
        break;
      case '\b':
        output += "\\b";
        break;
      case '\f':
        output += "\\f";
        break;
      case '\n':
        output += "\\n";
        break;
      case '\r':
        output += "\\r";
        break;
      case '\t':
        output += "\\t";
        break;
      default:
        if (character < 0x20) {
          output += "\\u00";
          output.push_back(kHex[character >> 4]);
          output.push_back(kHex[character & 0x0f]);
        } else {
          output.push_back(static_cast<char>(character));
        }
    }
  }
  output.push_back('"');
  return output;
}

std::optional<std::string> ParseJsonString(std::string_view json,
                                           std::size_t* position) {
  if (*position >= json.size() || json[*position] != '"') {
    return std::nullopt;
  }
  ++*position;
  std::string output;
  while (*position < json.size()) {
    const unsigned char character =
        static_cast<unsigned char>(json[(*position)++]);
    if (character == '"') {
      return output;
    }
    if (character < 0x20) {
      return std::nullopt;
    }
    if (character != '\\') {
      output.push_back(static_cast<char>(character));
      continue;
    }
    if (*position >= json.size()) {
      return std::nullopt;
    }
    const char escaped = json[(*position)++];
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
        if (*position + 4 > json.size()) {
          return std::nullopt;
        }
        unsigned int code_point = 0;
        for (int index = 0; index < 4; ++index) {
          const char digit = json[(*position)++];
          code_point <<= 4;
          if (digit >= '0' && digit <= '9') {
            code_point += static_cast<unsigned int>(digit - '0');
          } else if (digit >= 'a' && digit <= 'f') {
            code_point += static_cast<unsigned int>(digit - 'a' + 10);
          } else if (digit >= 'A' && digit <= 'F') {
            code_point += static_cast<unsigned int>(digit - 'A' + 10);
          } else {
            return std::nullopt;
          }
        }
        if (code_point <= 0x7f) {
          output.push_back(static_cast<char>(code_point));
        } else if (code_point <= 0x7ff) {
          output.push_back(static_cast<char>(0xc0 | (code_point >> 6)));
          output.push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
        } else {
          output.push_back(static_cast<char>(0xe0 | (code_point >> 12)));
          output.push_back(
              static_cast<char>(0x80 | ((code_point >> 6) & 0x3f)));
          output.push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
        }
        break;
      }
      default:
        return std::nullopt;
    }
  }
  return std::nullopt;
}

std::optional<std::string> FindJsonStringField(std::string_view json,
                                               std::string_view field) {
  std::size_t position = 0;
  while (position < json.size()) {
    if (json[position] != '"') {
      ++position;
      continue;
    }
    auto candidate = ParseJsonString(json, &position);
    if (!candidate.has_value()) {
      return std::nullopt;
    }
    std::size_t value_position = position;
    while (value_position < json.size() &&
           std::isspace(static_cast<unsigned char>(json[value_position]))) {
      ++value_position;
    }
    if (*candidate != field || value_position >= json.size() ||
        json[value_position] != ':') {
      continue;
    }
    ++value_position;
    while (value_position < json.size() &&
           std::isspace(static_cast<unsigned char>(json[value_position]))) {
      ++value_position;
    }
    return ParseJsonString(json, &value_position);
  }
  return std::nullopt;
}

bool ContainsJsonObjectField(std::string_view json, std::string_view field) {
  std::size_t position = 0;
  while (position < json.size()) {
    if (json[position] != '"') {
      ++position;
      continue;
    }
    auto candidate = ParseJsonString(json, &position);
    if (!candidate.has_value()) {
      return false;
    }
    std::size_t value_position = position;
    while (value_position < json.size() &&
           std::isspace(static_cast<unsigned char>(json[value_position]))) {
      ++value_position;
    }
    if (*candidate != field || value_position >= json.size() ||
        json[value_position] != ':') {
      continue;
    }
    ++value_position;
    while (value_position < json.size() &&
           std::isspace(static_cast<unsigned char>(json[value_position]))) {
      ++value_position;
    }
    return value_position < json.size() && json[value_position] == '{';
  }
  return false;
}

std::optional<std::uint64_t> ParseDecimal(std::string_view value) {
  if (value.empty()) {
    return std::nullopt;
  }
  std::uint64_t result = 0;
  const auto parsed =
      std::from_chars(value.data(), value.data() + value.size(), result);
  if (parsed.ec != std::errc() || parsed.ptr != value.data() + value.size()) {
    return std::nullopt;
  }
  return result;
}

std::string Base64Encode(std::span<const std::uint8_t> input) {
  constexpr char kAlphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string output;
  output.reserve(((input.size() + 2) / 3) * 4);
  for (std::size_t offset = 0; offset < input.size(); offset += 3) {
    const std::uint32_t first = input[offset];
    const std::uint32_t second =
        offset + 1 < input.size() ? input[offset + 1] : 0;
    const std::uint32_t third =
        offset + 2 < input.size() ? input[offset + 2] : 0;
    const std::uint32_t block = (first << 16) | (second << 8) | third;
    output.push_back(kAlphabet[(block >> 18) & 0x3f]);
    output.push_back(kAlphabet[(block >> 12) & 0x3f]);
    output.push_back(offset + 1 < input.size() ? kAlphabet[(block >> 6) & 0x3f]
                                               : '=');
    output.push_back(offset + 2 < input.size() ? kAlphabet[block & 0x3f] : '=');
  }
  return output;
}

std::string SocketError(std::string_view action) {
  return std::string(action) + " failed: " + std::strerror(errno);
}

Aria2RpcResult<std::string> PostToLoopback(std::uint16_t port,
                                          std::string_view request_body) {
  ScopedSocket socket_descriptor(socket(AF_INET, SOCK_STREAM, 0));
  if (socket_descriptor.get() < 0) {
    return {std::nullopt, SocketError("socket")};
  }

  timeval timeout{};
  timeout.tv_sec = kRpcTimeout.count();
  if (setsockopt(socket_descriptor.get(), SOL_SOCKET, SO_RCVTIMEO, &timeout,
                 sizeof(timeout)) != 0 ||
      setsockopt(socket_descriptor.get(), SOL_SOCKET, SO_SNDTIMEO, &timeout,
                 sizeof(timeout)) != 0) {
    return {std::nullopt, SocketError("setsockopt")};
  }
#if defined(__APPLE__)
  int no_sigpipe = 1;
  if (setsockopt(socket_descriptor.get(), SOL_SOCKET, SO_NOSIGPIPE, &no_sigpipe,
                 sizeof(no_sigpipe)) != 0) {
    return {std::nullopt, SocketError("setsockopt")};
  }
#endif

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (connect(socket_descriptor.get(), reinterpret_cast<sockaddr*>(&address),
              sizeof(address)) != 0) {
    return {std::nullopt, SocketError("connect")};
  }

  std::string request =
      "POST /jsonrpc HTTP/1.1\r\nHost: 127.0.0.1:" +
      std::to_string(port) +
      "\r\nContent-Type: application/json\r\nAccept: application/json\r\n"
      "Connection: close\r\nContent-Length: " +
      std::to_string(request_body.size()) + "\r\n\r\n" +
      std::string(request_body);
  ScopedStringErase erase_request(&request);

  std::size_t sent = 0;
  while (sent < request.size()) {
#if defined(MSG_NOSIGNAL)
    constexpr int kSendFlags = MSG_NOSIGNAL;
#else
    constexpr int kSendFlags = 0;
#endif
    const ssize_t count =
        send(socket_descriptor.get(), request.data() + sent,
             request.size() - sent, kSendFlags);
    if (count <= 0) {
      return {std::nullopt, SocketError("send")};
    }
    sent += static_cast<std::size_t>(count);
  }

  std::string response;
  std::array<char, 8192> buffer{};
  while (true) {
    const ssize_t count =
        recv(socket_descriptor.get(), buffer.data(), buffer.size(), 0);
    if (count == 0) {
      break;
    }
    if (count < 0) {
      return {std::nullopt, SocketError("recv")};
    }
    if (response.size() + static_cast<std::size_t>(count) >
        kMaximumRpcResponseBytes) {
      return {std::nullopt, "RPC response exceeded the 1 MiB limit"};
    }
    response.append(buffer.data(), static_cast<std::size_t>(count));
  }

  const std::size_t header_end = response.find("\r\n\r\n");
  if (header_end == std::string::npos) {
    return {std::nullopt, "RPC response did not contain complete HTTP headers"};
  }
  const std::size_t status_end = response.find("\r\n");
  const std::string_view status_line(response.data(), status_end);
  if (status_line != "HTTP/1.1 200 OK" && status_line != "HTTP/1.0 200 OK") {
    return {std::nullopt, "aria2 RPC returned a non-200 HTTP status"};
  }
  return {response.substr(header_end + 4), {}};
}

Aria2TransferState ParseState(std::string_view state) {
  if (state == "waiting") {
    return Aria2TransferState::kWaiting;
  }
  if (state == "paused") {
    return Aria2TransferState::kPaused;
  }
  if (state == "active") {
    return Aria2TransferState::kActive;
  }
  if (state == "complete") {
    return Aria2TransferState::kComplete;
  }
  if (state == "error") {
    return Aria2TransferState::kError;
  }
  if (state == "removed") {
    return Aria2TransferState::kRemoved;
  }
  return Aria2TransferState::kUnknown;
}

}  // namespace

bool IsValidAria2Secret(std::string_view secret) {
  return secret.size() == 64 &&
         std::all_of(secret.begin(), secret.end(), [](char character) {
           return (character >= '0' && character <= '9') ||
                  (character >= 'a' && character <= 'f');
         });
}

bool IsValidAria2Gid(std::string_view gid) {
  return gid.size() == 16 &&
         std::all_of(gid.begin(), gid.end(), [](char character) {
           return (character >= '0' && character <= '9') ||
                  (character >= 'a' && character <= 'f');
         });
}

Aria2RpcClient::Aria2RpcClient(std::uint16_t port,
                               std::string secret,
                               TransferPersistence persistence,
                               bool allow_peer_to_peer)
    : port_(port),
      secret_(std::move(secret)),
      persistence_(persistence),
      allow_peer_to_peer_(allow_peer_to_peer) {}

Aria2RpcClient::~Aria2RpcClient() {
  volatile char* bytes = secret_.empty() ? nullptr : secret_.data();
  for (std::size_t index = 0; index < secret_.size(); ++index) {
    bytes[index] = 0;
  }
}

bool Aria2RpcClient::IsConfigurationValid() const {
  return port_ != 0 && IsValidAria2Secret(secret_);
}

Aria2RpcResult<std::string> Aria2RpcClient::Call(
    std::string_view method,
    std::string params_json) {
  ScopedStringErase erase_params(&params_json);
  if (!IsConfigurationValid()) {
    return {std::nullopt, "invalid loopback RPC configuration"};
  }
  const std::string request_id = "fireball-" + std::to_string(next_request_id_++);
  std::string body =
      "{\"jsonrpc\":\"2.0\",\"id\":" + JsonQuote(request_id) +
      ",\"method\":" + JsonQuote(method) + ",\"params\":" +
      params_json + "}";
  ScopedStringErase erase_body(&body);
  auto response = PostToLoopback(port_, body);
  if (!response.ok()) {
    return response;
  }
  const auto response_id = FindJsonStringField(*response.value, "id");
  if (!response_id.has_value() || *response_id != request_id) {
    return {std::nullopt, "aria2 RPC response id mismatch"};
  }
  if (ContainsJsonObjectField(*response.value, "error")) {
    const std::string message =
        FindJsonStringField(*response.value, "message")
            .value_or("aria2 RPC returned an error");
    return {std::nullopt, message.substr(0, 512)};
  }
  return response;
}

Aria2RpcResult<std::string> Aria2RpcClient::CallForString(
    std::string_view method,
    std::string params_json) {
  ScopedStringErase erase_params(&params_json);
  auto response = Call(method, params_json);
  if (!response.ok()) {
    return response;
  }
  auto result = FindJsonStringField(*response.value, "result");
  if (!result.has_value()) {
    return {std::nullopt, "aria2 RPC result was not a string"};
  }
  return {std::move(result), {}};
}

Aria2RpcResult<std::string> Aria2RpcClient::GetVersion() {
  auto response = Call("aria2.getVersion", "[" + JsonQuote("token:" + secret_) + "]");
  if (!response.ok()) {
    return response;
  }
  auto version = FindJsonStringField(*response.value, "version");
  if (!version.has_value() || version->empty()) {
    return {std::nullopt, "aria2 RPC version result was malformed"};
  }
  return {std::move(version), {}};
}

Aria2RpcResult<std::string> Aria2RpcClient::Enqueue(
    const TransferRequest& request) {
  if (request.persistence != persistence_) {
    return {std::nullopt,
            "transfer request does not match the sidecar storage boundary"};
  }
  if (!allow_peer_to_peer_ &&
      (request.source_kind == TransferSourceKind::kMagnet ||
       request.source_kind == TransferSourceKind::kTorrentMetainfo)) {
    return {std::nullopt,
            "peer-to-peer transfers are disabled for the active egress route"};
  }
  if (!IsValidTransferRequest(request)) {
    return {std::nullopt, "invalid transfer request"};
  }
  // aria2 forwards custom Authorization/Cookie headers across cross-origin
  // redirects. Its RPC API has no per-request redirect prohibition, so a
  // credential-bearing request must use Chromium's origin-aware network
  // backend instead of this sidecar.
  if (!request.request_headers.empty()) {
    return {std::nullopt,
            "credential headers require the origin-pinned browser backend"};
  }
  std::string options = "{";
  ScopedStringErase erase_options(&options);
  if (request.output_name.has_value()) {
    options += "\"out\":" + JsonQuote(*request.output_name) +
               ",\"auto-file-renaming\":\"" +
               (request.allow_automatic_renaming ? "true" : "false") + "\"";
  }
  options.push_back('}');
  std::string plain_token = "token:";
  plain_token += secret_;
  ScopedStringErase erase_plain_token(&plain_token);
  std::string token = JsonQuote(plain_token);
  ScopedStringErase erase_token(&token);
  Aria2RpcResult<std::string> result;
  if (request.source_kind == TransferSourceKind::kHttp ||
      request.source_kind == TransferSourceKind::kMagnet) {
    std::string quoted_source = JsonQuote(request.source);
    ScopedStringErase erase_quoted_source(&quoted_source);
    std::string params =
        "[" + token + ",[" + quoted_source + "]," + options + "]";
    ScopedStringErase erase_params(&params);
    result = CallForString("aria2.addUri", params);
  } else if (request.source_kind == TransferSourceKind::kTorrentMetainfo) {
    std::string encoded_metainfo = Base64Encode(request.torrent_metainfo);
    ScopedStringErase erase_metainfo(&encoded_metainfo);
    std::string quoted_metainfo = JsonQuote(encoded_metainfo);
    ScopedStringErase erase_quoted_metainfo(&quoted_metainfo);
    std::string params = "[" + token + "," + quoted_metainfo + ",[]," +
                         options + "]";
    ScopedStringErase erase_params(&params);
    result = CallForString("aria2.addTorrent", params);
  } else {
    return {std::nullopt, "unsupported transfer source"};
  }
  if (result.ok() && !IsValidAria2Gid(*result.value)) {
    return {std::nullopt, "aria2 returned an invalid GID"};
  }
  return result;
}

Aria2RpcResult<Aria2TransferStatus> Aria2RpcClient::TellStatus(
    std::string_view gid) {
  if (!IsValidAria2Gid(gid)) {
    return {std::nullopt, "invalid aria2 GID"};
  }
  const std::string keys =
      "[\"gid\",\"status\",\"totalLength\",\"completedLength\","
      "\"downloadSpeed\",\"errorCode\",\"errorMessage\"]";
  auto response = Call("aria2.tellStatus",
                       "[" + JsonQuote("token:" + secret_) + "," +
                           JsonQuote(gid) + "," + keys + "]");
  if (!response.ok()) {
    return {std::nullopt, std::move(response.error)};
  }
  const auto parsed_gid = FindJsonStringField(*response.value, "gid");
  const auto state = FindJsonStringField(*response.value, "status");
  const auto total = FindJsonStringField(*response.value, "totalLength");
  const auto completed =
      FindJsonStringField(*response.value, "completedLength");
  const auto speed = FindJsonStringField(*response.value, "downloadSpeed");
  if (!parsed_gid.has_value() || *parsed_gid != gid || !state.has_value() ||
      !total.has_value() || !completed.has_value() || !speed.has_value()) {
    return {std::nullopt, "aria2 status result was malformed"};
  }
  const auto total_bytes = ParseDecimal(*total);
  const auto completed_bytes = ParseDecimal(*completed);
  const auto bytes_per_second = ParseDecimal(*speed);
  if (!total_bytes.has_value() || !completed_bytes.has_value() ||
      !bytes_per_second.has_value()) {
    return {std::nullopt, "aria2 status contained an invalid byte count"};
  }

  Aria2TransferStatus status;
  status.gid = *parsed_gid;
  status.state = ParseState(*state);
  status.total_bytes = *total_bytes;
  status.completed_bytes = *completed_bytes;
  status.bytes_per_second = *bytes_per_second;
  status.error_code =
      FindJsonStringField(*response.value, "errorCode").value_or("");
  status.error_message =
      FindJsonStringField(*response.value, "errorMessage").value_or("");
  if (status.error_message.size() > 512) {
    status.error_message.resize(512);
  }
  return {std::move(status), {}};
}

Aria2RpcResult<std::string> Aria2RpcClient::Pause(std::string_view gid) {
  if (!IsValidAria2Gid(gid)) {
    return {std::nullopt, "invalid aria2 GID"};
  }
  return CallForString("aria2.pause", "[" + JsonQuote("token:" + secret_) +
                                          "," + JsonQuote(gid) + "]");
}

Aria2RpcResult<std::string> Aria2RpcClient::Unpause(std::string_view gid) {
  if (!IsValidAria2Gid(gid)) {
    return {std::nullopt, "invalid aria2 GID"};
  }
  return CallForString("aria2.unpause", "[" + JsonQuote("token:" + secret_) +
                                            "," + JsonQuote(gid) + "]");
}

Aria2RpcResult<std::string> Aria2RpcClient::Remove(std::string_view gid) {
  if (!IsValidAria2Gid(gid)) {
    return {std::nullopt, "invalid aria2 GID"};
  }
  return CallForString("aria2.forceRemove",
                       "[" + JsonQuote("token:" + secret_) + "," +
                           JsonQuote(gid) + "]");
}

Aria2RpcResult<std::string> Aria2RpcClient::ForgetDownloadResult(
    std::string_view gid) {
  if (!IsValidAria2Gid(gid)) {
    return {std::nullopt, "invalid aria2 GID"};
  }
  return CallForString("aria2.removeDownloadResult",
                       "[" + JsonQuote("token:" + secret_) + "," +
                           JsonQuote(gid) + "]");
}

Aria2RpcResult<std::string> Aria2RpcClient::ForceShutdown() {
  return CallForString("aria2.forceShutdown",
                       "[" + JsonQuote("token:" + secret_) + "]");
}

}  // namespace fireball::transfer
