from __future__ import annotations

import hashlib
import json
import os
import pathlib
import subprocess
import sys
import tempfile
import threading
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
TOOL = ROOT / "tools/url_cleaner_rules.py"
RULES = ROOT / "rules/url-cleaner/rules-v1.json"
CORPUS = ROOT / "rules/url-cleaner/false-positive-corpus-v1.json"


class UrlCleanerRulesToolTests(unittest.TestCase):
    def run_tool(
        self, *arguments: object, check: bool = True
    ) -> subprocess.CompletedProcess[str]:
        result = subprocess.run(
            [sys.executable, str(TOOL), *(str(value) for value in arguments)],
            cwd=ROOT,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        if check and result.returncode != 0:
            self.fail(f"tool failed: {result.stdout}{result.stderr}")
        return result

    def test_checked_in_outputs_are_current_and_report_source_checksums(self) -> None:
        result = self.run_tool("check")
        report = json.loads(result.stdout)
        self.assertEqual(report["schema_version"], 1)
        self.assertEqual(report["rules_version"], "2026.8.1")
        self.assertEqual(report["parameter_count"], 16)
        self.assertEqual(report["case_count"], 20)
        self.assertFalse(report["automatic_import"])
        self.assertEqual(report["rules_sha256"], hashlib.sha256(RULES.read_bytes()).hexdigest())
        self.assertEqual(report["corpus_sha256"], hashlib.sha256(CORPUS.read_bytes()).hexdigest())

    def test_generation_is_deterministic_and_check_rejects_stale_output(self) -> None:
        with tempfile.TemporaryDirectory(prefix="fireball-url-cleaner-") as temporary:
            directory = pathlib.Path(temporary)
            first_rules = directory / "first-rules.h"
            first_corpus = directory / "first-corpus.h"
            second_rules = directory / "second-rules.h"
            second_corpus = directory / "second-corpus.h"
            self.run_tool(
                "generate",
                "--rules-header",
                first_rules,
                "--corpus-header",
                first_corpus,
            )
            self.run_tool(
                "generate",
                "--rules-header",
                second_rules,
                "--corpus-header",
                second_corpus,
            )
            self.assertEqual(first_rules.read_bytes(), second_rules.read_bytes())
            self.assertEqual(first_corpus.read_bytes(), second_corpus.read_bytes())
            first_rules.write_bytes(first_rules.read_bytes() + b"// stale\n")
            result = self.run_tool(
                "check",
                "--rules-header",
                first_rules,
                "--corpus-header",
                first_corpus,
                check=False,
            )
            self.assertNotEqual(result.returncode, 0)

    def test_unknown_unsorted_duplicate_and_upstream_rule_data_fail_closed(self) -> None:
        original = json.loads(RULES.read_text(encoding="utf-8"))
        mutations = []
        unknown = dict(original)
        unknown["unexpected"] = True
        mutations.append(unknown)
        unsorted = json.loads(json.dumps(original))
        unsorted["parameters"] = list(reversed(unsorted["parameters"]))
        mutations.append(unsorted)
        duplicate = json.loads(json.dumps(original))
        duplicate["parameters"].append(duplicate["parameters"][0])
        mutations.append(duplicate)
        imported = json.loads(json.dumps(original))
        imported["provenance"]["automatic_import"] = True
        mutations.append(imported)

        with tempfile.TemporaryDirectory(prefix="fireball-url-cleaner-invalid-") as temporary:
            directory = pathlib.Path(temporary)
            for index, document in enumerate(mutations):
                rules = directory / f"rules-{index}.json"
                rules.write_text(json.dumps(document), encoding="utf-8")
                result = self.run_tool(
                    "generate",
                    "--rules",
                    rules,
                    "--rules-header",
                    directory / f"rules-{index}.h",
                    "--corpus-header",
                    directory / f"corpus-{index}.h",
                    check=False,
                )
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse((directory / f"rules-{index}.h").exists())
                self.assertFalse((directory / f"corpus-{index}.h").exists())

    def test_corpus_version_and_semantics_are_strict(self) -> None:
        original = json.loads(CORPUS.read_text(encoding="utf-8"))
        mutations = []
        mismatch = json.loads(json.dumps(original))
        mismatch["rules_version"] = "2026.8.2"
        mutations.append(mismatch)
        invalid_output = json.loads(json.dumps(original))
        invalid_output["cases"][0]["expected_status"] = "unchanged"
        mutations.append(invalid_output)
        duplicate_name = json.loads(json.dumps(original))
        duplicate_name["cases"][1]["name"] = duplicate_name["cases"][0]["name"]
        mutations.append(duplicate_name)

        with tempfile.TemporaryDirectory(prefix="fireball-url-corpus-invalid-") as temporary:
            directory = pathlib.Path(temporary)
            for index, document in enumerate(mutations):
                corpus = directory / f"corpus-{index}.json"
                corpus.write_text(json.dumps(document), encoding="utf-8")
                result = self.run_tool(
                    "generate",
                    "--corpus",
                    corpus,
                    "--rules-header",
                    directory / f"rules-{index}.h",
                    "--corpus-header",
                    directory / f"corpus-{index}.h",
                    check=False,
                )
                self.assertNotEqual(result.returncode, 0)

    def test_input_output_aliases_are_rejected_without_mutation(self) -> None:
        with tempfile.TemporaryDirectory(prefix="fireball-url-cleaner-alias-") as temporary:
            directory = pathlib.Path(temporary)
            shared_output = directory / "shared.h"
            result = self.run_tool(
                "generate",
                "--rules-header",
                shared_output,
                "--corpus-header",
                shared_output,
                check=False,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse(shared_output.exists())

            preserved = RULES.read_bytes()
            result = self.run_tool(
                "generate",
                "--rules-header",
                RULES,
                "--corpus-header",
                directory / "corpus.h",
                check=False,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(RULES.read_bytes(), preserved)
            self.assertFalse((directory / "corpus.h").exists())

    @unittest.skipUnless(hasattr(os, "mkfifo"), "FIFO support is required")
    def test_non_regular_and_symlink_inputs_fail_without_blocking(self) -> None:
        with tempfile.TemporaryDirectory(prefix="fireball-url-cleaner-files-") as temporary:
            directory = pathlib.Path(temporary)
            linked_rules = directory / "linked-rules.json"
            linked_rules.symlink_to(RULES)
            linked = self.run_tool("check", "--rules", linked_rules, check=False)
            self.assertNotEqual(linked.returncode, 0)

            fifo = directory / "rules.fifo"
            os.mkfifo(fifo)
            result: list[subprocess.CompletedProcess[str]] = []
            worker = threading.Thread(
                target=lambda: result.append(
                    self.run_tool("check", "--rules", fifo, check=False)
                ),
                daemon=True,
            )
            worker.start()
            worker.join(timeout=2)
            self.assertFalse(worker.is_alive(), "FIFO input must not block the validator")
            self.assertEqual(len(result), 1)
            self.assertNotEqual(result[0].returncode, 0)

            if pathlib.Path("/dev/zero").exists():
                device = self.run_tool("check", "--rules", "/dev/zero", check=False)
                self.assertNotEqual(device.returncode, 0)

    def test_json_schemas_lock_unknown_fields_and_limits(self) -> None:
        rules_schema = json.loads(
            (ROOT / "schemas/url-cleaner-rules-v1.schema.json").read_text(encoding="utf-8")
        )
        corpus_schema = json.loads(
            (ROOT / "schemas/url-cleaner-corpus-v1.schema.json").read_text(encoding="utf-8")
        )
        self.assertFalse(rules_schema["additionalProperties"])
        self.assertEqual(rules_schema["properties"]["parameters"]["maxItems"], 128)
        self.assertEqual(
            rules_schema["x-fireball-executable-invariants"],
            ["parameters are bytewise sorted"],
        )
        self.assertFalse(corpus_schema["additionalProperties"])
        self.assertFalse(
            corpus_schema["properties"]["cases"]["items"]["additionalProperties"]
        )
        self.assertEqual(corpus_schema["properties"]["cases"]["maxItems"], 256)
        self.assertIn(
            "case names are unique",
            corpus_schema["x-fireball-executable-invariants"],
        )
        self.assertIn(
            "rules_version equals the selected rules artifact rules_version",
            corpus_schema["x-fireball-executable-invariants"],
        )


if __name__ == "__main__":
    unittest.main()
