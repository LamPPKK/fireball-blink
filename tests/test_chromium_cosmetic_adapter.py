from __future__ import annotations

import json
import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class ChromiumCosmeticAdapterSourceTests(unittest.TestCase):
    def test_renderer_endpoint_is_typed_and_script_free(self) -> None:
        mojom = (
            ROOT / "fireball/chromium/cosmetic_style_agent.mojom"
        ).read_text(encoding="utf-8")
        self.assertIn("interface CosmeticStyleAgent", mojom)
        self.assertIn("GetDocumentEpoch()", mojom)
        self.assertIn("uint64 expected_document_epoch", mojom)
        self.assertIn("uint64 binding_generation", mojom)
        self.assertIn("uint64 expected_binding_generation", mojom)
        self.assertIn("BindDocument(string document_id,", mojom)
        self.assertIn("SetStylesheet(string document_id", mojom)
        self.assertIn("CollectDomSnapshot(string document_id", mojom)
        self.assertIn("kCosmeticDomSnapshotWireBytes = 270336", mojom)
        self.assertIn(
            "array<uint8, kCosmeticDomSnapshotWireBytes> payload", mojom
        )
        self.assertNotIn("array<string>", mojom)
        self.assertIn("RemoveDocumentStyles(string document_id,", mojom)
        for forbidden in ("javascript", "html", "url", "selector", "profile"):
            self.assertNotIn(f"string {forbidden}", mojom.lower())

    def test_agent_uses_blink_user_stylesheet_api_and_document_identity(self) -> None:
        header = (
            ROOT / "fireball/chromium/fireball_cosmetic_style_agent.h"
        ).read_text(encoding="utf-8")
        source = (
            ROOT / "fireball/chromium/fireball_cosmetic_style_agent.cc"
        ).read_text(encoding="utf-8")
        combined = header + source
        self.assertIn("AssociatedReceiver<", header)
        self.assertNotIn("AssociatedReceiverSet", header)
        self.assertIn("std::optional<blink::DocumentToken>", header)
        self.assertIn("void DidCreateNewDocument() override", header)
        self.assertIn("state_.BeginDocument()", source)
        self.assertIn("state_.document_epoch()", source)
        self.assertIn("state_.binding_generation()", source)
        self.assertIn("expected_document_epoch", source)
        self.assertIn("expected_binding_generation", source)
        self.assertIn("RemoveBoundStylesAndSuspend()", source)
        self.assertIn("OnReceiverDisconnected()", source)
        self.assertIn("state_.SuspendBinding()", source)
        self.assertIn("if (receiver_.is_bound())", source)
        bind_receiver = source.index("void FireballCosmeticStyleAgent::BindReceiver")
        disconnected = source.index(
            "void FireballCosmeticStyleAgent::OnReceiverDisconnected",
            bind_receiver,
        )
        bind_source = source[bind_receiver:disconnected]
        self.assertLess(
            bind_source.index("RemoveBoundStylesAndSuspend()"),
            bind_source.index("receiver_.reset()"),
        )
        disconnect_source = source[disconnected:]
        self.assertIn("RemoveBoundStylesAndSuspend()", disconnect_source)
        did_create = source.index("DidCreateNewDocument()")
        get_epoch = source.index("GetDocumentEpoch(", did_create)
        self.assertIn("receiver_.reset()", source[did_create:get_epoch])
        self.assertIn("document.Token()", source)
        self.assertIn("document->Token() == *bound_document_token_", source)
        self.assertIn("WebCssOrigin::kUser", source)
        self.assertIn("BackForwardCacheAware::kPossiblyDisallow", source)
        self.assertIn("InsertStyleSheet", source)
        self.assertIn("RemoveInsertedStyleSheet", source)
        self.assertIn("document.All()", source)
        self.assertNotIn("elements.length()", source)
        self.assertIn("elements.FirstItem()", source)
        self.assertIn("elements.NextItem()", source)
        collection = source.index("blink::WebElementCollection elements = document.All()")
        loop = source.index("for (blink::WebElement element", collection)
        collection_setup = source[collection:loop]
        self.assertIn("if (!elements)", collection_setup)
        self.assertIn("ReplyDomSnapshotRejected", collection_setup)
        quota = source.index(
            "scanned_element_count == kMaximumCosmeticDomElements", loop
        )
        read_id = source.index("element.GetIdAttribute()", quota)
        self.assertLess(quota, read_id)
        builder_limit = source.index("if (!builder.AddElement", loop)
        builder_limit_result = source.index(
            "ReplyDomSnapshotLimited(*revision", builder_limit
        )
        self.assertLess(builder_limit, builder_limit_result)
        self.assertIn("CosmeticDomSnapshotBuilder", source)
        snapshot_header = (
            ROOT / "fireball/chromium/cosmetic_dom_snapshot.h"
        ).read_text(encoding="utf-8")
        self.assertIn("kMaximumCosmeticDomElements", snapshot_header)
        self.assertIn("kMaximumCosmeticDomAttributeCodeUnits", source)
        self.assertIn("Utf8ConversionMode::kStrict", source)
        self.assertIn("state_.NextDomSnapshotRevision", source)
        self.assertIn("void DidFinishLoad() override", header)
        self.assertIn("document.IsLoaded()", source)
        self.assertIn("kMaximumDomLoadWait", source)
        self.assertIn("base::Seconds(5)", source)
        self.assertIn("CompletePendingDomSnapshot", source)
        self.assertIn("CancelPendingDomSnapshot", source)
        self.assertIn("EncodeCosmeticDomSnapshot", source)
        remove = source.index(
            "void FireballCosmeticStyleAgent::RemoveDocumentStyles"
        )
        bind_receiver = source.index(
            "void FireballCosmeticStyleAgent::BindReceiver", remove
        )
        remove_section = source[remove:bind_receiver]
        validate = remove_section.index("state_.PrepareMutation")
        rejected = remove_section.index("if (IsRejected(probe))", validate)
        cancel = remove_section.index("CancelPendingDomSnapshot();", rejected)
        self.assertLess(validate, rejected)
        self.assertLess(rejected, cancel)
        self.assertIn("IsValidCompiledCosmeticStylesheet", (
            ROOT / "fireball/chromium/renderer_cosmetic_style_state.cc"
        ).read_text(encoding="utf-8"))
        for forbidden in (
            "ExecuteJavaScript",
            "WebScriptSource",
            "innerHTML",
            "document.write",
            "ExecuteScript",
        ):
            self.assertNotIn(forbidden, combined)

    def test_new_style_is_committed_before_previous_key_is_removed(self) -> None:
        source = (
            ROOT / "fireball/chromium/fireball_cosmetic_style_agent.cc"
        ).read_text(encoding="utf-8")
        insert = source.index("document.InsertStyleSheet")
        commit = source.index("state_.CommitMutation(mutation)", insert)
        remove_previous = source.index("if (!mutation.previous_key.empty())", commit)
        self.assertLess(insert, commit)
        self.assertLess(commit, remove_previous)

    def test_renderer_target_is_compile_gated_but_not_activated(self) -> None:
        chromium_build = (ROOT / "fireball/chromium/BUILD.gn").read_text(
            encoding="utf-8"
        )
        root_build = (ROOT / "fireball/BUILD.gn").read_text(encoding="utf-8")
        self.assertIn('mojom("cosmetic_style_agent_mojom")', chromium_build)
        self.assertIn('source_set("chromium_renderer_adapter")', chromium_build)
        self.assertIn('"cosmetic_dom_snapshot.cc"', chromium_build)
        self.assertIn('"//content/public/renderer"', chromium_build)
        self.assertIn('"//third_party/blink/public:blink"', chromium_build)
        self.assertIn(
            '"//fireball/chromium:chromium_renderer_adapter"', root_build
        )
        patch_manifest = json.loads(
            (ROOT / "patches/manifest.json").read_text(encoding="utf-8")
        )
        self.assertEqual(patch_manifest["patches"], [])
        self.assertEqual(
            list((ROOT / "chromium_src").glob("**/*")),
            [ROOT / "chromium_src/README.md"],
        )


if __name__ == "__main__":
    unittest.main()
