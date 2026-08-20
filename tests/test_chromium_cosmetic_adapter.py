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
        self.assertIn("BindDocument(string document_id,", mojom)
        self.assertIn("SetStylesheet(string document_id", mojom)
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
        self.assertIn("expected_document_epoch", source)
        self.assertIn("if (receiver_.is_bound())", source)
        did_create = source.index("DidCreateNewDocument()")
        get_epoch = source.index("GetDocumentEpoch(", did_create)
        self.assertIn("receiver_.reset()", source[did_create:get_epoch])
        self.assertIn("document.Token()", source)
        self.assertIn("document->Token() == *bound_document_token_", source)
        self.assertIn("WebCssOrigin::kUser", source)
        self.assertIn("BackForwardCacheAware::kPossiblyDisallow", source)
        self.assertIn("InsertStyleSheet", source)
        self.assertIn("RemoveInsertedStyleSheet", source)
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
