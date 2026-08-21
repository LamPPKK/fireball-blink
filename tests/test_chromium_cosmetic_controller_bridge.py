from __future__ import annotations

import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class ChromiumCosmeticControllerBridgeSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.header = (
            ROOT / "fireball/chromium/fireball_cosmetic_controller_bridge.h"
        ).read_text(encoding="utf-8")
        cls.source = (
            ROOT / "fireball/chromium/fireball_cosmetic_controller_bridge.cc"
        ).read_text(encoding="utf-8")
        cls.host_header = (
            ROOT / "fireball/chromium/fireball_cosmetic_document_host.h"
        ).read_text(encoding="utf-8")
        cls.host_source = (
            ROOT / "fireball/chromium/fireball_cosmetic_document_host.cc"
        ).read_text(encoding="utf-8")
        cls.tab_binding_header = (
            ROOT / "fireball/chromium/tab_web_contents_binding.h"
        ).read_text(encoding="utf-8")
        cls.tab_binding_source = (
            ROOT / "fireball/chromium/tab_web_contents_binding.cc"
        ).read_text(encoding="utf-8")
        cls.profile_binding_source = (
            ROOT / "fireball/chromium/profile_policy_binding.cc"
        ).read_text(encoding="utf-8")

    def test_bridge_owns_lifecycle_and_checks_profile_tab_boundary(self) -> None:
        self.assertIn("public FireballCosmeticLifecycleDelegate", self.header)
        self.assertIn("ProfilePolicyBinding::Get", self.source)
        self.assertIn("binding->profile_id() != profile_id", self.source)
        self.assertIn("TabWebContentsBinding::FromWebContents", self.source)
        self.assertIn("AcquireCosmeticController", self.source)
        self.assertIn("OwnsCosmeticController(tab_claim_)", self.source)
        self.assertIn("CanActivateCosmeticDocument", self.source)
        self.assertIn("ProfileOwnsTab()", self.source)
        self.assertIn("FindTab(tab_id_)", self.source)
        self.assertIn("FindSpace(tab->space_id)", self.source)
        self.assertIn(
            "std::make_unique<FireballCosmeticLifecycleOwner>", self.source
        )
        self.assertIn("lifecycle_owner_.reset()", self.source)
        self.assertIn("lifecycle_owner_->Start()", self.source)

    def test_tab_binding_is_unique_and_profile_authoritative(self) -> None:
        self.assertIn("WebContentsUserData<TabWebContentsBinding>", self.tab_binding_header)
        self.assertIn("RegisterTabContents", self.profile_binding_source)
        self.assertIn("UnregisterTabContents", self.profile_binding_source)
        self.assertIn("OwnsTabContents", self.tab_binding_source)
        self.assertIn("controller_claimed_", self.tab_binding_header)
        self.assertIn("claim == claim_generation_", self.tab_binding_source)

    def test_policy_uses_only_committed_active_http_document(self) -> None:
        self.assertIn("AsRenderFrameHostIfValid()", self.source)
        self.assertIn("frame->IsInPrimaryMainFrame()", self.source)
        self.assertIn("frame->IsActive()", self.source)
        self.assertIn("frame->GetLastCommittedURL()", self.source)
        self.assertIn("url.SchemeIsHTTPOrHTTPS()", self.source)
        self.assertIn(
            "policy_->BeginDocument(profile_id_, url.spec(), url.host())",
            self.source,
        )

    def test_document_state_commits_only_after_renderer_ack(self) -> None:
        ready = self.source.index("OnCosmeticDocumentReady(")
        apply_call = self.source.index("host.SetStylesheet(", ready)
        callback = self.source.index("OnDocumentStylesheetApplied", apply_call)
        callback_body = self.source.index(
            "void FireballCosmeticControllerBridge::"
            "OnDocumentStylesheetApplied"
        )
        complete = self.source.index("state_.CompleteActivation", callback_body)
        discard_css = self.source.index("plan.stylesheet.clear()", complete)
        emplace = self.source.index("documents_.emplace", complete)
        self.assertLess(apply_call, callback)
        self.assertLess(callback, callback_body)
        self.assertLess(callback_body, complete)
        self.assertLess(complete, discard_css)
        self.assertLess(discard_css, emplace)
        self.assertIn(
            "transport_result.status == CosmeticTransportStatus::kApplied",
            self.source[callback_body:],
        )

    def test_generic_revision_and_revocation_are_acknowledged(self) -> None:
        self.assertIn(
            "BeginGenericMutation(document_id, snapshot.revision)", self.source
        )
        self.assertIn(
            "IsBoundedCosmeticDomSnapshot(snapshot.classes, snapshot.ids)",
            self.source,
        )
        self.assertIn("COSMETIC_SNAPSHOT_INVALID", self.source)
        self.assertIn("COSMETIC_SNAPSHOT_STALE", self.source)
        self.assertIn("OnGenericStylesheetApplied", self.source)
        self.assertIn("CompleteGenericMutation(ticket, accepted)", self.source)
        self.assertIn("BeginRevocation(document_id)", self.source)
        self.assertIn("host->Revoke", self.source)
        self.assertIn("CompleteRevocation(ticket, accepted)", self.source)
        self.assertIn("documents_.erase(ticket.document_id)", self.source)
        self.assertIn("RefreshActiveDocument()", self.source)
        self.assertIn("OnDocumentRevokedForRefresh", self.source)
        self.assertIn("OnHostReactivatedForRefresh", self.source)
        self.assertIn("PrepareReadyDocument(ticket, *host)", self.source)
        self.assertGreaterEqual(self.source.count("if (!ContextValid())"), 7)

    def test_renderer_snapshot_is_collected_automatically_and_not_public_input(self) -> None:
        public = self.header.index("public:")
        private = self.header.index("private:", public)
        public_section = self.header[public:private]
        self.assertIn("bool RefreshDomSnapshot();", public_section)
        self.assertNotIn("std::vector<std::string> classes", public_section)
        self.assertNotIn("ApplyDomSnapshot", public_section)
        self.assertIn("host->CollectDomSnapshot", self.source)
        self.assertIn("CosmeticTransportStatus::kCollected", self.source)
        self.assertIn("ScheduleDomCollection(tracked->first)", self.source)
        self.assertIn("ScheduleDomCollection(document_id)", self.source)
        schedule = self.source.index("ScheduleDomCollection(tracked->first)")
        publish = self.source.index(
            "Publish(tracked->first, tracked->second.last_result)", schedule
        )
        self.assertLess(schedule, publish)

    def test_dom_collection_fails_closed_when_context_or_transport_is_lost(self) -> None:
        start = self.source.index(
            "bool FireballCosmeticControllerBridge::StartDomCollection"
        )
        callback = self.source.index(
            "void FireballCosmeticControllerBridge::OnDomSnapshotCollected",
            start,
        )
        reset_document = self.source.index(
            "void FireballCosmeticControllerBridge::ResetDocument", callback
        )
        start_section = self.source[start:callback]
        callback_section = self.source[callback:reset_document]

        start_context = start_section.index("if (!ContextValid())")
        start_reset = start_section.index("ResetAllDocuments();", start_context)
        start_state = start_section.index("if (!state_.ready()", start_reset)
        self.assertLess(start_context, start_reset)
        self.assertLess(start_reset, start_state)

        callback_context = callback_section.index("if (!ContextValid())")
        callback_reset = callback_section.index(
            "ResetAllDocuments();", callback_context
        )
        callback_identity = callback_section.index(
            "if (!state_.IsCurrent(ticket))", callback_reset
        )
        inactive_host = callback_section.index(
            "if (ResolveHost(document_id, document) == nullptr)",
            callback_identity,
        )
        reset_inactive_document = callback_section.index(
            "ResetDocument(document_id, document);", inactive_host
        )
        self.assertLess(callback_context, callback_reset)
        self.assertLess(callback_reset, callback_identity)
        self.assertLess(callback_identity, inactive_host)
        self.assertLess(inactive_host, reset_inactive_document)

        limit = callback_section.index(
            "CosmeticTransportStatus::kLimited", reset_inactive_document
        )
        error = callback_section.index(
            "transport_result.status != CosmeticTransportStatus::kCollected",
            limit,
        )
        reset_failed_document = callback_section.index(
            "ResetDocument(document_id, document);", error
        )
        self.assertLess(limit, error)
        self.assertLess(error, reset_failed_document)
        self.assertIn("state_.BeginDomCollection(document_id)", start_section)
        self.assertIn("ClearGenericStylesheetAfterLimit", callback_section)
        self.assertIn("OnGenericStylesheetClearedAfterLimit", callback_section)
        self.assertIn("CancelDomCollection(document_id", self.source)
        self.assertIn("host.CancelDomSnapshot()", self.source)

    def test_revoke_and_refresh_cancel_dom_collection_before_mutation(self) -> None:
        revoke = self.source.index(
            "bool FireballCosmeticControllerBridge::RevokeActiveDocument"
        )
        refresh = self.source.index(
            "bool FireballCosmeticControllerBridge::RefreshActiveDocument",
            revoke,
        )
        activation = self.source.index(
            "bool FireballCosmeticControllerBridge::CanActivateCosmeticDocument",
            refresh,
        )
        revoke_section = self.source[revoke:refresh]
        refresh_section = self.source[refresh:activation]
        for section in (revoke_section, refresh_section):
            cancel = section.index("CancelDomCollection(document_id")
            ready = section.index("if (!state_.ready()", cancel)
            begin = section.index("state_.BeginRevocation(document_id)", ready)
            self.assertLess(cancel, ready)
            self.assertLess(ready, begin)

        cancel_helper = self.source.index(
            "bool FireballCosmeticControllerBridge::CancelDomCollection"
        )
        callback = self.source.index(
            "void FireballCosmeticControllerBridge::OnDomSnapshotCollected",
            cancel_helper,
        )
        cancel_section = self.source[cancel_helper:callback]
        host_cancel = cancel_section.index("host.CancelDomSnapshot()")
        state_cancel = cancel_section.index("state_.CancelDomCollection", host_cancel)
        cleanup = cancel_section.index("ResetDocument(document_id, document)")
        self.assertLess(host_cancel, state_cancel)
        self.assertLess(state_cancel, cleanup)

    def test_suspended_document_snapshot_cannot_revoke_active_document(self) -> None:
        begin = self.source.index("bool FireballCosmeticControllerBridge::ApplyDomSnapshot")
        end = self.source.index("bool FireballCosmeticControllerBridge::RevokeActiveDocument", begin)
        section = self.source[begin:end]
        active_identity = section.index("*state_.active_document_id() != document_id")
        active_host = section.index("ResolveHost(document_id, tracked->second.document)")
        disabled_early_return = section.index(
            "!tracked->second.plan.generic_scan_allowed"
        )
        policy = section.index("policy_->MatchGenericSelectors")
        self.assertLess(active_identity, active_host)
        self.assertLess(active_host, disabled_early_return)
        self.assertLess(active_host, policy)

    def test_failed_state_can_reset_revoke_or_rebind(self) -> None:
        self.assertIn("BrowserCosmeticControllerPhase::kFailed", self.source)
        self.assertIn("host->ResetForController()", self.source)
        self.assertIn("state_.Reset(document_id)", self.source)
        self.assertIn("host->Activate", self.source)
        self.assertIn("void FireballCosmeticDocumentHost::ResetForController", self.host_source)

    def test_teardown_clears_active_and_bfcache_hosts(self) -> None:
        destructor = self.source.index(
            "FireballCosmeticControllerBridge::~FireballCosmeticControllerBridge"
        )
        initialize = self.source.index(
            "bool FireballCosmeticControllerBridge::Initialize", destructor
        )
        body = self.source[destructor:initialize]
        self.assertIn("weak_factory_.InvalidateWeakPtrs()", body)
        self.assertIn("host->ResetForController()", body)
        self.assertIn("lifecycle_owner_->Shutdown()", body)
        self.assertIn("ReleaseCosmeticController(tab_claim_)", body)

    def test_invalid_binding_clears_every_tracked_document(self) -> None:
        reset = self.source.index(
            "void FireballCosmeticControllerBridge::ResetAllDocuments"
        )
        resolve = self.source.index(
            "FireballCosmeticDocumentHost *FireballCosmeticControllerBridge::ResolveHost",
            reset,
        )
        section = self.source[reset:resolve]
        move = section.index("std::move(documents_)")
        clear = section.index("documents_.clear()", move)
        reset_state = section.index("state_.ResetAll()", clear)
        invalidate = section.index("weak_factory_.InvalidateWeakPtrs()", reset_state)
        loop = section.index(
            "for (const auto &[document_id, tracked] : tracked_documents)",
            invalidate,
        )
        reset_host = section.index("host->ResetForController()", loop)
        self.assertLess(move, clear)
        self.assertLess(clear, reset_state)
        self.assertLess(reset_state, invalidate)
        self.assertLess(invalidate, loop)
        self.assertLess(loop, reset_host)
        self.assertIn("host->ResetForController()", section)
        self.assertIn("lifecycle_owner_->active_document_id()", section)
        self.assertIn("lifecycle_document.AsRenderFrameHostIfValid()", section)
        self.assertGreaterEqual(self.source.count("ResetAllDocuments();"), 10)

    def test_terminal_documents_are_removed_from_tracking(self) -> None:
        disposed = self.source.index("OnCosmeticDocumentDisposed(")
        failed = self.source.index("OnCosmeticDocumentFailed(", disposed)
        section = self.source[disposed:failed]
        self.assertIn("state_.Dispose(document_id)", section)
        self.assertIn("documents_.erase(document_id)", section)

    def test_bfcache_restore_replays_document_then_rescans_generic(self) -> None:
        binding = self.host_source.index("state_.CompleteBinding")
        restore = self.host_source.index("RestoreDesiredStyles", binding)
        document = self.host_source.index(
            "CosmeticStyleLayer::kDocument", restore
        )
        complete = self.host_source.index("state_.CompleteRestore", document)
        self.assertLess(binding, restore)
        self.assertLess(restore, document)
        self.assertLess(document, complete)
        self.assertNotIn("RestoreDesiredGenericStyle", self.host_source)
        self.assertIn("desired_document_stylesheet_", self.host_header)
        self.assertIn("desired_generic_stylesheet_", self.host_header)
        self.assertIn("result.status == CosmeticTransportStatus::kApplied", self.host_source)
        self.assertIn("desired_document_stylesheet_.clear()", self.host_source)
        self.assertIn("desired_generic_stylesheet_.clear()", self.host_source)

        prepare = self.source.index(
            "void FireballCosmeticControllerBridge::PrepareReadyDocument"
        )
        schedule = self.source.index("ScheduleDomCollection(document_id)", prepare)
        reset_result = self.source.index(
            "tracked->second.last_result =", prepare, schedule
        )
        document_only_count = self.source.index(
            "tracked->second.plan.hidden_selector_count", reset_result, schedule
        )
        publish = self.source.index("Publish(document_id, tracked->second.last_result)", schedule)
        self.assertLess(reset_result, document_only_count)
        self.assertLess(document_only_count, schedule)
        self.assertLess(schedule, publish)

    def test_bridge_is_compile_gated_but_not_constructed_by_chrome(self) -> None:
        build = (ROOT / "fireball/chromium/BUILD.gn").read_text(encoding="utf-8")
        smoke = (ROOT / "fireball/overlay_smoke.cc").read_text(encoding="utf-8")
        self.assertIn('"fireball_cosmetic_controller_bridge.cc"', build)
        self.assertIn('"browser_cosmetic_controller_state.cc"', build)
        self.assertNotIn("FireballCosmeticControllerBridge", smoke)
        for forbidden in (
            "ExecuteJavaScript",
            "WebScriptSource",
            "innerHTML",
            "document.write",
        ):
            self.assertNotIn(forbidden, self.header + self.source)


if __name__ == "__main__":
    unittest.main()
