#!/usr/bin/env python3
"""
Unit tests for Fireball Desktop Client Core.
"""

import unittest
from fireball_client import ClientConfig, FireballDesktopClientCore


class FireballClientTests(unittest.TestCase):
    def test_client_config_defaults(self):
        cfg = ClientConfig()
        self.assertEqual(cfg.server_url, "http://localhost:9090")
        self.assertEqual(cfg.fps_target, 30)

    def test_client_initial_state(self):
        client = FireballDesktopClientCore()
        self.assertFalse(client.is_connected)
        self.assertEqual(client.frames_received, 0)
        self.assertIsNone(client.last_frame_bytes)


if __name__ == "__main__":
    unittest.main()
