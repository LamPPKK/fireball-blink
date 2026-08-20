#!/usr/bin/env python3
"""Validate Fireball URL Cleaner data and generate deterministic C++ tables."""

from __future__ import annotations

import argparse
import datetime
import hashlib
import ipaddress
import json
import os
import pathlib
import re
import stat
import sys
import tempfile
from typing import Any, NoReturn, Sequence


MAX_DOCUMENT_BYTES = 256 * 1024
RULE_FIELDS = {
    "schema_version",
    "rules_version",
    "minimum_app_version",
    "created_at",
    "parameters",
    "provenance",
}
PROVENANCE_FIELDS = {"maintainer", "automatic_import", "upstream_imports"}
CORPUS_FIELDS = {"schema_version", "rules_version", "cases"}
CASE_FIELDS = {
    "name",
    "input_url",
    "hostname",
    "enabled",
    "site_exempt",
    "expected_status",
    "expected_url",
    "removed_parameters",
}
RULE_VERSION = re.compile(r"^[0-9]+(?:\.[0-9]+)+$")
SEMVER = re.compile(r"^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$")
PARAMETER = re.compile(r"^[a-z0-9_.-]{1,64}$")
CASE_NAME = re.compile(r"^[a-z0-9-]{1,64}$")
DNS_LABEL = re.compile(r"^[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?$")


class UrlCleanerDataError(ValueError):
    pass


def fail(message: str) -> NoReturn:
    raise UrlCleanerDataError(message)


