#include "fireball/components/egress/socks5_probe.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <string>

namespace fireball::egress {
namespace {

class ScopedSocket final {
 public:
  explicit ScopedSocket(int descriptor) : descriptor_(descriptor) {}
  ~ScopedSocket() {
    if (descriptor_ >= 0) {
      close(descriptor_);
    }
  }
  int get() const { return descriptor_; }

 private:
  int descriptor_;
};

bool WaitForWritable(int descriptor,
                     std::chrono::milliseconds timeout,
                     std::string* error) {
  fd_set descriptors;
  FD_ZERO(&descriptors);
  FD_SET(descriptor, &descriptors);
  timeval value{
      static_cast<time_t>(timeout.count() / 1000),
      static_cast<suseconds_t>((timeout.count() % 1000) * 1000),
  };
  const int selected =
      select(descriptor + 1, nullptr, &descriptors, nullptr, &value);
  if (selected <= 0) {
    *error = selected == 0 ? "SOCKS5 probe timed out"
                           : "SOCKS5 probe select failed";
    return false;
  }
  int socket_error = 0;
  socklen_t length = sizeof(socket_error);
  if (getsockopt(descriptor, SOL_SOCKET, SO_ERROR, &socket_error, &length) !=
          0 ||
      socket_error != 0) {
    *error = "SOCKS5 loopback connection failed";
    return false;
  }
  return true;
}

bool SendAll(int descriptor,
             const unsigned char* data,
             std::size_t length) {
  std::size_t sent = 0;
  while (sent < length) {
    const ssize_t count =
        send(descriptor, data + sent, length - sent, MSG_NOSIGNAL);
    if (count < 0 && errno == EINTR) {
      continue;
    }
    if (count <= 0) {
      return false;
    }
    sent += static_cast<std::size_t>(count);
  }
  return true;
}

bool ReceiveAll(int descriptor, unsigned char* data, std::size_t length) {
  std::size_t received = 0;
  while (received < length) {
    const ssize_t count = recv(descriptor, data + received,
                               length - received, 0);
    if (count < 0 && errno == EINTR) {
      continue;
    }
    if (count <= 0) {
      return false;
    }
    received += static_cast<std::size_t>(count);
  }
  return true;
}

}  // namespace

bool ProbeLoopbackSocks5(std::uint16_t port,
                         std::chrono::milliseconds timeout,
                         std::string* error) {
  if (error == nullptr) {
    return false;
  }
  error->clear();
  if (port == 0 || timeout <= std::chrono::milliseconds::zero() ||
      timeout > std::chrono::seconds(30)) {
    *error = "SOCKS5 probe parameters are outside the supported range";
    return false;
  }

  const int descriptor = socket(AF_INET, SOCK_STREAM, 0);
  if (descriptor < 0) {
    *error = "could not create SOCKS5 probe socket";
    return false;
  }
  ScopedSocket socket_owner(descriptor);
  const int existing_flags = fcntl(descriptor, F_GETFL);
  if (existing_flags < 0 ||
      fcntl(descriptor, F_SETFL, existing_flags | O_NONBLOCK) != 0) {
    *error = "could not configure SOCKS5 probe socket";
    return false;
  }

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (connect(descriptor, reinterpret_cast<sockaddr*>(&address),
              sizeof(address)) != 0 &&
      errno != EINPROGRESS) {
    *error = "SOCKS5 loopback connection failed";
    return false;
  }
  if (!WaitForWritable(descriptor, timeout, error) ||
      fcntl(descriptor, F_SETFL, existing_flags) != 0) {
    return false;
  }

  timeval socket_timeout{
      static_cast<time_t>(timeout.count() / 1000),
      static_cast<suseconds_t>((timeout.count() % 1000) * 1000),
  };
  if (setsockopt(descriptor, SOL_SOCKET, SO_SNDTIMEO, &socket_timeout,
                 sizeof(socket_timeout)) != 0 ||
      setsockopt(descriptor, SOL_SOCKET, SO_RCVTIMEO, &socket_timeout,
                 sizeof(socket_timeout)) != 0) {
    *error = "could not set SOCKS5 probe timeout";
    return false;
  }

  constexpr std::array<unsigned char, 3> greeting = {0x05, 0x01, 0x00};
  std::array<unsigned char, 2> response{};
  if (!SendAll(descriptor, greeting.data(), greeting.size()) ||
      !ReceiveAll(descriptor, response.data(), response.size())) {
    *error = "SOCKS5 negotiation failed";
    return false;
  }
  if (response != std::array<unsigned char, 2>{0x05, 0x00}) {
    *error = "loopback endpoint rejected SOCKS5 no-auth negotiation";
    return false;
  }
  return true;
}

}  // namespace fireball::egress
