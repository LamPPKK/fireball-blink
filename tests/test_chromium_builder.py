from __future__ import annotations

import copy
import json
import pathlib
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from chromium_builder import (  # noqa: E402
    BuilderError,
    CONTROL_GN_ARGS,
    SystemProbe,
    build_manifest,
    evaluate_probe,
    export_pins,
    parse_gn_args,
    preflight_report,
    validate_control_gn_args,
    verify_checkout,
)


class ChromiumBuilderTests(unittest.TestCase):
    def passing_probe(self) -> SystemProbe:
        return SystemProbe(
            os_id="ubuntu",
            os_version="24.04",
            architecture="x86_64",
            cpu_count=8,
            memory_bytes=32 * 1024**3,
            free_disk_bytes=300 * 1024**3,
            effective_uid=1000,
            missing_commands=(),
            workspace="/runner-temp",
        )

    def test_exact_minimum_builder_passes(self) -> None:
        report = preflight_report(self.passing_probe())
        self.assertTrue(report["passed"])
        self.assertEqual(report["failures"], [])

    def test_each_builder_boundary_fails_closed(self) -> None:
        base = self.passing_probe()
        cases = (
            ("os_id", "debian", "UNSUPPORTED_OS"),
            ("os_version", "22.04", "UNSUPPORTED_OS"),
            ("architecture", "aarch64", "UNSUPPORTED_ARCH"),
            ("cpu_count", 7, "INSUFFICIENT_CPU"),
            ("memory_bytes", 32 * 1024**3 - 1, "INSUFFICIENT_MEMORY"),
            ("free_disk_bytes", 300 * 1024**3 - 1, "INSUFFICIENT_DISK"),
            ("effective_uid", 0, "ROOT_RUNNER"),
            ("missing_commands", ("git",), "MISSING_COMMANDS"),
        )
        for field, value, code in cases:
            with self.subTest(field=field):
                values = dict(base.__dict__)
                values[field] = value
                failures = evaluate_probe(SystemProbe(**values))
                self.assertIn(code, {failure["code"] for failure in failures})

    def test_control_gn_args_are_exact_and_generic(self) -> None:
        values = parse_gn_args(ROOT / "build-config/chromium-control.gn")
        validate_control_gn_args(values)
        self.assertEqual(values, CONTROL_GN_ARGS)
        self.assertEqual(values["target_cpu"], '"x64"')
        self.assertEqual(values["chrome_pgo_phase"], "0")
        self.assertEqual(values["use_thin_lto"], "false")
        self.assertNotIn("mimalloc", " ".join(values.values()))

    def test_control_gn_args_reject_optimization_or_extra_flags(self) -> None:
        for key, value in (
            ("use_thin_lto", "true"),
            ("chrome_pgo_phase", "2"),
            ("is_official_build", "true"),
            ("use_mimalloc", "true"),
        ):
            with self.subTest(key=key):
                changed = copy.deepcopy(CONTROL_GN_ARGS)
                changed[key] = value
                with self.assertRaisesRegex(BuilderError, "control GN arguments changed"):
                    validate_control_gn_args(changed)

    def test_checkout_requires_exact_origin_and_revision(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            checkout = pathlib.Path(temporary) / "checkout"
            checkout.mkdir()
            subprocess.run(["git", "init", "-q", str(checkout)], check=True)
            subprocess.run(
                ["git", "-C", str(checkout), "config", "user.email", "ci@example.invalid"],
                check=True,
            )
            subprocess.run(
                ["git", "-C", str(checkout), "config", "user.name", "Fireball CI"],
                check=True,
            )
            (checkout / "README").write_text("fixture\n", encoding="utf-8")
            subprocess.run(["git", "-C", str(checkout), "add", "README"], check=True)
            subprocess.run(
                ["git", "-C", str(checkout), "commit", "-qm", "fixture"], check=True
            )
            url = "https://example.invalid/upstream.git"
            subprocess.run(
                ["git", "-C", str(checkout), "remote", "add", "origin", url], check=True
            )
            revision = subprocess.run(
                ["git", "-C", str(checkout), "rev-parse", "HEAD"],
                check=True,
                capture_output=True,
                text=True,
            ).stdout.strip()
            self.assertEqual(verify_checkout(checkout, url, revision)["revision"], revision)
            with self.assertRaisesRegex(BuilderError, "unexpected origin"):
                verify_checkout(checkout, "https://example.invalid/other.git", revision)
            with self.assertRaisesRegex(BuilderError, "unexpected revision"):
                verify_checkout(checkout, url, "0" * 40)

    def test_manifest_records_exact_control_evidence(self) -> None:
        upstream = json.loads((ROOT / "pins/upstream.json").read_text(encoding="utf-8"))
        with tempfile.TemporaryDirectory() as temporary:
            fixture = pathlib.Path(temporary)
            artifact = fixture / "chromium-browser-stable_1_amd64.deb"
            artifact.write_bytes(b"deb-fixture")
            preflight = fixture / "preflight.json"
            preflight.write_text(
                json.dumps({"schema_version": 1, "passed": True}), encoding="utf-8"
            )
            version = fixture / "version.txt"
            version.write_text("Chromium 151.0.7922.169\n", encoding="utf-8")
            checkouts = {
                "chromium": {"revision": upstream["chromium"]["revision"]},
                "depot_tools": {"revision": upstream["depot_tools"]["revision"]},
            }
            manifest = build_manifest(
                ROOT,
                checkouts,
                ROOT / "build-config/chromium-control.gn",
                artifact,
                preflight,
                version,
            )
            self.assertEqual(manifest["kind"], "upstream-chromium-control")
            self.assertFalse(manifest["build"]["overlay_applied"])
            self.assertFalse(manifest["build"]["direct_patches_applied"])
            self.assertEqual(manifest["build"]["partition_allocator"], "chromium-default")
            self.assertFalse(manifest["verification"]["sandbox_bypass_used"])
            self.assertEqual(len(manifest["artifact"]["sha256"]), 64)

    def test_exported_pins_are_validated_and_line_safe(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            env_path = pathlib.Path(temporary) / "github.env"
            export_pins(ROOT, env_path)
            lines = env_path.read_text(encoding="utf-8").splitlines()
            self.assertEqual(len(lines), 6)
            self.assertTrue(all(line.count("=") == 1 for line in lines))
            self.assertTrue(any(line.startswith("CHROMIUM_REVISION=") for line in lines))


if __name__ == "__main__":
    unittest.main()
