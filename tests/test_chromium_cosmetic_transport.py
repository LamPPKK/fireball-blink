from __future__ import annotations

import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class ChromiumCosmeticTransportSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.header = (
            ROOT / "fireball/chromium/fireball_cosmetic_style_transport.h"
        ).read_text(encoding="utf-8")
        cls.source = (
            ROOT / "fireball/chromium/fireball_cosmetic_style_transport.cc"
        ).read_text(encoding="utf-8")
        cls.state = (
            ROOT / "fireball/chromium/browser_cosmetic_transport_state.cc"
        ).read_text(encoding="utf-8")

    def test_transport_is_bound_to_one_live_primary_document(self) -> None:
        self.assertIn("GetWeakDocumentPtr()", self.source)
        self.assertIn("AsRenderFrameHostIfValid()", self.source)
        self.assertIn("IsInPrimaryMainFrame()", self.source)
        self.assertIn("IsActive()", self.source)
        self.assertIn("GetRemoteAssociatedInterfaces()", self.source)
        self.assertIn("AssociatedRemote<", self.header)

    def test_epoch_and_generation_cross_every_async_boundary(self) -> None:
        self.assertIn("GetDocumentEpoch", self.source)
        self.assertIn("AcceptDocumentEpoch", self.source)
        self.assertIn("state_.document_epoch()", self.source)
        self.assertIn("BrowserCosmeticTransportTicket", self.header)
        self.assertIn("state_.IsCurrent(ticket)", self.source)
        self.assertIn("expected_document_epoch", (
            ROOT / "fireball/chromium/cosmetic_style_agent.mojom"
        ).read_text(encoding="utf-8"))
        self.assertIn("generation_", self.state)

    def test_transport_is_async_fail_closed_and_script_free(self) -> None:
        self.assertIn("OnceCallback", self.header)
        self.assertIn("set_disconnect_handler", self.source)
        self.assertIn("COSMETIC_TRANSPORT_DISCONNECTED", self.source)
        self.assertIn("COSMETIC_TRANSPORT_DOCUMENT_INACTIVE", self.source)
        self.assertIn("IsValidCompiledCosmeticStylesheet", self.source)
        self.assertIn("weak_factory_.InvalidateWeakPtrs()", self.source)
        for forbidden in (
            "RunLoop",
            "WaitForIncomingResponse",
            "ExecuteJavaScript",
            "WebScriptSource",
            "innerHTML",
            "document.write",
        ):
            self.assertNotIn(forbidden, self.header + self.source)

    def test_gn_compiles_transport_against_browser_and_mojo_apis(self) -> None:
        build = (ROOT / "fireball/chromium/BUILD.gn").read_text(encoding="utf-8")
        self.assertIn('"browser_cosmetic_transport_state.cc"', build)
        self.assertIn('"fireball_cosmetic_style_transport.cc"', build)
        self.assertIn('":cosmetic_style_agent_mojom"', build)
        self.assertIn('"//content/public/browser"', build)
        self.assertIn('"//mojo/public/cpp/bindings"', build)


if __name__ == "__main__":
    unittest.main()
