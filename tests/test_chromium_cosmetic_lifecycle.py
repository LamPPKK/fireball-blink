from __future__ import annotations

import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class ChromiumCosmeticLifecycleSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.host_header = (
            ROOT / "fireball/chromium/fireball_cosmetic_document_host.h"
        ).read_text(encoding="utf-8")
        cls.host_source = (
            ROOT / "fireball/chromium/fireball_cosmetic_document_host.cc"
        ).read_text(encoding="utf-8")
        cls.owner_header = (
            ROOT / "fireball/chromium/fireball_cosmetic_lifecycle_owner.h"
        ).read_text(encoding="utf-8")
        cls.owner_source = (
            ROOT / "fireball/chromium/fireball_cosmetic_lifecycle_owner.cc"
        ).read_text(encoding="utf-8")

    def test_document_identity_uses_chromium_document_lifetime(self) -> None:
        self.assertIn("DocumentUserData<", self.host_header)
        self.assertIn("DOCUMENT_USER_DATA_KEY_DECL", self.host_header)
        self.assertIn("CreateForCurrentDocument", self.owner_source)
        self.assertIn("GenerateRandomV4", self.owner_source)
        self.assertIn("GetWeakDocumentPtr", self.owner_source)
        self.assertIn("DeleteForCurrentDocument", self.owner_source)

    def test_owner_uses_primary_page_and_lifecycle_callbacks(self) -> None:
        self.assertIn("PrimaryPageChanged", self.owner_header)
        self.assertIn("page.GetMainDocument()", self.owner_source)
        self.assertIn("page.IsPrimary()", self.owner_source)
        self.assertIn("RenderFrameHostStateChanged", self.owner_header)
        self.assertIn("LifecycleState::kActive", self.owner_source)
        self.assertIn("PrimaryMainFrameRenderProcessGone", self.owner_header)
        self.assertNotIn("DidFinishNavigation", self.owner_header + self.owner_source)
        self.assertNotIn("weak_factory_.InvalidateWeakPtrs()", self.owner_source)
        self.assertIn("active_document_id_", self.owner_header)
        self.assertIn("lifecycle_generation_", self.owner_header)
        self.assertIn("if (!alive", self.owner_source)

    def test_bfcache_suspends_and_rebinds_without_raw_frame_retention(self) -> None:
        self.assertIn("host->Suspend()", self.owner_source)
        self.assertIn("state_.Suspend()", self.host_source)
        self.assertIn("transport->Invalidate()", self.host_source)
        self.assertIn("host->Activate", self.owner_source)
        self.assertNotIn("RenderFrameHost* render_frame_host_", self.host_header)
        renderer_state = (
            ROOT / "fireball/chromium/renderer_cosmetic_style_state.cc"
        ).read_text(encoding="utf-8")
        self.assertIn("if (*document_id_ != document_id)", renderer_state)

    def test_crash_drops_old_document_before_failure_notification(self) -> None:
        suspend = self.owner_source.index("host->Suspend()")
        delete = self.owner_source.index("DeleteForCurrentDocument(frame)")
        notify = self.owner_source.index(
            '"COSMETIC_RENDERER_PROCESS_GONE"'
        )
        self.assertLess(suspend, delete)
        self.assertLess(delete, notify)

    def test_same_rfh_navigation_finalizes_prior_document(self) -> None:
        change = self.owner_source.index(
            "void FireballCosmeticLifecycleOwner::PrimaryPageChanged"
        )
        suspend = self.owner_source.index("old_host->Suspend()", change)
        notify = self.owner_source.index(
            "OnCosmeticDocumentSuspended(old_document_id)", change
        )
        create = self.owner_source.index("CreateForCurrentDocument", change)
        self.assertLess(suspend, notify)
        self.assertLess(notify, create)
        self.assertIn(
            "primary_document.AsRenderFrameHostIfValid()", self.owner_source
        )

    def test_crash_revalidates_document_after_reentrant_suspend(self) -> None:
        crash = self.owner_source.index(
            "void FireballCosmeticLifecycleOwner::"
            "PrimaryMainFrameRenderProcessGone"
        )
        crash_source = self.owner_source[crash:]
        capture = crash_source.index("frame->GetWeakDocumentPtr()")
        suspend = crash_source.index("host->Suspend()")
        resolve = crash_source.index(
            "crashed_document.AsRenderFrameHostIfValid()"
        )
        delete = crash_source.index("DeleteForCurrentDocument(frame)")
        self.assertLess(capture, suspend)
        self.assertLess(suspend, resolve)
        self.assertLess(resolve, delete)

    def test_lifecycle_target_is_compile_gated_not_activated(self) -> None:
        build = (ROOT / "fireball/chromium/BUILD.gn").read_text(encoding="utf-8")
        self.assertIn('"browser_cosmetic_document_state.cc"', build)
        self.assertIn('"fireball_cosmetic_document_host.cc"', build)
        self.assertIn('"fireball_cosmetic_lifecycle_owner.cc"', build)
        self.assertNotIn("FireballCosmeticLifecycleOwner", (
            ROOT / "fireball/overlay_smoke.cc"
        ).read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
