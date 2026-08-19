from __future__ import annotations

import copy
import json
import pathlib
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from network_policy import PolicyError, generate_cpp, validate_policy  # noqa: E402


class NetworkPolicyTests(unittest.TestCase):
    def setUp(self) -> None:
        self.policy = json.loads(
            (ROOT / "policies/startup_network.json").read_text(encoding="utf-8")
        )

    def test_generated_policy_is_current_and_default_deny(self) -> None:
        generated = generate_cpp(self.policy)
        checked_in = (
            ROOT / "fireball/components/privacy/startup_network_policy.inc"
        ).read_text(encoding="utf-8")
        self.assertEqual(generated, checked_in)
        self.assertIn("NetworkPolicyEntry, 3", generated)
        self.assertIn('"fireball.transfer.aria2"', generated)
        self.assertIn('"fireball.egress.warp"', generated)
        self.assertIn('"fireball.egress.tor"', generated)
        self.assertIn("NetworkPhase::kPostStartup", generated)

    def test_default_allow_is_rejected(self) -> None:
        policy = copy.deepcopy(self.policy)
        policy["default_action"] = "allow"
        with self.assertRaisesRegex(PolicyError, "must remain deny"):
            validate_policy(policy)

    def test_rule_without_explicit_consent_is_rejected(self) -> None:
        policy = copy.deepcopy(self.policy)
        policy["rules"] = [
            {
                "id": "component_updater",
                "owner": "component-updater",
                "phase": "post_startup",
                "requires_user_consent": False,
                "purpose": "Fetch an update after the user enables updates.",
            }
        ]
        with self.assertRaisesRegex(PolicyError, "explicit user consent"):
            validate_policy(policy)

    def test_explicit_rule_generates_phase_and_owner(self) -> None:
        policy = copy.deepcopy(self.policy)
        policy["rules"] = [
            {
                "id": "component_updater",
                "owner": "component-updater",
                "phase": "post_startup",
                "requires_user_consent": True,
                "purpose": "Fetch an update after the user enables updates.",
            }
        ]
        generated = generate_cpp(policy)
        self.assertIn('"component-updater"', generated)
        self.assertIn("NetworkPhase::kPostStartup", generated)


if __name__ == "__main__":
    unittest.main()
