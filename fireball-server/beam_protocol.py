#!/usr/bin/env python3
"""
Fireball Beam Protocol Engine
Defines binary & JSON framing, touch/mouse normalization, and crypto pairing
inspired by seg6/surf for the Fireball remote browser streaming engine.
"""

from __future__ import annotations

import base64
import hashlib
import hmac
import json
import secrets
import struct
import time
from dataclasses import asdict, dataclass, field
from enum import IntEnum, unique
from typing import Any, Dict, List, Optional, Tuple


@unique
class BeamMessageType(IntEnum):
    # Control Handshake (0x01 - 0x0F)
    PAIRING_REQUEST = 0x01
    PAIRING_CHALLENGE = 0x02
    PAIRING_VERIFY = 0x03
    PAIRING_SUCCESS = 0x04
    SESSION_START = 0x05
    SESSION_ACK = 0x06
    SESSION_HEARTBEAT = 0x07
    SESSION_CLOSE = 0x08

    # Downstream Stream & Tab State (0x10 - 0x2F)
    FRAME_VIDEO_H264 = 0x10
    FRAME_AUDIO_OPUS = 0x11
    TAB_STATE_UPDATE = 0x12
    NAVIGATION_COMMITTED = 0x13
    FULLSCREEN_CHANGE = 0x14
    SECURITY_INFO = 0x15

    # Upstream Client Input (0x30 - 0x4F)
    INPUT_TOUCH_START = 0x30
    INPUT_TOUCH_MOVE = 0x31
    INPUT_TOUCH_END = 0x32
    INPUT_TOUCH_CANCEL = 0x33
    INPUT_KEY_EVENT = 0x34
    INPUT_SCROLL = 0x35
    NAVIGATE_URL = 0x36
    TAB_COMMAND = 0x37


# 6-word mnemonic verification wordlist (subset of BIP-39)
BEAM_WORDLIST = [
    "fireball", "orbital", "meteor", "plasma", "shield", "beacon",
    "matrix", "vector", "signal", "quantum", "stellar", "engine",
    "cyber", "tunnel", "crypto", "cipher", "vertex", "horizon",
    "pulsar", "nebula", "photon", "flux", "aurora", "comet"
]


def generate_six_word_phrase(seed_bytes: bytes) -> List[str]:
    """Generates a deterministic 6-word visual confirmation phrase from a seed."""
    digest = hashlib.sha256(seed_bytes).digest()
    words: List[str] = []
    for i in range(6):
        idx = digest[i] % len(BEAM_WORDLIST)
        words.append(BEAM_WORDLIST[idx])
    return words


@dataclass
class NormalizedTouch:
    """Represents a touch point normalized to 0.0 - 1.0 coordinates."""
    id: int
    norm_x: float  # 0.0 to 1.0 (relative to viewport width)
    norm_y: float  # 0.0 to 1.0 (relative to viewport height)
    pressure: float = 1.0

    def to_pixel_coords(self, viewport_width: int, viewport_height: int) -> Tuple[int, int]:
        px = int(round(self.norm_x * viewport_width))
        py = int(round(self.norm_y * viewport_height))
        return (max(0, min(viewport_width - 1, px)), max(0, min(viewport_height - 1, py)))


@dataclass
class BeamTabState:
    tab_id: str
    url: str
    title: str
    can_go_back: bool = False
    can_go_forward: bool = False
    is_loading: bool = False
    is_secure_https: bool = True
    blocked_trackers_count: int = 0


class BeamPacketCodec:
    """Encodes and decodes framed binary Beam protocol packets."""

    MAGIC = b"FBEAM"
    VERSION = 1

    @classmethod
    def encode_json_packet(cls, msg_type: BeamMessageType, payload: Dict[str, Any]) -> bytes:
        raw_json = json.dumps(payload, separators=(",", ":")).encode("utf-8")
        # Format: MAGIC (5B) + VERSION (1B) + TYPE (1B) + LENGTH (4B Big-Endian) + JSON Payload
        header = struct.pack(">5sBBI", cls.MAGIC, cls.VERSION, int(msg_type), len(raw_json))
        return header + raw_json

    @classmethod
    def decode_packet(cls, data: bytes) -> Tuple[BeamMessageType, Dict[str, Any]]:
        if len(data) < 11:
            raise ValueError(f"Packet too short: {len(data)} bytes")
        magic, version, msg_type_val, payload_len = struct.unpack(">5sBBI", data[:11])
        if magic != cls.MAGIC:
            raise ValueError(f"Invalid magic header: {magic!r}")
        if version != cls.VERSION:
            raise ValueError(f"Unsupported protocol version: {version}")
        
        payload_bytes = data[11:11 + payload_len]
        if len(payload_bytes) != payload_len:
            raise ValueError(f"Payload truncated: expected {payload_len}, got {len(payload_bytes)}")
        
        msg_type = BeamMessageType(msg_type_val)
        parsed = json.loads(payload_bytes.decode("utf-8"))
        return (msg_type, parsed)

    @classmethod
    def encode_media_frame(cls, msg_type: BeamMessageType, timestamp_us: int, frame_data: bytes) -> bytes:
        # Format: MAGIC (5B) + VERSION (1B) + TYPE (1B) + LENGTH (4B) + TIMESTAMP (8B) + RAW BYTES
        header = struct.pack(">5sBBIQ", cls.MAGIC, cls.VERSION, int(msg_type), len(frame_data) + 8, timestamp_us)
        return header + frame_data
