#include "fireball/components/egress/tor_config.h"

#include <cstdint>
#include <filesystem>
#include <string>

namespace fireball::egress {
namespace {

bool IsSafeTorPath(const std::filesystem::path& path) {
  if (!path.is_absolute()) {
    return false;
  }
  const std::string value = path.string();
  return !value.empty() && value.size() <= 1024 &&
         value.find("..") == std::string::npos &&
         value.find_first_not_of(
             "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/._-") ==
             std::string::npos;
}

}  // namespace

std::optional<std::string> BuildTorConfiguration(
    const std::filesystem::path& data_directory,
    std::uint16_t socks5_port,
    std::uint16_t http_connect_port,
    std::string* error) {
  if (error == nullptr) {
    return std::nullopt;
  }
  error->clear();
  if (!IsSafeTorPath(data_directory) || socks5_port == 0 ||
      http_connect_port == 0 || socks5_port == http_connect_port) {
    *error = "Tor data path or proxy ports are invalid";
    return std::nullopt;
  }

  return "ClientOnly 1\n"
         "RunAsDaemon 0\n"
         "AvoidDiskWrites 1\n"
         "SafeSocks 1\n"
         "TestSocks 1\n"
         "WarnUnsafeSocks 1\n"
         "DNSPort 0\n"
         "ControlPort 0\n"
         "DataDirectory " +
         data_directory.string() + "\n" + "SocksPort 127.0.0.1:" +
         std::to_string(socks5_port) +
         " IsolateClientAddr IsolateClientProtocol IPv6Traffic\n" +
         "HTTPTunnelPort 127.0.0.1:" + std::to_string(http_connect_port) +
         " IsolateClientAddr IsolateClientProtocol IPv6Traffic\n" +
         "SocksPolicy accept 127.0.0.1\n"
         "SocksPolicy reject *\n"
         "Log notice stdout\n";
}

}  // namespace fireball::egress
