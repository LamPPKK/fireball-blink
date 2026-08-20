from __future__ import annotations

import json
import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class ChromiumNavigationAdapterSourceTests(unittest.TestCase):
    def test_profile_binding_owns_policy_and_cannot_be_removed_mid_navigation(
        self,
    ) -> None:
        header = (ROOT / "fireball/chromium/profile_policy_binding.h").read_text(
            encoding="utf-8"
        )
        implementation = (
            ROOT / "fireball/chromium/profile_policy_binding.cc"
        ).read_text(encoding="utf-8")
        self.assertIn("public base::SupportsUserData::Data", header)
        self.assertIn("std::unique_ptr<NavigationPolicyEvaluator> evaluator_", header)
        self.assertIn("browser_context.SetUserData", implementation)
        self.assertIn("browser_context.IsOffTheRecord()", implementation)
        self.assertIn("evaluator->ExpectedProxyRules()", implementation)
        self.assertNotIn("RemoveUserData", header + implementation)

    def test_cleaned_navigation_preserves_metadata_and_posts_asynchronously(
        self,
    ) -> None:
        source = (
            ROOT / "fireball/chromium/fireball_navigation_throttle.cc"
        ).read_text(encoding="utf-8")
        self.assertIn("OpenURLParams::FromNavigationHandle", source)
        self.assertIn("GetWeakPtr()", source)
        self.assertIn("PostTask", source)
        self.assertIn("return CANCEL_AND_IGNORE", source)
        self.assertIn("return BLOCK_REQUEST", source)
        self.assertNotIn("std::cout", source)

    def test_adapter_is_compiled_but_not_activated_before_lifecycle_gate(
        self,
    ) -> None:
        root_build = (ROOT / "fireball/BUILD.gn").read_text(encoding="utf-8")
        self.assertIn('"//fireball/chromium:chromium_adapter"', root_build)
        patch_manifest = json.loads(
            (ROOT / "patches/manifest.json").read_text(encoding="utf-8")
        )
        self.assertEqual(patch_manifest["patches"], [])
        self.assertEqual(
            list((ROOT / "chromium_src").glob("**/*")),
            [ROOT / "chromium_src/README.md"],
        )

    def test_subresource_redirects_marshal_back_to_profile_sequence(self) -> None:
        header = (
            ROOT / "fireball/chromium/fireball_url_loader_throttle.h"
        ).read_text(encoding="utf-8")
        source = (
            ROOT / "fireball/chromium/fireball_url_loader_throttle.cc"
        ).read_text(encoding="utf-8")
        self.assertIn("request.request_initiator", source)
        self.assertIn("SameDomainOrHost", source)
        self.assertIn("INCLUDE_PRIVATE_REGISTRIES", source)
        self.assertIn("EvaluateSubresource", source)
        self.assertIn("*defer = true", source)
        self.assertIn("policy_task_runner_->PostTask", source)
        self.assertIn("reply_task_runner->PostTask", source)
        self.assertIn("CancelWithError", source)
        self.assertIn("WeakPtrFactory<FireballURLLoaderThrottle>", header)
        self.assertNotIn("std::cout", source)

    def test_profile_bundle_owns_and_retires_egress_backend(self) -> None:
        header = (
            ROOT / "fireball/chromium/profile_request_policy_bundle.h"
        ).read_text(encoding="utf-8")
        source = (
            ROOT / "fireball/chromium/profile_request_policy_bundle.cc"
        ).read_text(encoding="utf-8")
        self.assertIn("std::unique_ptr<egress::EgressBackend>", header)
        self.assertIn("std::unique_ptr<adblock::NetworkEvaluator>", header)
        self.assertIn("egress_controller_.RemoveProfile(profile_id_)", source)
        self.assertIn("PROFILE_POLICY_BOUNDARY_MISMATCH", source)


if __name__ == "__main__":
    unittest.main()
