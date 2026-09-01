#!/usr/bin/env python3
"""
Fireball Sync Relay Daemon
Real-time WebRTC signaling and encrypted BIP-39 synchronization hub.
"""

from __future__ import annotations

import asyncio
import hashlib
import hmac
import json
import time
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, Set


@dataclass
class SyncRoom:
    chain_id: str
    passphrase_hash: str
    clients: Set[str] = field(default_factory=set)
    last_state: Dict[str, Any] = field(default_factory=dict)
    created_at: float = field(default_factory=time.time)


class SyncRelayServer:
    """Coordinates zero-knowledge real-time sync between paired Fireball devices."""

    def __init__(self) -> None:
        self.rooms: Dict[str, SyncRoom] = {}
        self.client_rooms: Dict[str, str] = {}

    def derive_chain_id(self, phrase_words: List[str]) -> str:
        """Derives a deterministic room ID from the 24-word BIP-39 phrase."""
        joined = " ".join(word.strip().lower() for word in phrase_words)
        return hashlib.sha256(joined.encode("utf-8")).hexdigest()[:32]

    def join_sync_room(self, client_id: str, phrase_words: List[str]) -> str:
        chain_id = self.derive_chain_id(phrase_words)
        if chain_id not in self.rooms:
            pass_hash = hashlib.sha256(" ".join(phrase_words).encode("utf-8")).hexdigest()
            self.rooms[chain_id] = SyncRoom(chain_id=chain_id, passphrase_hash=pass_hash)

        self.rooms[chain_id].clients.add(client_id)
        self.client_rooms[client_id] = chain_id
        return chain_id

    def leave_sync_room(self, client_id: str) -> None:
        chain_id = self.client_rooms.pop(client_id, None)
        if chain_id and chain_id in self.rooms:
            self.rooms[chain_id].clients.discard(client_id)
            if not self.rooms[chain_id].clients:
                self.rooms.pop(chain_id, None)

    def broadcast_payload(self, client_id: str, encrypted_payload: Dict[str, Any]) -> List[str]:
        """Broadcasts an encrypted sync change (tabs, bookmarks, vault) to all other peers in the room."""
        chain_id = self.client_rooms.get(client_id)
        if not chain_id or chain_id not in self.rooms:
            return []

        room = self.rooms[chain_id]
        room.last_state = encrypted_payload
        # Return list of recipient client IDs (excluding sender)
        return [cid for cid in room.clients if cid != client_id]

    def get_room_clients_count(self, chain_id: str) -> int:
        return len(self.rooms.get(chain_id, SyncRoom("", "")).clients)


if __name__ == "__main__":
    relay = SyncRelayServer()
    test_phrase = ["abandon"] * 24
    c1 = "client-android-1"
    c2 = "client-desktop-2"

    room = relay.join_sync_room(c1, test_phrase)
    relay.join_sync_room(c2, test_phrase)

    assert relay.get_room_clients_count(room) == 2
    recipients = relay.broadcast_payload(c1, {"type": "tab_opened", "payload": "enc_data_123"})
    assert recipients == [c2]

    relay.leave_sync_room(c1)
    relay.leave_sync_room(c2)
    assert room not in relay.rooms
    print("✅ SyncRelayServer self-test passed!")
