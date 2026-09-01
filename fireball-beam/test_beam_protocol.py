#!/usr/bin/env python3
"""
Unit tests for Fireball Beam protocol, codec, and pairing manager.
"""

import time
import unittest

from beam_protocol import (
    BEAM_WORDLIST,
    BeamMessageType,
    BeamPacketCodec,
    BeamTabState,
    NormalizedTouch,
    generate_six_word_phrase,
)
from beam_server import BeamPairingManager, BeamSession


class BeamProtocolTests(unittest.TestCase):

    def test_json_packet_encoding_and_decoding(self):
        payload = {
            "tab_id": "tab-1234",
            "url": "https://duckduckgo.com",
            "title": "DuckDuckGo",
            "can_go_back": True,
            "blocked_trackers": 7,
        }
        encoded = BeamPacketCodec.encode_json_packet(BeamMessageType.TAB_STATE_UPDATE, payload)
        self.assertTrue(encoded.startswith(b"FBEAM"))
        
        msg_type, decoded = BeamPacketCodec.decode_packet(encoded)
        self.assertEqual(msg_type, BeamMessageType.TAB_STATE_UPDATE)
        self.assertEqual(decoded["tab_id"], "tab-1234")
        self.assertEqual(decoded["url"], "https://duckduckgo.com")
        self.assertEqual(decoded["blocked_trackers"], 7)

    def test_corrupted_magic_rejection(self):
        corrupted = b"XBEAM\x01\x12\x00\x00\x00\x05{}"
        with self.assertRaises(ValueError) as ctx:
            BeamPacketCodec.decode_packet(corrupted)
        self.assertIn("Invalid magic header", str(ctx.exception))

    def test_truncated_packet_rejection(self):
        short_packet = b"FBEAM\x01\x12\x00\x00\x00\x20{}"  # Claims 32 bytes payload, only gives 2
        with self.assertRaises(ValueError) as ctx:
            BeamPacketCodec.decode_packet(short_packet)
        self.assertIn("Payload truncated", str(ctx.exception))

    def test_media_frame_encoding(self):
        sample_frame = b"\x00\x00\x00\x01\x67\x42\x00\x1f"
        timestamp = 1725178900123456
        encoded = BeamPacketCodec.encode_media_frame(BeamMessageType.FRAME_VIDEO_H264, timestamp, sample_frame)
        self.assertTrue(encoded.startswith(b"FBEAM"))
        self.assertEqual(len(encoded), 11 + 8 + len(sample_frame))

    def test_six_word_phrase_generation(self):
        seed = b"deterministic_seed_bytes_32_len_"
        phrase = generate_six_word_phrase(seed)
        self.assertEqual(len(phrase), 6)
        for word in phrase:
            self.assertIn(word, BEAM_WORDLIST)

        # Must be deterministic for same seed
        phrase2 = generate_six_word_phrase(seed)
        self.assertEqual(phrase, phrase2)

    def test_touch_coordinate_normalization_and_translation(self):
        touch = NormalizedTouch(id=0, norm_x=0.5, norm_y=0.25, pressure=1.0)
        px, py = touch.to_pixel_coords(viewport_width=1080, viewport_height=1920)
        self.assertEqual(px, 540)
        self.assertEqual(py, 480)

        # Boundary clamping
        touch_overflow = NormalizedTouch(id=1, norm_x=1.5, norm_y=-0.5)
        px_clamp, py_clamp = touch_overflow.to_pixel_coords(viewport_width=1080, viewport_height=1920)
        self.assertEqual(px_clamp, 1079)
        self.assertEqual(py_clamp, 0)

    def test_pairing_manager_lifecycle(self):
        mgr = BeamPairingManager(token_ttl_seconds=60)
        token, words, qr_payload = mgr.create_invitation()
        self.assertTrue(token)
        self.assertEqual(len(words), 6)
        self.assertTrue(qr_payload.startswith("fireball-beam://pair?token="))

        # Successful pairing
        res = mgr.verify_pairing_attempt(
            token=token,
            device_id="iphone-15-pro-uuid",
            device_name="My iPhone",
            platform="ios",
            client_nonce="random_nonce_1234",
        )
        self.assertIsNotNone(res)
        shared_secret, confirmed_words = res
        self.assertEqual(confirmed_words, words)
        self.assertEqual(len(shared_secret), 32)
        self.assertTrue(mgr.is_device_authorized("iphone-15-pro-uuid"))

        # Single-use: Second attempt with same token must fail
        res_reuse = mgr.verify_pairing_attempt(
            token=token,
            device_id="attacker-device",
            device_name="Attacker",
            platform="ios",
            client_nonce="nonce_attack",
        )
        self.assertIsNone(res_reuse)

        # Revocation
        self.assertTrue(mgr.revoke_device("iphone-15-pro-uuid"))
        self.assertFalse(mgr.is_device_authorized("iphone-15-pro-uuid"))

    def test_beam_session_cdp_events(self):
        session = BeamSession(session_id="sess-01", device_id="ipad-pro", viewport_width=1000, viewport_height=2000)
        touch = NormalizedTouch(id=0, norm_x=0.2, norm_y=0.4, pressure=0.8)
        
        cdp_touch = session.generate_cdp_touch_event("touchStart", touch)
        self.assertEqual(cdp_touch["type"], "touchStart")
        self.assertEqual(cdp_touch["touchPoints"][0]["x"], 200)
        self.assertEqual(cdp_touch["touchPoints"][0]["y"], 800)

        cdp_mouse = session.generate_cdp_mouse_event("mousePressed", touch, button="left")
        self.assertEqual(cdp_mouse["type"], "mousePressed")
        self.assertEqual(cdp_mouse["x"], 200)
        self.assertEqual(cdp_mouse["y"], 800)
        self.assertEqual(cdp_mouse["button"], "left")


if __name__ == "__main__":
    unittest.main()
