from __future__ import annotations

import hashlib
import json
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from fireball_patches import ManifestError, apply_queue, validate_manifest  # noqa: E402


class PatchToolTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.repo = pathlib.Path(self.temporary.name)
        (self.repo / "patches").mkdir()
        shutil.copy(ROOT / "tests/fixtures/patches/sample.patch", self.repo / "patches/sample.patch")
        self.manifest = self.repo / "patches/manifest.json"
        self.write_manifest(checksum=self.checksum())

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_validate_apply_and_reverse(self) -> None:
        checkout = self.make_checkout("base")
        validate_manifest(self.manifest, self.repo)
        apply_queue(self.manifest, self.repo, checkout, reverse=False)
        self.assertEqual((checkout / "sample.txt").read_text(), "fireball overlay\n")
        apply_queue(self.manifest, self.repo, checkout, reverse=True)
        self.assertEqual((checkout / "sample.txt").read_text(), "chromium baseline\n")

    def test_conflict_fails_before_mutation(self) -> None:
        checkout = self.make_checkout("conflict")
        with self.assertRaises(subprocess.CalledProcessError):
            apply_queue(self.manifest, self.repo, checkout, reverse=False)
        self.assertEqual((checkout / "sample.txt").read_text(), "locally changed baseline\n")

    def test_checksum_mismatch_is_rejected(self) -> None:
        self.write_manifest(checksum="0" * 64)
        with self.assertRaisesRegex(ManifestError, "checksum mismatch"):
            validate_manifest(self.manifest, self.repo)

    def test_path_traversal_is_rejected(self) -> None:
        self.write_manifest(checksum=self.checksum(), path="../outside.patch")
        with self.assertRaisesRegex(ManifestError, "below patches"):
            validate_manifest(self.manifest, self.repo)

    def test_automatic_import_is_rejected(self) -> None:
        document = json.loads(self.manifest.read_text(encoding="utf-8"))
        document["automatic_import"] = True
        self.manifest.write_text(json.dumps(document), encoding="utf-8")
        with self.assertRaisesRegex(ManifestError, "automatic patch import"):
            validate_manifest(self.manifest, self.repo)

    def test_source_path_traversal_is_rejected(self) -> None:
        document = json.loads(self.manifest.read_text(encoding="utf-8"))
        document["patches"][0]["source"]["path"] = "../stolen.patch"
        self.manifest.write_text(json.dumps(document), encoding="utf-8")
        with self.assertRaisesRegex(ManifestError, "repository-relative"):
            validate_manifest(self.manifest, self.repo)

    def test_stale_chromium_verification_is_rejected(self) -> None:
        with self.assertRaisesRegex(ManifestError, "pinned Chromium commit"):
            validate_manifest(self.manifest, self.repo, "b" * 40)

    def checksum(self) -> str:
        return hashlib.sha256((self.repo / "patches/sample.patch").read_bytes()).hexdigest()

    def write_manifest(self, checksum: str, path: str = "patches/sample.patch") -> None:
        document = {
            "schema_version": 2,
            "automatic_import": False,
            "patches": [
                {
                    "id": "sample_patch",
                    "path": path,
                    "sha256": checksum,
                    "source": {
                        "project": "example",
                        "repository": "https://example.invalid/source.git",
                        "path": "patches/sample.patch",
                        "commit": "a" * 40,
                        "license": "BSD-3-Clause",
                        "license_url": "https://example.invalid/LICENSE",
                    },
                    "chromium_range": {"minimum": 140, "maximum": 145},
                    "verified_against": "a" * 40,
                    "security_impact": "fixture only; no production impact",
                    "required_tests": ["tests.test_patch_tool"],
                }
            ],
        }
        self.manifest.write_text(json.dumps(document), encoding="utf-8")

    def make_checkout(self, fixture: str) -> pathlib.Path:
        checkout = self.repo / f"checkout-{fixture}"
        shutil.copytree(ROOT / f"tests/fixtures/{fixture}", checkout)
        subprocess.run(["git", "init", "--quiet", str(checkout)], check=True)
        return checkout


if __name__ == "__main__":
    unittest.main()
