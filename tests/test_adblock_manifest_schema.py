from __future__ import annotations

import json
import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class AdblockManifestSchemaTests(unittest.TestCase):
    def setUp(self) -> None:
        self.schema = json.loads(
            (ROOT / "schemas/adblock-rules-manifest-v1.schema.json").read_text(
                encoding="utf-8"
            )
        )
        self.source_lock_schema = json.loads(
            (ROOT / "schemas/adblock-source-lock-v1.schema.json").read_text(
                encoding="utf-8"
            )
        )

    def test_manifest_shape_is_closed_and_signed(self) -> None:
        self.assertFalse(self.schema["additionalProperties"])
        self.assertEqual(
            set(self.schema["required"]),
            {
                "schema_version",
                "created_at",
                "minimum_app_version",
                "engine",
                "artifact",
                "signature",
                "sources",
            },
        )
        signature = self.schema["properties"]["signature"]
        self.assertFalse(signature["additionalProperties"])
        self.assertEqual(signature["properties"]["algorithm"]["const"], "Ed25519")
        self.assertEqual(signature["properties"]["key_id"]["pattern"], "^[0-9a-f]{64}$")

    def test_engine_and_artifact_limits_match_release_contract(self) -> None:
        engine = self.schema["properties"]["engine"]["properties"]
        self.assertEqual(engine["name"]["const"], "adblock-rust")
        self.assertEqual(engine["version"]["const"], "0.13.2")
        artifact = self.schema["properties"]["artifact"]["properties"]
        self.assertEqual(artifact["size"]["maximum"], 16 * 1024 * 1024)
        self.assertEqual(artifact["sha256"]["pattern"], "^[0-9a-f]{64}$")

    def test_source_lock_is_closed_and_pins_local_bytes(self) -> None:
        schema = self.source_lock_schema
        self.assertFalse(schema["additionalProperties"])
        self.assertEqual(set(schema["required"]), {"schema_version", "sources"})
        source = schema["properties"]["sources"]["items"]
        self.assertFalse(source["additionalProperties"])
        self.assertEqual(
            set(source["required"]),
            {"name", "url", "revision", "license", "path", "sha256"},
        )
        self.assertEqual(source["properties"]["revision"]["pattern"], "^[0-9a-f]{40}$")
        self.assertEqual(source["properties"]["sha256"]["pattern"], "^[0-9a-f]{64}$")


if __name__ == "__main__":
    unittest.main()
