#!/usr/bin/env python3
"""Validate and apply the deliberately small Fireball Chromium patch queue."""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import re
import subprocess
import sys
from typing import Any

SHA_PATTERN = re.compile(r"^[0-9a-f]{40}$")
REQUIRED_PATCH_FIELDS = {
    "id",
    "path",
    "sha256",
    "source",
    "chromium_range",
    "verified_against",
    "security_impact",
    "required_tests",
}
REQUIRED_SOURCE_FIELDS = {
    "project",
    "repository",
    "path",
    "commit",
    "license",
    "license_url",
}


class ManifestError(ValueError):
    pass


def load_manifest(path: pathlib.Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        document = json.load(handle)
    if (
        set(document) != {"schema_version", "automatic_import", "patches"}
        or document.get("schema_version") != 2
        or not isinstance(document.get("patches"), list)
    ):
        raise ManifestError("manifest must use the exact schema_version 2 shape")
    if document["automatic_import"] is not False:
        raise ManifestError("automatic patch import must remain disabled")
    return document


def _validate_source_path(value: object, patch_id: str) -> None:
    if not isinstance(value, str) or not value:
        raise ManifestError(f"{patch_id}: source path is required")
    source_path = pathlib.PurePosixPath(value)
    if source_path.is_absolute() or ".." in source_path.parts or source_path.as_posix() != value:
        raise ManifestError(f"{patch_id}: source path must be repository-relative")


def validate_manifest(
    manifest_path: pathlib.Path,
    repository_root: pathlib.Path,
    expected_chromium_revision: str | None = None,
) -> None:
    document = load_manifest(manifest_path)
    patch_root = (repository_root / "patches").resolve()
    seen_ids: set[str] = set()
    for index, patch in enumerate(document["patches"]):
        if not isinstance(patch, dict) or set(patch) != REQUIRED_PATCH_FIELDS:
            raise ManifestError(f"patch[{index}] must contain exactly {sorted(REQUIRED_PATCH_FIELDS)}")
        patch_id = patch["id"]
        if not isinstance(patch_id, str) or not re.fullmatch(r"[a-z0-9][a-z0-9_-]{2,63}", patch_id):
            raise ManifestError(f"patch[{index}].id is invalid")
        if patch_id in seen_ids:
            raise ManifestError(f"duplicate patch id: {patch_id}")
        seen_ids.add(patch_id)

        path = (repository_root / patch["path"]).resolve()
        if not path.is_relative_to(patch_root) or path.suffix != ".patch" or not path.is_file():
            raise ManifestError(f"{patch_id}: path must resolve to an existing .patch below patches/")
        actual_checksum = hashlib.sha256(path.read_bytes()).hexdigest()
        if not re.fullmatch(r"[0-9a-f]{64}", patch["sha256"]) or patch["sha256"] != actual_checksum:
            raise ManifestError(f"{patch_id}: checksum mismatch")

        source = patch["source"]
        if not isinstance(source, dict) or set(source) != REQUIRED_SOURCE_FIELDS:
            raise ManifestError(f"{patch_id}: incomplete source provenance")
        text_fields = ("project", "repository", "license", "license_url")
        if not all(isinstance(source[field], str) and source[field] for field in text_fields):
            raise ManifestError(f"{patch_id}: source text fields may not be empty")
        if not source["repository"].startswith("https://") or not source["license_url"].startswith("https://"):
            raise ManifestError(f"{patch_id}: source repository and license require HTTPS URLs")
        if not SHA_PATTERN.fullmatch(source["commit"]):
            raise ManifestError(f"{patch_id}: source requires an exact commit")
        _validate_source_path(source["path"], patch_id)

        verified_against = patch["verified_against"]
        if not isinstance(verified_against, str) or not SHA_PATTERN.fullmatch(verified_against):
            raise ManifestError(f"{patch_id}: verified_against must be an exact Chromium commit")
        if expected_chromium_revision is not None and verified_against != expected_chromium_revision:
            raise ManifestError(f"{patch_id}: patch was not verified against the pinned Chromium commit")

        chromium_range = patch["chromium_range"]
        if not isinstance(chromium_range, dict) or set(chromium_range) != {"minimum", "maximum"}:
            raise ManifestError(f"{patch_id}: chromium_range must have minimum and maximum")
        if not all(isinstance(chromium_range[key], int) and chromium_range[key] > 0 for key in chromium_range):
            raise ManifestError(f"{patch_id}: Chromium milestones must be positive integers")
        if chromium_range["minimum"] > chromium_range["maximum"]:
            raise ManifestError(f"{patch_id}: inverted Chromium range")
        if not isinstance(patch["security_impact"], str) or not patch["security_impact"].strip():
            raise ManifestError(f"{patch_id}: security impact is required")
        tests = patch["required_tests"]
        if not isinstance(tests, list) or not tests or not all(isinstance(test, str) and test for test in tests):
            raise ManifestError(f"{patch_id}: at least one required test is required")


def patch_paths(manifest_path: pathlib.Path, repository_root: pathlib.Path) -> list[pathlib.Path]:
    document = load_manifest(manifest_path)
    return [(repository_root / patch["path"]).resolve() for patch in document["patches"]]


def run_git_apply(checkout: pathlib.Path, patch: pathlib.Path, *, reverse: bool, check_only: bool) -> None:
    arguments = ["git", "-C", str(checkout), "apply"]
    if reverse:
        arguments.append("--reverse")
    if check_only:
        arguments.append("--check")
    arguments.extend(["--whitespace=error-all", str(patch)])
    subprocess.run(arguments, check=True)


def apply_queue(manifest_path: pathlib.Path, repository_root: pathlib.Path, checkout: pathlib.Path, reverse: bool) -> None:
    patches = patch_paths(manifest_path, repository_root)
    ordered = list(reversed(patches)) if reverse else patches
    for patch in ordered:
        run_git_apply(checkout, patch, reverse=reverse, check_only=True)
        run_git_apply(checkout, patch, reverse=reverse, check_only=False)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=("validate", "apply", "reverse"))
    parser.add_argument("--repo-root", type=pathlib.Path, default=pathlib.Path(__file__).resolve().parents[1])
    parser.add_argument("--manifest", type=pathlib.Path)
    parser.add_argument("--checkout", type=pathlib.Path)
    return parser


def main() -> int:
    arguments = build_parser().parse_args()
    repository_root = arguments.repo_root.resolve()
    manifest = (arguments.manifest or repository_root / "patches/manifest.json").resolve()
    try:
        expected_revision = None
        upstream_pin = repository_root / "pins/upstream.json"
        if upstream_pin.is_file():
            expected_revision = json.loads(upstream_pin.read_text(encoding="utf-8"))["chromium"]["revision"]
        validate_manifest(manifest, repository_root, expected_revision)
        if arguments.command != "validate":
            if arguments.checkout is None:
                raise ManifestError("--checkout is required for apply/reverse")
            apply_queue(manifest, repository_root, arguments.checkout.resolve(), arguments.command == "reverse")
    except (ManifestError, OSError, subprocess.CalledProcessError) as error:
        print(f"fireball-patches: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
