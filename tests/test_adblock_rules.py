from __future__ import annotations

import json
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
TOOL = ROOT / "tools/adblock_rules.py"
FIXTURES = ROOT / "tests/fixtures/adblock"
ARTIFACT_URL = "https://updates.fireball.example/adblock/rules-v1.txt"
CREATED_AT = "2026-08-20T00:00:00Z"


class AdblockRulesToolTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(prefix="fireball-adblock-test-")
        self.root = pathlib.Path(self.temporary.name)
        self.openssl = shutil.which("openssl")
        if self.openssl is None:
            self.skipTest("OpenSSL is required")
        self.key = self.root / "signing-key.pem"
        subprocess.run(
            [self.openssl, "genpkey", "-algorithm", "ED25519", "-out", str(self.key)],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        self.key.chmod(0o600)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def run_tool(self, *arguments: object, check: bool = True) -> subprocess.CompletedProcess[str]:
        result = subprocess.run(
            [sys.executable, str(TOOL), *(str(argument) for argument in arguments)],
            cwd=ROOT,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        if check and result.returncode != 0:
            self.fail(f"tool failed: {result.stdout}{result.stderr}")
        return result

    def build(self, directory: pathlib.Path, source_lock: pathlib.Path | None = None) -> tuple[pathlib.Path, pathlib.Path, pathlib.Path]:
        directory.mkdir()
        rules = directory / "rules.txt"
        manifest = directory / "manifest.json"
        public_key = directory / "public-key.bin"
        self.run_tool(
            "build",
            "--source-lock",
            source_lock or FIXTURES / "source-lock.json",
            "--signing-key",
            self.key,
            "--artifact-url",
            ARTIFACT_URL,
            "--minimum-app-version",
            "0.1.0",
            "--created-at",
            CREATED_AT,
            "--output-rules",
            rules,
            "--output-manifest",
            manifest,
            "--output-public-key",
            public_key,
            "--openssl",
            self.openssl,
        )
        return rules, manifest, public_key

    def verify(self, rules: pathlib.Path, manifest: pathlib.Path, public_key: pathlib.Path) -> subprocess.CompletedProcess[str]:
        return self.run_tool(
            "verify",
            "--rules",
            rules,
            "--manifest",
            manifest,
            "--public-key",
            public_key,
            "--openssl",
            self.openssl,
            check=False,
        )

    def test_build_is_deterministic_sorted_and_verifiable(self) -> None:
        first = self.build(self.root / "first")
        second = self.build(self.root / "second")
        for first_path, second_path in zip(first, second, strict=True):
            self.assertEqual(first_path.read_bytes(), second_path.read_bytes())

        rules = first[0].read_text(encoding="utf-8")
        self.assertEqual(rules.count("[Adblock Plus 2.0]"), 1)
        self.assertLess(rules.index("fireball-source easylist"), rules.index("fireball-source easyprivacy"))
        manifest = json.loads(first[1].read_text(encoding="utf-8"))
        self.assertEqual([source["name"] for source in manifest["sources"]], ["easylist", "easyprivacy"])
        self.assertEqual(self.verify(*first).returncode, 0)

    def test_rules_and_signed_provenance_tampering_fail_verification(self) -> None:
        rules, manifest, public_key = self.build(self.root / "tamper")
        original_rules = rules.read_bytes()
        rules.write_bytes(original_rules + b"||tampered.example^\n")
        self.assertNotEqual(self.verify(rules, manifest, public_key).returncode, 0)
        rules.write_bytes(original_rules)

        document = json.loads(manifest.read_text(encoding="utf-8"))
        document["artifact"]["url"] = "https://evil.example/rules.txt"
        manifest.write_text(json.dumps(document), encoding="utf-8")
        self.assertNotEqual(self.verify(rules, manifest, public_key).returncode, 0)
        document["artifact"]["url"] = ARTIFACT_URL
        document["sources"][0]["revision"] = "f" * 40
        manifest.write_text(json.dumps(document), encoding="utf-8")
        self.assertNotEqual(self.verify(rules, manifest, public_key).returncode, 0)

    def test_source_checksum_failure_publishes_no_output(self) -> None:
        fixture_root = self.root / "bad-source"
        shutil.copytree(FIXTURES, fixture_root)
        with (fixture_root / "easylist.txt").open("ab") as handle:
            handle.write(b"||not-pinned.example^\n")
        output = self.root / "bad-output"
        output.mkdir()
        result = self.run_tool(
            "build",
            "--source-lock",
            fixture_root / "source-lock.json",
            "--signing-key",
            self.key,
            "--artifact-url",
            ARTIFACT_URL,
            "--minimum-app-version",
            "0.1.0",
            "--created-at",
            CREATED_AT,
            "--output-rules",
            output / "rules.txt",
            "--output-manifest",
            output / "manifest.json",
            "--output-public-key",
            output / "public-key.bin",
            "--openssl",
            self.openssl,
            check=False,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(list(output.iterdir()), [])

    def test_insecure_private_key_and_output_overwrite_are_rejected(self) -> None:
        self.key.chmod(0o644)
        output = self.root / "insecure"
        output.mkdir()
        arguments = [
            "build",
            "--source-lock", FIXTURES / "source-lock.json",
            "--signing-key", self.key,
            "--artifact-url", ARTIFACT_URL,
            "--minimum-app-version", "0.1.0",
            "--created-at", CREATED_AT,
            "--output-rules", output / "rules.txt",
            "--output-manifest", output / "manifest.json",
            "--output-public-key", output / "public-key.bin",
            "--openssl", self.openssl,
        ]
        self.assertNotEqual(self.run_tool(*arguments, check=False).returncode, 0)
        self.assertEqual(list(output.iterdir()), [])

        self.key.chmod(0o600)
        linked_key = self.root / "linked-key.pem"
        linked_key.symlink_to(self.key)
        arguments[arguments.index("--signing-key") + 1] = linked_key
        self.assertNotEqual(self.run_tool(*arguments, check=False).returncode, 0)
        self.assertEqual(list(output.iterdir()), [])
        arguments[arguments.index("--signing-key") + 1] = self.key

        existing_rules, existing_manifest, existing_public_key = self.build(
            self.root / "existing"
        )
        preserved = existing_rules.read_bytes()
        overwrite = self.run_tool(
            "build",
            "--source-lock",
            FIXTURES / "source-lock.json",
            "--signing-key",
            self.key,
            "--artifact-url",
            ARTIFACT_URL,
            "--minimum-app-version",
            "0.1.0",
            "--created-at",
            CREATED_AT,
            "--output-rules",
            existing_rules,
            "--output-manifest",
            existing_manifest,
            "--output-public-key",
            existing_public_key,
            "--openssl",
            self.openssl,
            check=False,
        )
        self.assertNotEqual(overwrite.returncode, 0)
        self.assertEqual(existing_rules.read_bytes(), preserved)

        partial = self.root / "partial"
        partial.mkdir()
        foreign_manifest = partial / "manifest.json"
        foreign_manifest.write_bytes(b"foreign\n")
        partial_build = self.run_tool(
            "build",
            "--source-lock",
            FIXTURES / "source-lock.json",
            "--signing-key",
            self.key,
            "--artifact-url",
            ARTIFACT_URL,
            "--minimum-app-version",
            "0.1.0",
            "--created-at",
            CREATED_AT,
            "--output-rules",
            partial / "rules.txt",
            "--output-manifest",
            foreign_manifest,
            "--output-public-key",
            partial / "public-key.bin",
            "--openssl",
            self.openssl,
            check=False,
        )
        self.assertNotEqual(partial_build.returncode, 0)
        self.assertEqual(foreign_manifest.read_bytes(), b"foreign\n")
        self.assertFalse((partial / "rules.txt").exists())
        self.assertFalse((partial / "public-key.bin").exists())


if __name__ == "__main__":
    unittest.main()
