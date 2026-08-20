from __future__ import annotations

import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
WORKFLOW = ROOT / ".github/workflows/chromium-control.yml"


class ChromiumWorkflowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.workflow = WORKFLOW.read_text(encoding="utf-8")

    def test_workflow_never_runs_for_pull_requests_or_forks(self) -> None:
        self.assertNotIn("pull_request:", self.workflow)
        self.assertIn("github.repository == 'LamPPKK/fireball-blink'", self.workflow)
        self.assertIn("github.ref == 'refs/heads/main'", self.workflow)
        self.assertIn("persist-credentials: false", self.workflow)
        self.assertIn("environment: chromium-builder", self.workflow)

    def test_workflow_requires_dedicated_builder_and_preflight(self) -> None:
        self.assertIn(
            "runs-on: [self-hosted, linux, x64, fireball-chromium]", self.workflow
        )
        self.assertIn("chromium_builder.py\" preflight", self.workflow)
        self.assertIn("FIREBALL_CHROMIUM_NIGHTLY_ENABLED", self.workflow)
        self.assertIn("timeout-minutes: 720", self.workflow)

    def test_workflow_builds_exact_unmodified_control(self) -> None:
        self.assertIn('DEPOT_TOOLS_UPDATE: "0"', self.workflow)
        self.assertIn('checkout --detach "$DEPOT_TOOLS_REVISION"', self.workflow)
        self.assertIn('checkout --detach "$CHROMIUM_REVISION"', self.workflow)
        self.assertIn("build-config/chromium-control.gn", self.workflow)
        self.assertIn("chrome/installer/linux:stable_deb", self.workflow)
        self.assertIn("verify-checkouts", self.workflow)

    def test_smoke_test_does_not_disable_the_sandbox(self) -> None:
        self.assertIn("Smoke-test with Chromium sandbox enabled", self.workflow)
        self.assertIn("--headless=new", self.workflow)
        self.assertNotIn("--no-sandbox", self.workflow)

    def test_artifact_has_manifest_checksum_and_bounded_retention(self) -> None:
        self.assertIn("chromium_builder.py\" manifest", self.workflow)
        self.assertIn("sha256sum \"$bundle\"", self.workflow)
        self.assertIn(
            "actions/upload-artifact@043fb46d1a93c77aae656e7c1c64a875d1fc6a0a",
            self.workflow,
        )
        self.assertIn("retention-days: 14", self.workflow)

    def test_control_artifact_is_completed_before_overlay_staging(self) -> None:
        control_upload = self.workflow.index("Upload exact control package and checksum")
        overlay_stage = self.workflow.index("Stage checksum-pinned Fireball overlay")
        self.assertLess(control_upload, overlay_stage)
        self.assertIn('fireball_overlay.py"', self.workflow)
        self.assertIn('--repository-root "$FIREBALL_ROOT"', self.workflow)
        self.assertIn("\n            stage", self.workflow)
        self.assertIn('fireball_patches.py" apply', self.workflow)

    def test_overlay_lane_builds_only_the_honestly_named_link_gate(self) -> None:
        self.assertIn(
            "autoninja -C out/FireballOverlay fireball:overlay_smoke", self.workflow
        )
        self.assertIn("\n            evidence", self.workflow)
        self.assertIn("Upload component-link evidence", self.workflow)
        self.assertNotIn("fireball:browser", self.workflow)

    def test_cleanup_is_scoped_to_runner_temp(self) -> None:
        self.assertIn('"$RUNNER_TEMP"/fireball-chromium-control-*', self.workflow)
        self.assertIn("refusing unsafe cleanup path", self.workflow)
        self.assertIn('"$RUNNER_TEMP"/fireball-overlay-link-*.tar', self.workflow)


if __name__ == "__main__":
    unittest.main()
