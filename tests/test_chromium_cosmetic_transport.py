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
        self.assertIn("state_.binding_generation()", self.source)
        self.assertIn("BrowserCosmeticTransportTicket", self.header)
        self.assertIn("state_.IsCurrent(ticket)", self.source)
        mojom = (
            ROOT / "fireball/chromium/cosmetic_style_agent.mojom"
        ).read_text(encoding="utf-8")
        self.assertIn("expected_document_epoch", mojom)
        self.assertIn("expected_binding_generation", mojom)
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

    def test_dom_snapshot_uses_typed_bounded_transport_phase(self) -> None:
        self.assertIn("CollectDomSnapshot(DomSnapshotCallback callback)", self.header)
        self.assertIn("CancelDomSnapshot()", self.header)
        self.assertIn("state_.BeginDomCollection()", self.source)
        self.assertIn("state_.CompleteDomCollection", self.source)
        self.assertIn("DecodeCosmeticDomSnapshot", self.source)
        self.assertIn("IsZeroedCosmeticDomWirePayload", self.source)
        self.assertIn("mojo::ReportBadMessage", self.source)
        self.assertIn("COSMETIC_DOM_LIMIT_EXCEEDED", self.source)
        self.assertIn("CosmeticTransportStatus::kLimited", self.source)
        self.assertIn("COSMETIC_DOM_SNAPSHOT_REJECTED", self.source)
        self.assertIn("pending_dom_snapshot_callback_", self.header)
        self.assertIn("kCollectingDom", self.state)
        callback = self.source.index(
            "void FireballCosmeticStyleTransport::OnDomSnapshotCollected"
        )
        mutation = self.source.index(
            "void FireballCosmeticStyleTransport::OnMutationCompleted", callback
        )
        section = self.source[callback:mutation]
        decode = section.index("DecodeCosmeticDomSnapshot")
        bad_message = section.index("mojo::ReportBadMessage", decode)
        stale_return = section.index(
            "if (!state_.IsCurrent(ticket))", bad_message
        )
        self.assertLess(decode, bad_message)
        self.assertLess(bad_message, stale_return)

    def test_gn_compiles_transport_against_browser_and_mojo_apis(self) -> None:
        build = (ROOT / "fireball/chromium/BUILD.gn").read_text(encoding="utf-8")
        self.assertIn('"browser_cosmetic_transport_state.cc"', build)
        self.assertIn('"fireball_cosmetic_style_transport.cc"', build)
        self.assertIn('":cosmetic_style_agent_mojom"', build)
        self.assertIn('"//content/public/browser"', build)
        self.assertIn('"//mojo/public/cpp/bindings"', build)


if __name__ == "__main__":
    unittest.main()
