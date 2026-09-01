#include "fireball/win/beam_client.h"
#include <cstring>

namespace fireball::win {

namespace {
const uint8_t kMagicHeader[4] = {'F', 'B', 'E', 'A'}; // FBEAM binary frame
}

std::vector<uint8_t> BeamPacket::Encode(const BeamPacket& packet) {
    std::vector<uint8_t> bytes;
    // Magic: 4 bytes
    bytes.insert(bytes.end(), kMagicHeader, kMagicHeader + 4);
    // Type: 1 byte
    bytes.push_back(static_cast<uint8_t>(packet.type));
    // Session ID: 4 bytes (Big Endian)
    bytes.push_back(static_cast<uint8_t>((packet.session_id >> 24) & 0xFF));
    bytes.push_back(static_cast<uint8_t>((packet.session_id >> 16) & 0xFF));
    bytes.push_back(static_cast<uint8_t>((packet.session_id >> 8) & 0xFF));
    bytes.push_back(static_cast<uint8_t>(packet.session_id & 0xFF));
    // Payload length: 4 bytes (Big Endian)
    uint32_t len = static_cast<uint32_t>(packet.payload.size());
    bytes.push_back(static_cast<uint8_t>((len >> 24) & 0xFF));
    bytes.push_back(static_cast<uint8_t>((len >> 16) & 0xFF));
    bytes.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
    bytes.push_back(static_cast<uint8_t>(len & 0xFF));
    // Payload data
    bytes.insert(bytes.end(), packet.payload.begin(), packet.payload.end());
    return bytes;
}

bool BeamPacket::Decode(const std::vector<uint8_t>& raw_bytes, BeamPacket& out_packet) {
    if (raw_bytes.size() < 13) return false;
    if (std::memcmp(raw_bytes.data(), kMagicHeader, 4) != 0) return false;

    out_packet.type = static_cast<BeamMessageType>(raw_bytes[4]);
    out_packet.session_id = (static_cast<uint32_t>(raw_bytes[5]) << 24) |
                            (static_cast<uint32_t>(raw_bytes[6]) << 16) |
                            (static_cast<uint32_t>(raw_bytes[7]) << 8) |
                            static_cast<uint32_t>(raw_bytes[8]);

    uint32_t len = (static_cast<uint32_t>(raw_bytes[9]) << 24) |
                   (static_cast<uint32_t>(raw_bytes[10]) << 16) |
                   (static_cast<uint32_t>(raw_bytes[11]) << 8) |
                   static_cast<uint32_t>(raw_bytes[12]);

    if (raw_bytes.size() < 13 + len) return false;

    out_packet.payload.assign(raw_bytes.begin() + 13, raw_bytes.begin() + 13 + len);
    return true;
}

} // namespace fireball::win
