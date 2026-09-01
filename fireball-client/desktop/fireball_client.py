#!/usr/bin/env python3
"""
Fireball Desktop Client
Standalone, cross-platform thin streaming client for Windows, macOS, and Linux.
"""

from __future__ import annotations

import argparse
import sys
import time
import urllib.request
import json
from dataclasses import dataclass
from typing import Optional, Tuple


@dataclass
class ClientConfig:
    server_url: str = "http://localhost:9090"
    fps_target: int = 30
    token: Optional[str] = None
    window_title: str = "Fireball Desktop Client"


class FireballDesktopClientCore:
    """Core thin-client network and streaming engine."""

    def __init__(self, config: Optional[ClientConfig] = None) -> None:
        self.config = config or ClientConfig()
        self.is_connected = False
        self.last_frame_bytes: Optional[bytes] = None
        self.frames_received = 0

    def check_server_health(self) -> bool:
        """Pings Fireball Server /health endpoint."""
        try:
            req = urllib.request.Request(f"{self.config.server_url}/health", headers={"User-Agent": "FireballClient/1.0"})
            with urllib.request.urlopen(req, timeout=3.0) as resp:
                if resp.status == 200:
                    data = json.loads(resp.read().decode("utf-8"))
                    return data.get("status") == "ok"
        except Exception:
            return False
        return False

    def fetch_next_frame(self) -> Optional[bytes]:
        """Fetches latest frame from server."""
        try:
            req = urllib.request.Request(f"{self.config.server_url}/stream/frame?t={time.time()}", headers={"User-Agent": "FireballClient/1.0"})
            with urllib.request.urlopen(req, timeout=2.0) as resp:
                if resp.status == 200:
                    self.last_frame_bytes = resp.read()
                    self.frames_received += 1
                    self.is_connected = True
                    return self.last_frame_bytes
        except Exception:
            self.is_connected = False
            return None
        return None

    def send_input_touch(self, normalized_x: float, normalized_y: float, event_type: str = "touchStart") -> bool:
        """Dispatches normalized (0.0-1.0) touch event to Fireball Server."""
        payload = {
            "type": "touch_event",
            "event": event_type,
            "x": max(0.0, min(1.0, normalized_x)),
            "y": max(0.0, min(1.0, normalized_y)),
            "timestamp": time.time()
        }
        try:
            req = urllib.request.Request(
                f"{self.config.server_url}/input/touch",
                data=json.dumps(payload).encode("utf-8"),
                headers={"Content-Type": "application/json", "User-Agent": "FireballClient/1.0"},
                method="POST"
            )
            with urllib.request.urlopen(req, timeout=2.0) as resp:
                return resp.status == 200
        except Exception:
            return False

    def navigate_url(self, target_url: str) -> bool:
        """Instructs Fireball Server to navigate to a target URL."""
        payload = {"url": target_url}
        try:
            req = urllib.request.Request(
                f"{self.config.server_url}/navigation/load",
                data=json.dumps(payload).encode("utf-8"),
                headers={"Content-Type": "application/json", "User-Agent": "FireballClient/1.0"},
                method="POST"
            )
            with urllib.request.urlopen(req, timeout=2.0) as resp:
                return resp.status == 200
        except Exception:
            return False


def main() -> int:
    parser = argparse.ArgumentParser(description="Fireball Desktop Client")
    parser.add_argument("--server", default="http://localhost:9090", help="Fireball Server URL")
    parser.add_argument("--fps", type=int, default=30, help="Target FPS")
    args = parser.parse_args()

    cfg = ClientConfig(server_url=args.server, fps_target=args.fps)
    client = FireballDesktopClientCore(cfg)

    print(f"📡 Connecting to Fireball Server at {cfg.server_url}...")
    if not client.check_server_health():
        print(f"⚠️ Warning: Could not connect to {cfg.server_url}. Make sure Fireball Server is running.")
    else:
        print("✅ Connected to Fireball Server!")

    print("🚀 Fireball Client ready.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
