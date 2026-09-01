#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace fireball::win {

enum class BeamMessageType : uint8_t {
    HANDSHAKE_REQUEST = 0x01,
    HANDSHAKE_RESPONSE = 0x02,
    HEARTBEAT = 0x03,
    VIDEO_FRAME = 0x10,
    TOUCH_DOWN = 0x20,
    TOUCH_MOVE = 0x21,
    TOUCH_UP = 0x22,
    TOUCH_CANCEL = 0x23,
    KEY_DOWN = 0x30,
    KEY_UP = 0x31,
    SCROLL = 0x32,
    DISCONNECT = 0xFF
};

struct BeamPacket {
    BeamMessageType type;
    uint32_t session_id = 0;
    std::vector<uint8_t> payload;

    static std::vector<uint8_t> Encode(const BeamPacket& packet);
    static bool Decode(const std::vector<uint8_t>& raw_bytes, BeamPacket& out_packet);
};

struct NormalizedInput {
    float x = 0.0f; // 0.0 - 1.0
    float y = 0.0f; // 0.0 - 1.0
    int32_t pointer_id = 0;

    std::pair<int32_t, int32_t> ToHostPixels(int32_t host_width, int32_t host_height) const {
        int32_t px = static_cast<int32_t>(x * host_width);
        int32_t py = static_cast<int32_t>(y * host_height);
        return {px, py};
    }
};

} // namespace fireball::win
