#!/usr/bin/env python3
"""
Fireball Beam Server
Streaming daemon and pairing coordinator for remote browser execution.
"""

from __future__ import annotations

import asyncio
import hashlib
import hmac
import os
import secrets
import time
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, Set, Tuple

from beam_protocol import (
    BeamMessageType,
    BeamPacketCodec,
    BeamTabState,
    NormalizedTouch,
    generate_six_word_phrase,
)


@dataclass
class PairingInvitation:
    token: str
    created_at: float
    expires_at: float
    six_words: List[str]
    is_used: bool = False
    failed_attempts: int = 0


@dataclass
class PairedDevice:
    device_id: str
    device_name: str
    platform: str  # "ios", "android", "macos", "windows"
    shared_secret: bytes
    paired_at: float
    last_seen_at: float


class BeamPairingManager:
    """Manages secure device pairing inspired by seg6/surf."""

    def __init__(self, token_ttl_seconds: int = 300) -> None:
        self.token_ttl = token_ttl_seconds
        self.pending_invitations: Dict[str, PairingInvitation] = {}
        self.paired_devices: Dict[str, PairedDevice] = {}

    def create_invitation(self) -> Tuple[str, List[str], str]:
        """Creates a single-use pairing invitation ticket."""
        raw_seed = secrets.token_bytes(32)
        token = secrets.token_hex(16)
        six_words = generate_six_word_phrase(raw_seed)
        now = time.time()

        invitation = PairingInvitation(
            token=token,
            created_at=now,
            expires_at=now + self.token_ttl,
            six_words=six_words,
        )
        self.pending_invitations[token] = invitation
        qr_payload = f"fireball-beam://pair?token={token}&phrase={'-'.join(six_words)}"
        return token, six_words, qr_payload

    def verify_pairing_attempt(
        self,
        token: str,
        device_id: str,
        device_name: str,
        platform: str,
        client_nonce: str,
    ) -> Optional[Tuple[bytes, List[str]]]:
        """Verifies pairing request, generates shared session secret, and consumes invitation."""
        invitation = self.pending_invitations.get(token)
        if not invitation:
            return None
        
        now = time.time()
        if invitation.is_used or now > invitation.expires_at:
            self.pending_invitations.pop(token, None)
            return None
        
        if invitation.failed_attempts >= 5:
            self.pending_invitations.pop(token, None)
            return None

        # Derive shared secret: HKDF-like HMAC(token, client_nonce + device_id)
        secret_material = f"{token}:{client_nonce}:{device_id}".encode("utf-8")
        shared_secret = hashlib.sha256(secret_material).digest()

        # Mark consumed
        invitation.is_used = True
        self.pending_invitations.pop(token, None)

        paired = PairedDevice(
            device_id=device_id,
            device_name=device_name,
            platform=platform,
            shared_secret=shared_secret,
            paired_at=now,
            last_seen_at=now,
        )
        self.paired_devices[device_id] = paired
        return shared_secret, invitation.six_words

    def is_device_authorized(self, device_id: str) -> bool:
        return device_id in self.paired_devices

    def revoke_device(self, device_id: str) -> bool:
        return self.paired_devices.pop(device_id, None) is not None


class BeamSession:
    """Represents an active streaming session with a connected client device."""

    def __init__(
        self,
        session_id: str,
        device_id: str,
        viewport_width: int = 1080,
        viewport_height: int = 1920,
    ) -> None:
        self.session_id = session_id
        self.device_id = device_id
        self.viewport_width = viewport_width
        self.viewport_height = viewport_height
        self.active_tab: Optional[BeamTabState] = None
        self.is_streaming: bool = False
        self.last_heartbeat: float = time.time()

    def update_viewport(self, width: int, height: int) -> None:
        self.viewport_width = max(320, width)
        self.viewport_height = max(480, height)

    def translate_touch(self, touch: NormalizedTouch) -> Tuple[int, int]:
        """Translates normalized 0.0 - 1.0 client coordinates to native host viewport pixels."""
        return touch.to_pixel_coords(self.viewport_width, self.viewport_height)

    def generate_cdp_touch_event(self, touch_type: str, touch: NormalizedTouch) -> Dict[str, Any]:
        """Generates a Chrome DevTools Protocol Input.dispatchTouchEvent payload."""
        px, py = self.translate_touch(touch)
        return {
            "type": touch_type,
            "touchPoints": [
                {
                    "x": px,
                    "y": py,
                    "radiusX": 5,
                    "radiusY": 5,
                    "force": touch.pressure,
                    "id": touch.id,
                }
            ],
        }

    def generate_cdp_mouse_event(self, event_type: str, touch: NormalizedTouch, button: str = "left") -> Dict[str, Any]:
        """Generates a Chrome DevTools Protocol Input.dispatchMouseEvent payload."""
        px, py = self.translate_touch(touch)
        return {
            "type": event_type,
            "x": px,
            "y": py,
            "button": button,
            "clickCount": 1 if event_type == "mousePressed" else 0,
        }
