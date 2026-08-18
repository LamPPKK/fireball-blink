from __future__ import annotations

import copy
import datetime
import json
import pathlib
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from security_rebases import RebaseError, gate_status, validate_ledger  # noqa: E402


class SecurityRebaseTests(unittest.TestCase):
    def setUp(self) -> None:
        self.ledger = json.loads((ROOT / "security/rebases.json").read_text(encoding="utf-8"))

    def event(self, sequence: int, elapsed_hours: int = 48) -> dict[str, object]:
        released = datetime.datetime(2026, 8, sequence, tzinfo=datetime.timezone.utc)
        return {
            "id": f"m151-2026-08-{sequence:02d}",
            "version": f"151.0.7922.{100 + sequence}",
            "milestone": 151,
            "chromium_revision": f"{sequence:040x}",
            "upstream_released_at": released.isoformat(),
            "triaged_at": (released + datetime.timedelta(hours=1)).isoformat(),
            "rebase_completed_at": (released + datetime.timedelta(hours=24)).isoformat(),
            "artifact_promoted_at": (released + datetime.timedelta(hours=elapsed_hours)).isoformat(),
            "artifact_sha256": f"{sequence:064x}",
            "outcome": "passed",
            "failure_reason": None,
            "results": {
                "control_build": "passed",
                "overlay_build": "passed",
                "smoke_tests": "passed",
                "startup_network_audit": "passed",
            },
        }

    def test_empty_ledger_is_valid_but_gate_is_closed(self) -> None:
        self.assertEqual(
            gate_status(self.ledger),
            {
                "completed_consecutive_passes": 0,
                "required_consecutive_passes": 2,
                "linux_alpha_gate_ready": False,
            },
        )

    def test_two_passing_rebases_open_gate(self) -> None:
        self.ledger["events"] = [self.event(1), self.event(2)]
        self.assertTrue(gate_status(self.ledger)["linux_alpha_gate_ready"])

    def test_sla_breach_is_rejected(self) -> None:
        self.ledger["events"] = [self.event(1, elapsed_hours=73)]
        with self.assertRaisesRegex(RebaseError, "72-hour SLA"):
            validate_ledger(self.ledger)

    def test_failed_check_is_recorded_and_closes_gate(self) -> None:
        event = self.event(1)
        event["results"]["overlay_build"] = "failed"  # type: ignore[index]
        event["outcome"] = "failed"
        event["failure_reason"] = "Overlay build failed."
        event["artifact_promoted_at"] = None
        event["artifact_sha256"] = None
        self.ledger["events"] = [event]
        self.assertFalse(gate_status(self.ledger)["linux_alpha_gate_ready"])

    def test_failed_event_breaks_consecutive_streak(self) -> None:
        first = self.event(1)
        failed = self.event(2)
        failed["results"]["smoke_tests"] = "failed"  # type: ignore[index]
        failed["outcome"] = "failed"
        failed["failure_reason"] = "Smoke tests failed."
        failed["artifact_promoted_at"] = None
        failed["artifact_sha256"] = None
        third = self.event(3)
        self.ledger["events"] = [first, failed, third]
        status = gate_status(self.ledger)
        self.assertEqual(status["completed_consecutive_passes"], 1)
        self.assertFalse(status["linux_alpha_gate_ready"])

    def test_out_of_order_timestamps_are_rejected(self) -> None:
        event = self.event(1)
        event["triaged_at"] = "2026-07-31T23:00:00+00:00"
        self.ledger["events"] = [event]
        with self.assertRaisesRegex(RebaseError, "out of order"):
            validate_ledger(self.ledger)

    def test_duplicate_release_revision_is_rejected(self) -> None:
        first = self.event(1)
        second = self.event(2)
        second["chromium_revision"] = first["chromium_revision"]
        self.ledger["events"] = [first, second]
        with self.assertRaisesRegex(RebaseError, "duplicate Chromium revision"):
            validate_ledger(self.ledger)


if __name__ == "__main__":
    unittest.main()