def load_document(path: pathlib.Path) -> tuple[dict[str, Any], bytes]:
    flags = os.O_RDONLY | getattr(os, "O_CLOEXEC", 0) | getattr(os, "O_NONBLOCK", 0)
    flags |= getattr(os, "O_NOFOLLOW", 0)
    try:
        descriptor = os.open(path, flags)
    except OSError as error:
        raise UrlCleanerDataError(f"{path.name}: cannot read file") from error
    try:
        metadata = os.fstat(descriptor)
        if (
            not stat.S_ISREG(metadata.st_mode)
            or metadata.st_size <= 0
            or metadata.st_size > MAX_DOCUMENT_BYTES
        ):
            fail(f"{path.name}: bounded regular file is required")
        chunks: list[bytes] = []
        remaining = MAX_DOCUMENT_BYTES + 1
        while remaining > 0:
            chunk = os.read(descriptor, min(64 * 1024, remaining))
            if not chunk:
                break
            chunks.append(chunk)
            remaining -= len(chunk)
        payload = b"".join(chunks)
        final_metadata = os.fstat(descriptor)
        if (
            len(payload) != metadata.st_size
            or final_metadata.st_size != metadata.st_size
            or final_metadata.st_mtime_ns != metadata.st_mtime_ns
        ):
            fail(f"{path.name}: file changed while reading")
    except OSError as error:
        raise UrlCleanerDataError(f"{path.name}: cannot read file") from error
    finally:
        os.close(descriptor)
    if not payload or len(payload) > MAX_DOCUMENT_BYTES or b"\x00" in payload:
        fail(f"{path.name}: invalid size or NUL byte")
    try:
        document = json.loads(payload.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise UrlCleanerDataError(f"{path.name}: valid UTF-8 JSON is required") from error
    if not isinstance(document, dict):
        fail(f"{path.name}: root must be an object")
    return document, payload


def validate_timestamp(value: object) -> str:
    if not isinstance(value, str) or len(value) != 20 or not value.endswith("Z"):
        fail("created_at: canonical UTC timestamp is required")
    try:
        parsed = datetime.datetime.strptime(value, "%Y-%m-%dT%H:%M:%SZ")
    except ValueError as error:
        raise UrlCleanerDataError("created_at: invalid UTC timestamp") from error
    if parsed.strftime("%Y-%m-%dT%H:%M:%SZ") != value:
        fail("created_at: non-canonical UTC timestamp")
    return value


def validate_rules(path: pathlib.Path) -> tuple[dict[str, Any], bytes]:
    document, payload = load_document(path)
    if set(document) != RULE_FIELDS or document.get("schema_version") != 1:
        fail("rules: exact schema_version 1 shape is required")
    version = document["rules_version"]
    if (
        not isinstance(version, str)
        or len(version) > 32
        or RULE_VERSION.fullmatch(version) is None
    ):
        fail("rules_version: dotted numeric version is required")
    minimum = document["minimum_app_version"]
    if not isinstance(minimum, str) or SEMVER.fullmatch(minimum) is None:
        fail("minimum_app_version: canonical semantic version is required")
    validate_timestamp(document["created_at"])
    parameters = document["parameters"]
    if not isinstance(parameters, list) or not 1 <= len(parameters) <= 128:
        fail("parameters: one to 128 entries are required")
    if any(not isinstance(value, str) or PARAMETER.fullmatch(value) is None for value in parameters):
        fail("parameters: lower-case exact-name tokens are required")
    if parameters != sorted(parameters) or len(parameters) != len(set(parameters)):
        fail("parameters: entries must be unique and bytewise sorted")
    provenance = document["provenance"]
    if not isinstance(provenance, dict) or set(provenance) != PROVENANCE_FIELDS:
        fail("provenance: exact first-party provenance shape is required")
    if (
        provenance["maintainer"] != "Fireball"
        or provenance["automatic_import"] is not False
        or provenance["upstream_imports"] != []
    ):
        fail("provenance: automatic or unreviewed upstream imports are forbidden")
    return document, payload


def validate_hostname(value: object, field: str) -> str:
    if not isinstance(value, str) or not value or len(value.encode("ascii", "ignore")) != len(value):
        fail(f"{field}: bounded lower-case ASCII hostname is required")
    if value != value.lower() or len(value) > 253:
        fail(f"{field}: bounded lower-case ASCII hostname is required")
    try:
        address = ipaddress.ip_address(value)
    except ValueError:
        labels = value.split(".")
        if not labels or any(DNS_LABEL.fullmatch(label) is None for label in labels):
            fail(f"{field}: canonical DNS name or IP literal is required")
    else:
        if str(address) != value:
            fail(f"{field}: canonical IP literal is required")
    return value


def validate_url_text(value: object, field: str, allow_empty: bool = False) -> str:
    if not isinstance(value, str) or (not value and not allow_empty):
        fail(f"{field}: string is required")
    try:
        encoded = value.encode("ascii")
    except UnicodeEncodeError as error:
        raise UrlCleanerDataError(f"{field}: ASCII URL is required") from error
    if len(encoded) > 8192 or any(ord(character) <= 0x20 or ord(character) == 0x7F for character in value):
        fail(f"{field}: bounded URL without controls is required")
    return value


def validate_corpus(path: pathlib.Path, rules_version: str) -> tuple[dict[str, Any], bytes]:
    document, payload = load_document(path)
    if set(document) != CORPUS_FIELDS or document.get("schema_version") != 1:
        fail("corpus: exact schema_version 1 shape is required")
    if document["rules_version"] != rules_version:
        fail("corpus: rules_version must match the rules artifact")
    cases = document["cases"]
    if not isinstance(cases, list) or not 1 <= len(cases) <= 256:
        fail("corpus: one to 256 cases are required")
    names: set[str] = set()
    for index, case in enumerate(cases):
        prefix = f"cases[{index}]"
        if not isinstance(case, dict) or set(case) != CASE_FIELDS:
            fail(f"{prefix}: exact field set is required")
        name = case["name"]
        if not isinstance(name, str) or CASE_NAME.fullmatch(name) is None or name in names:
            fail(f"{prefix}.name: unique lower-case token is required")
        names.add(name)
        input_url = validate_url_text(case["input_url"], f"{prefix}.input_url")
        validate_hostname(case["hostname"], f"{prefix}.hostname")
        if not isinstance(case["enabled"], bool) or not isinstance(case["site_exempt"], bool):
            fail(f"{prefix}: enabled and site_exempt must be booleans")
        status = case["expected_status"]
        if status not in {"invalid", "unchanged", "cleaned"}:
            fail(f"{prefix}.expected_status: unknown status")
        expected_url = validate_url_text(
            case["expected_url"], f"{prefix}.expected_url", allow_empty=True
        )
        removed = case["removed_parameters"]
        if not isinstance(removed, int) or isinstance(removed, bool) or not 0 <= removed <= 512:
            fail(f"{prefix}.removed_parameters: bounded integer is required")
        if status == "invalid" and (expected_url or removed != 0):
            fail(f"{prefix}: invalid cases cannot expose output")
        if status == "unchanged" and (expected_url != input_url or removed != 0):
            fail(f"{prefix}: unchanged cases must preserve input exactly")
        if status == "cleaned" and (expected_url == input_url or removed == 0):
            fail(f"{prefix}: cleaned cases must remove at least one parameter")
        if case["site_exempt"] and status != "unchanged":
            fail(f"{prefix}: site exemptions must remain unchanged")
    return document, payload


def sha256_hex(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def cpp_string(value: str) -> str:
    return json.dumps(value, ensure_ascii=True)


def rules_header(document: dict[str, Any], payload: bytes) -> bytes:
    parameters = document["parameters"]
    entries = "\n".join(f"    {cpp_string(value)}," for value in parameters)
    result = f"""// Generated by tools/url_cleaner_rules.py. Do not edit.
#ifndef FIREBALL_COMPONENTS_NAVIGATION_GENERATED_URL_CLEANER_RULES_H_
#define FIREBALL_COMPONENTS_NAVIGATION_GENERATED_URL_CLEANER_RULES_H_

#include <array>
#include <string_view>

namespace fireball::navigation::generated {{

inline constexpr std::string_view kUrlCleanerRulesVersion = {cpp_string(document['rules_version'])};
inline constexpr std::string_view kUrlCleanerRulesSha256 = {cpp_string(sha256_hex(payload))};
inline constexpr std::array<std::string_view, {len(parameters)}> kUrlCleanerTrackingParameters = {{{{
{entries}
}}}};

}}  // namespace fireball::navigation::generated

#endif  // FIREBALL_COMPONENTS_NAVIGATION_GENERATED_URL_CLEANER_RULES_H_
"""
    return result.encode("utf-8")


def corpus_header(document: dict[str, Any], payload: bytes) -> bytes:
    rows = []
    for case in document["cases"]:
        rows.append(
            "    {"
            + ", ".join(
                [
                    cpp_string(case["name"]),
                    cpp_string(case["input_url"]),
                    cpp_string(case["hostname"]),
                    "true" if case["enabled"] else "false",
                    "true" if case["site_exempt"] else "false",
                    cpp_string(case["expected_status"]),
                    cpp_string(case["expected_url"]),
                    str(case["removed_parameters"]),
                ]
            )
            + "},"
        )
    entries = "\n".join(rows)
    result = f"""// Generated by tools/url_cleaner_rules.py. Do not edit.
#ifndef TESTS_GENERATED_URL_CLEANER_CORPUS_H_
#define TESTS_GENERATED_URL_CLEANER_CORPUS_H_

#include <array>
#include <cstddef>
#include <string_view>

namespace fireball::navigation::test_data {{

struct UrlCleanerCorpusCase {{
  std::string_view name;
  std::string_view input_url;
  std::string_view hostname;
  bool enabled;
  bool site_exempt;
  std::string_view expected_status;
  std::string_view expected_url;
  std::size_t removed_parameters;
}};

inline constexpr std::string_view kUrlCleanerCorpusRulesVersion = {cpp_string(document['rules_version'])};
inline constexpr std::string_view kUrlCleanerCorpusSha256 = {cpp_string(sha256_hex(payload))};
inline constexpr std::array<UrlCleanerCorpusCase, {len(document['cases'])}> kUrlCleanerCorpus = {{{{
{entries}
}}}};

}}  // namespace fireball::navigation::test_data

#endif  // TESTS_GENERATED_URL_CLEANER_CORPUS_H_
"""
    return result.encode("utf-8")


def canonical_path(path: pathlib.Path) -> pathlib.Path:
    try:
        return path.resolve(strict=False)
    except OSError as error:
        raise UrlCleanerDataError(f"{path}: cannot resolve path") from error


def validate_paths(args: argparse.Namespace) -> None:
    labelled = [
        ("rules input", args.rules),
        ("corpus input", args.corpus),
        ("rules header output", args.rules_header),
        ("corpus header output", args.corpus_header),
    ]
    for index, (left_label, left) in enumerate(labelled):
        left_resolved = canonical_path(left)
        for right_label, right in labelled[index + 1 :]:
            right_resolved = canonical_path(right)
            aliases = left_resolved == right_resolved
            try:
                aliases = aliases or (
                    left.exists() and right.exists() and os.path.samefile(left, right)
                )
            except OSError as error:
                raise UrlCleanerDataError("cannot compare input/output paths") from error
            if aliases:
                fail(f"paths: {left_label} and {right_label} must be distinct")


def stage_output(path: pathlib.Path, payload: bytes) -> pathlib.Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary: pathlib.Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            dir=path.parent, prefix=f".{path.name}.", delete=False
        ) as handle:
            temporary = pathlib.Path(handle.name)
            handle.write(payload)
            handle.flush()
            os.fsync(handle.fileno())
        os.chmod(temporary, 0o644)
        return temporary
    except BaseException:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
        raise


def publish_outputs(outputs: Sequence[tuple[pathlib.Path, bytes]]) -> None:
    staged: list[tuple[pathlib.Path, pathlib.Path]] = []
    try:
        for path, payload in outputs:
            if path.is_symlink() or (path.exists() and not path.is_file()):
                fail(f"{path.name}: output must be a regular non-symlink file")
            staged.append((path, stage_output(path, payload)))
        for path, temporary in staged:
            os.replace(temporary, path)
    finally:
        for _, temporary in staged:
            temporary.unlink(missing_ok=True)


def expected_outputs(args: argparse.Namespace) -> tuple[bytes, bytes, dict[str, Any]]:
    rules, rules_payload = validate_rules(args.rules)
    corpus, corpus_payload = validate_corpus(args.corpus, rules["rules_version"])
    report = {
        "schema_version": 1,
        "rules_version": rules["rules_version"],
        "rules_sha256": sha256_hex(rules_payload),
        "corpus_sha256": sha256_hex(corpus_payload),
        "parameter_count": len(rules["parameters"]),
        "case_count": len(corpus["cases"]),
        "automatic_import": False,
    }
    return rules_header(rules, rules_payload), corpus_header(corpus, corpus_payload), report


def create_parser(root: pathlib.Path) -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("generate", "check"))
    parser.add_argument("--rules", type=pathlib.Path, default=root / "rules/url-cleaner/rules-v1.json")
    parser.add_argument("--corpus", type=pathlib.Path, default=root / "rules/url-cleaner/false-positive-corpus-v1.json")
    parser.add_argument("--rules-header", type=pathlib.Path, default=root / "fireball/components/navigation/generated_url_cleaner_rules.h")
    parser.add_argument("--corpus-header", type=pathlib.Path, default=root / "tests/generated/url_cleaner_corpus.h")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    root = pathlib.Path(__file__).resolve().parents[1]
    args = create_parser(root).parse_args(argv)
    try:
        validate_paths(args)
        expected_rules, expected_corpus, report = expected_outputs(args)
        if args.command == "generate":
            publish_outputs(
                (
                    (args.rules_header, expected_rules),
                    (args.corpus_header, expected_corpus),
                )
            )
        else:
            for path, expected in (
                (args.rules_header, expected_rules),
                (args.corpus_header, expected_corpus),
            ):
                if path.is_symlink() or not path.is_file() or path.read_bytes() != expected:
                    fail(f"{path.name}: generated output is missing or stale")
        print(json.dumps(report, sort_keys=True, separators=(",", ":")))
        return 0
    except (OSError, UrlCleanerDataError) as error:
        print(f"url-cleaner-rules: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
