#!/usr/bin/env python3
from __future__ import annotations

import datetime
import json
import pathlib
import re
from typing import Any

SHA_PATTERN = re.compile(r"^[0-9a-f]{40}$")
VERSION_PATTERN = re.compile(r"^(\d+)\.\d+\.\d+\.\d+$")
ROLE_PATTERN = re.compile(r"^[a-z0-9][a-z0-9-]{2,63}$")

EXPECTED_REFERENCES = {
    "brave-core": {
        "url": "https://github.com/brave/brave-core.git",
        "ref": "refs/heads/master",
        "license": "MPL-2.0",
    },
    "helium": {
        "url": "https://github.com/imputnet/helium.git",
        "ref": "refs/heads/main",
        "license": "GPL-3.0",
    },
}


class PinError(ValueError):
    pass


def load_json(path: pathlib.Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        document = json.load(handle)
    if not isinstance(document, dict):
        raise PinError(f"{path.name}: root must be an object")
    return document


def validate_timestamp(value: object, field: str) -> None:
    if not isinstance(value, str):
        raise PinError(f"{field}: timestamp is required")
    try:
        parsed = datetime.datetime.fromisoformat(value)
    except ValueError as error:
        raise PinError(f"{field}: timestamp must use ISO 8601") from error
    if parsed.tzinfo is None:
        raise PinError(f"{field}: timestamp must include a timezone")


def validate_revision(value: object, field: str) -> None:
    if not isinstance(value, str) or not SHA_PATTERN.fullmatch(value):
        raise PinError(f"{field}: revision must be an exact SHA-1")


def validate_upstream(document: dict[str, Any]) -> None:
    if set(document) != {
        "schema_version",
        "captured_at",
        "release_source",
        "chromium",
        "depot_tools",
    } or document.get("schema_version") != 2:
        raise PinError("upstream pins must use the exact schema_version 2 shape")
    validate_timestamp(document["captured_at"], "upstream.captured_at")
    if not str(document["release_source"]).startswith("https://chromiumdash.appspot.com/"):
        raise PinError("upstream.release_source must be Chromium Dash")

    chromium = document["chromium"]
    if not isinstance(chromium, dict) or set(chromium) != {
        "url",
        "ref",
        "version",
        "milestone",
        "revision",
    }:
        raise PinError("chromium: incomplete pin")
    if chromium["url"] != "https://chromium.googlesource.com/chromium/src.git":
        raise PinError("chromium: untrusted upstream URL")
    version_match = VERSION_PATTERN.fullmatch(str(chromium["version"]))
    if version_match is None:
        raise PinError("chromium: invalid release version")
    if chromium["ref"] != f"refs/tags/{chromium['version']}":
        raise PinError("chromium: ref must be the exact release tag")
    if chromium["milestone"] != int(version_match.group(1)):
        raise PinError("chromium: milestone does not match version")
    validate_revision(chromium["revision"], "chromium")

    depot_tools = document["depot_tools"]
    if not isinstance(depot_tools, dict) or set(depot_tools) != {"url", "ref", "revision"}:
        raise PinError("depot_tools: incomplete pin")
    if depot_tools["url"] != "https://chromium.googlesource.com/chromium/tools/depot_tools.git":
        raise PinError("depot_tools: untrusted upstream URL")
    if depot_tools["ref"] != "refs/heads/main":
        raise PinError("depot_tools: unexpected ref")
    validate_revision(depot_tools["revision"], "depot_tools")


def validate_references(document: dict[str, Any]) -> None:
    if set(document) != {"schema_version", "captured_at", "automatic_patch_import", "references"}:
        raise PinError("reference browsers: unexpected document shape")
    if document["schema_version"] != 1:
        raise PinError("reference browsers: unsupported schema")
    validate_timestamp(document["captured_at"], "references.captured_at")
    if document["automatic_patch_import"] is not False:
        raise PinError("reference browsers: automatic patch import must remain disabled")
    references = document["references"]
    if not isinstance(references, list) or len(references) != len(EXPECTED_REFERENCES):
        raise PinError("reference browsers: Brave and Helium are both required")

    seen: set[str] = set()
    expected_fields = {
        "name",
        "url",
        "ref",
        "revision",
        "license",
        "observed_product_version",
        "observed_chromium_version",
        "roles",
    }
    for reference in references:
        if not isinstance(reference, dict) or set(reference) != expected_fields:
            raise PinError("reference browser: unexpected entry shape")
        name = reference["name"]
        if name not in EXPECTED_REFERENCES or name in seen:
            raise PinError(f"reference browser: unexpected or duplicate name {name!r}")
        seen.add(name)
        for field, expected in EXPECTED_REFERENCES[name].items():
            if reference[field] != expected:
                raise PinError(f"{name}: unexpected {field}")
        validate_revision(reference["revision"], name)
        if VERSION_PATTERN.fullmatch(str(reference["observed_chromium_version"])) is None:
            raise PinError(f"{name}: invalid observed Chromium version")
        if not isinstance(reference["observed_product_version"], str) or not reference["observed_product_version"]:
            raise PinError(f"{name}: observed product version is required")
        roles = reference["roles"]
        if not isinstance(roles, list) or not roles or len(set(roles)) != len(roles):
            raise PinError(f"{name}: roles must be a non-empty unique list")
        if not all(isinstance(role, str) and ROLE_PATTERN.fullmatch(role) for role in roles):
            raise PinError(f"{name}: invalid role")

    if seen != set(EXPECTED_REFERENCES):
        raise PinError("reference browsers: missing required reference")


def validate_repository(repository_root: pathlib.Path) -> None:
    validate_upstream(load_json(repository_root / "pins/upstream.json"))
    validate_references(load_json(repository_root / "pins/reference-browsers.json"))


def main() -> int:
    root = pathlib.Path(__file__).resolve().parents[1]
    try:
        validate_repository(root)
    except (OSError, json.JSONDecodeError, PinError) as error:
        print(f"fireball-pins: {error}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
