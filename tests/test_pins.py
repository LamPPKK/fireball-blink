from __future__ import annotations

import copy
import json
import pathlib
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from check_pins import PinError, validate_references, validate_upstream  # noqa: E402


class PinTests(unittest.TestCase):
    def setUp(self) -> None:
        self.upstream = json.loads((ROOT / "pins/upstream.json").read_text(encoding="utf-8"))
        self.references = json.loads(
            (ROOT / "pins/reference-browsers.json").read_text(encoding="utf-8")
        )

    def test_repository_pins_are_valid(self) -> None:
        validate_upstream(self.upstream)
        validate_references(self.references)

    def test_chromium_branch_is_rejected(self) -> None:
        document = copy.deepcopy(self.upstream)
        document["chromium"]["ref"] = "refs/heads/main"
        with self.assertRaisesRegex(PinError, "exact release tag"):
            validate_upstream(document)

    def test_chromium_milestone_must_match_version(self) -> None:
        document = copy.deepcopy(self.upstream)
        document["chromium"]["milestone"] = 150
        with self.assertRaisesRegex(PinError, "milestone"):
            validate_upstream(document)

    def test_automatic_reference_import_is_rejected(self) -> None:
        document = copy.deepcopy(self.references)
        document["automatic_patch_import"] = True
        with self.assertRaisesRegex(PinError, "automatic patch import"):
            validate_references(document)

    def test_reference_commit_must_be_exact(self) -> None:
        document = copy.deepcopy(self.references)
        document["references"][0]["revision"] = "master"
        with self.assertRaisesRegex(PinError, "exact SHA-1"):
            validate_references(document)


if __name__ == "__main__":
    unittest.main()
