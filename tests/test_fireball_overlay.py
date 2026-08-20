from __future__ import annotations

import copy
import hashlib
import json
import pathlib
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from fireball_overlay import (  # noqa: E402
    EXPECTED_SMOKE,
    OverlayError,
    SourceRecord,
    build_link_evidence,
    load_overlay_manifest,
    source_record,
    source_tree,
    stage_tree,
    tree_summary,
    validate_overlay,
    validate_stage_report,
)


class FireballOverlayTests(unittest.TestCase):
    def setUp(self) -> None:
        self.manifest_path = ROOT / "overlay/manifest.json"
        self.manifest = json.loads(self.manifest_path.read_text(encoding="utf-8"))

    def test_repository_overlay_manifest_is_current(self) -> None:
        manifest, records = validate_overlay(ROOT, self.manifest_path)
        self.assertEqual(manifest["tree"], tree_summary(records))
        self.assertIn("fireball/overlay_smoke.cc", {record.path for record in records})
        self.assertFalse(any("/target/" in record.path for record in records))

    def test_gn_smoke_target_depends_on_the_complete_overlay_group(self) -> None:
        build = (ROOT / "fireball/BUILD.gn").read_text(encoding="utf-8")
        self.assertIn('group("fireball_overlay")', build)
        self.assertIn('executable("overlay_smoke")', build)
        self.assertIn('deps = [ ":fireball_overlay" ]', build)
        self.assertIn('output_name = "fireball_overlay_smoke"', build)
        self.assertIn('"//fireball/chromium:chromium_adapter"', build)
        self.assertIn(
            '"//fireball/chromium:chromium_renderer_adapter"', build
        )

        adapter = (ROOT / "fireball/chromium/BUILD.gn").read_text(
            encoding="utf-8"
        )
        self.assertIn('source_set("adapter_contract")', adapter)
        self.assertIn('source_set("chromium_adapter")', adapter)
        self.assertIn('source_set("chromium_renderer_adapter")', adapter)
        self.assertIn('mojom("cosmetic_style_agent_mojom")', adapter)
        self.assertIn('"//content/public/browser"', adapter)
        self.assertIn('"//content/public/renderer"', adapter)
        self.assertIn('"fireball_url_loader_throttle.cc"', adapter)

    def test_mojom_is_an_allowlisted_overlay_source_type(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repository = pathlib.Path(temporary)
            source = repository / "fireball/interface.mojom"
            source.parent.mkdir()
            source.write_text("module fireball.mojom;\n", encoding="utf-8")
            record = source_record(
                repository, pathlib.PurePosixPath("fireball/interface.mojom")
            )
            self.assertEqual(record.path, "fireball/interface.mojom")

    def test_tree_hash_binds_path_size_and_content_digest(self) -> None:
        records = source_tree(ROOT, "fireball")
        original = tree_summary(records)
        changed = list(records)
        first = changed[0]
        changed[0] = SourceRecord(first.path, first.bytes, "0" * 64)
        self.assertNotEqual(original["sha256"], tree_summary(changed)["sha256"])

    def test_manifest_rejects_automatic_import(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = pathlib.Path(temporary) / "manifest.json"
            document = copy.deepcopy(self.manifest)
            document["automatic_import"] = True
            path.write_text(json.dumps(document), encoding="utf-8")
            with self.assertRaisesRegex(OverlayError, "automatic overlay import"):
                load_overlay_manifest(path)

    def test_source_rejects_symlink_executable_and_unknown_type(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repository = pathlib.Path(temporary)
            source = repository / "fireball/source.cc"
            source.parent.mkdir()
            source.write_text("int value = 1;\n", encoding="utf-8")
            source.chmod(0o755)
            with self.assertRaisesRegex(OverlayError, "may not be executable"):
                source_record(repository, pathlib.PurePosixPath("fireball/source.cc"))
            source.chmod(0o644)
            link = repository / "fireball/link.cc"
            link.symlink_to(source)
            with self.assertRaisesRegex(OverlayError, "non-symlink"):
                source_record(repository, pathlib.PurePosixPath("fireball/link.cc"))
            unknown = repository / "fireball/payload.bin"
            unknown.write_bytes(b"payload")
            with self.assertRaisesRegex(OverlayError, "not allowlisted"):
                source_record(repository, pathlib.PurePosixPath("fireball/payload.bin"))

    def test_stage_tree_is_exact_and_refuses_overwrite(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            repository = root / "repository"
            checkout = root / "chromium"
            (repository / "fireball/sub").mkdir(parents=True)
            checkout.mkdir()
            source = repository / "fireball/sub/value.cc"
            source.write_text("int value = 7;\n", encoding="utf-8")
            record = SourceRecord(
                path="fireball/sub/value.cc",
                bytes=source.stat().st_size,
                sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
            )
            manifest = {"root": "fireball", "destination": "fireball"}
            destination = stage_tree(repository, checkout, manifest, [record])
            self.assertEqual(
                (destination / "sub/value.cc").read_text(encoding="utf-8"),
                "int value = 7;\n",
            )
            with self.assertRaisesRegex(OverlayError, "already exists"):
                stage_tree(repository, checkout, manifest, [record])

    def test_stage_tree_rejects_record_path_traversal(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            repository = root / "repository"
            checkout = root / "chromium"
            repository.mkdir()
            checkout.mkdir()
            malicious = SourceRecord("fireball/../escape.cc", 0, hashlib.sha256(b"").hexdigest())
            with self.assertRaisesRegex(OverlayError, "unsafe staged overlay path"):
                stage_tree(
                    repository,
                    checkout,
                    {"root": "fireball", "destination": "fireball"},
                    [malicious],
                )
            self.assertFalse((root / "escape.cc").exists())

    def test_stage_report_file_records_are_cryptographically_bound(self) -> None:
        records = source_tree(ROOT, "fireball")
        report = {
            "schema_version": 1,
            "kind": "fireball-overlay-stage",
            "source_repository": self.manifest["source_repository"],
            "fireball_revision": "1" * 40,
            "chromium_revision": self.manifest["chromium_revision"],
            "destination": "//fireball",
            "architecture_order": self.manifest["architecture_order"],
            "tree": self.manifest["tree"],
            "files": [record.__dict__ for record in records],
            "overrides_applied": 0,
        }
        validate_stage_report(report, self.manifest)
        report["files"][0]["sha256"] = "0" * 64
        with self.assertRaisesRegex(OverlayError, "do not match"):
            validate_stage_report(report, self.manifest)

    def test_link_evidence_names_its_remaining_browser_limitations(self) -> None:
        records = source_tree(ROOT, "fireball")
        stage_report = {
            "schema_version": 1,
            "kind": "fireball-overlay-stage",
            "source_repository": self.manifest["source_repository"],
            "fireball_revision": "1" * 40,
            "chromium_revision": self.manifest["chromium_revision"],
            "destination": "//fireball",
            "architecture_order": self.manifest["architecture_order"],
            "tree": self.manifest["tree"],
            "files": [record.__dict__ for record in records],
            "overrides_applied": 0,
        }
        with tempfile.TemporaryDirectory() as temporary:
            fixture = pathlib.Path(temporary)
            stage_path = fixture / "stage.json"
            stage_path.write_text(json.dumps(stage_report), encoding="utf-8")
            binary = fixture / "fireball_overlay_smoke"
            binary.write_bytes(b"linked-binary")
            smoke = fixture / "smoke.json"
            smoke.write_text(json.dumps(EXPECTED_SMOKE), encoding="utf-8")
            preflight = fixture / "preflight.json"
            preflight.write_text(
                json.dumps({"schema_version": 1, "passed": True}), encoding="utf-8"
            )
            evidence = build_link_evidence(
                ROOT,
                self.manifest_path,
                stage_path,
                ROOT / "build-config/chromium-control.gn",
                binary,
                smoke,
                preflight,
            )
            self.assertEqual(evidence["build"]["target"], "//fireball:overlay_smoke")
            self.assertIn("not-a-chromium-browser-target", evidence["limitations"])
            self.assertNotIn(
                "not-a-webcontents-or-profile-adapter", evidence["limitations"]
            )
            self.assertIn(
                "profile-lifecycle-hook-not-wired", evidence["limitations"]
            )
            self.assertIn(
                "subresource-lifecycle-hook-not-wired",
                evidence["limitations"],
            )
            self.assertIn(
                "keepalive-and-prefetch-policy-not-wired",
                evidence["limitations"],
            )
            self.assertIn(
                "renderer-cosmetic-controller-bridge-not-wired",
                evidence["limitations"],
            )
            self.assertIn(
                "renderer-cosmetic-lifecycle-owner-not-activated",
                evidence["limitations"],
            )
            self.assertIn(
                "renderer-content-client-registration-not-wired",
                evidence["limitations"],
            )
            self.assertNotIn(
                "renderer-cosmetic-adapter-not-wired",
                evidence["limitations"],
            )
            self.assertEqual(len(evidence["binary"]["sha256"]), 64)


if __name__ == "__main__":
    unittest.main()
