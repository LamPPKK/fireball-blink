#!/usr/bin/env python3
"""Validate, stage and attest the Fireball Chromium overlay tree."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from typing import Any, Sequence

from check_pins import PinError, load_json, validate_references, validate_upstream
from chromium_builder import (
    BuilderError,
    atomic_write_json,
    parse_gn_args,
    sha256_file,
    validate_control_gn_args,
    verify_checkout,
)
from fireball_patches import ManifestError
from fireball_patches import load_manifest as load_patch_manifest
from fireball_patches import validate_manifest as validate_patch_manifest

SHA_PATTERN = re.compile(r"^[0-9a-f]{40}$")
SHA256_PATTERN = re.compile(r"^[0-9a-f]{64}$")
MAX_SOURCE_FILE_BYTES = 2 * 1024 * 1024
MAX_SOURCE_TREE_BYTES = 32 * 1024 * 1024
ALLOWED_SUFFIXES = {
    ".cc",
    ".gn",
    ".h",
    ".inc",
    ".lock",
    ".md",
    ".mojom",
    ".rs",
    ".toml",
}
EXPECTED_ARCHITECTURE_ORDER = ["overlay", "chromium_src", "direct_patch"]
EXPECTED_REFERENCE_POLICY = {
    "brave": "overlay-before-override-before-direct-patch",
    "helium": "pinned-tree-checksum-and-ordered-series",
}
EXPECTED_SMOKE = {
    "schema_version": 1,
    "kind": "fireball-overlay-component-link",
    "status": "ok",
}


class OverlayError(ValueError):
    pass


@dataclass(frozen=True)
class SourceRecord:
    path: str
    bytes: int
    sha256: str


def run_git(repository_root: pathlib.Path, *arguments: str) -> str:
    try:
        result = subprocess.run(
            ["git", "-C", str(repository_root), *arguments],
            check=True,
            capture_output=True,
            timeout=30,
        )
    except (OSError, subprocess.CalledProcessError, subprocess.TimeoutExpired) as error:
        raise OverlayError("unable to inspect Fireball Git source") from error
    try:
        return result.stdout.decode("utf-8")
    except UnicodeDecodeError as error:
        raise OverlayError("Fireball source paths must be UTF-8") from error


def repository_file_paths(
    repository_root: pathlib.Path, source_root: str
) -> list[pathlib.PurePosixPath]:
    output = run_git(
        repository_root,
        "ls-files",
        "--cached",
        "--others",
        "--exclude-standard",
        "-z",
        "--",
        source_root,
    )
    paths: list[pathlib.PurePosixPath] = []
    for value in output.split("\0"):
        if not value:
            continue
        candidate = pathlib.PurePosixPath(value)
        if (
            candidate.is_absolute()
            or ".." in candidate.parts
            or not candidate.parts
            or candidate.parts[0] != source_root
            or candidate.as_posix() != value
        ):
            raise OverlayError(f"unsafe overlay path: {value}")
        paths.append(candidate)
    if len(paths) != len(set(paths)):
        raise OverlayError("overlay source list contains duplicate paths")
    return sorted(paths, key=lambda item: item.as_posix())


def source_record(repository_root: pathlib.Path, relative: pathlib.PurePosixPath) -> SourceRecord:
    path = repository_root.joinpath(*relative.parts)
    if path.is_symlink() or not path.is_file():
        raise OverlayError(f"overlay source must be a regular non-symlink file: {relative}")
    if path.suffix not in ALLOWED_SUFFIXES:
        raise OverlayError(f"overlay source type is not allowlisted: {relative}")
    stat = path.stat()
    if stat.st_mode & 0o111:
        raise OverlayError(f"overlay source may not be executable: {relative}")
    if stat.st_size > MAX_SOURCE_FILE_BYTES:
        raise OverlayError(f"overlay source exceeds 2 MiB: {relative}")
    return SourceRecord(
        path=relative.as_posix(),
        bytes=stat.st_size,
        sha256=sha256_file(path),
    )


def source_tree(repository_root: pathlib.Path, source_root: str) -> list[SourceRecord]:
    records = [
        source_record(repository_root, relative)
        for relative in repository_file_paths(repository_root, source_root)
    ]
    if not records:
        raise OverlayError("overlay source tree is empty")
    total_bytes = sum(record.bytes for record in records)
    if total_bytes > MAX_SOURCE_TREE_BYTES:
        raise OverlayError("overlay source tree exceeds 32 MiB")
    return records


def tree_sha256(records: Sequence[SourceRecord]) -> str:
    digest = hashlib.sha256()
    for record in records:
        digest.update(record.path.encode("utf-8"))
        digest.update(b"\0")
        digest.update(str(record.bytes).encode("ascii"))
        digest.update(b"\0")
        digest.update(record.sha256.encode("ascii"))
        digest.update(b"\n")
    return digest.hexdigest()


def tree_summary(records: Sequence[SourceRecord]) -> dict[str, Any]:
    return {
        "file_count": len(records),
        "bytes": sum(record.bytes for record in records),
        "sha256": tree_sha256(records),
    }


def load_overlay_manifest(path: pathlib.Path) -> dict[str, Any]:
    document = load_json(path)
    expected_fields = {
        "schema_version",
        "automatic_import",
        "source_repository",
        "root",
        "destination",
        "chromium_revision",
        "architecture_order",
        "reference_policy",
        "tree",
    }
    if set(document) != expected_fields or document.get("schema_version") != 1:
        raise OverlayError("overlay manifest must use the exact schema_version 1 shape")
    if document["automatic_import"] is not False:
        raise OverlayError("automatic overlay import must remain disabled")
    if document["source_repository"] != "https://github.com/LamPPKK/fireball-blink.git":
        raise OverlayError("overlay source repository is not trusted")
    if document["root"] != "fireball" or document["destination"] != "fireball":
        raise OverlayError("overlay root and Chromium destination must both be fireball")
    if document["architecture_order"] != EXPECTED_ARCHITECTURE_ORDER:
        raise OverlayError("overlay architecture order changed")
    if document["reference_policy"] != EXPECTED_REFERENCE_POLICY:
        raise OverlayError("Brave/Helium reference policy changed")
    tree = document["tree"]
    if not isinstance(tree, dict) or set(tree) != {"file_count", "bytes", "sha256"}:
        raise OverlayError("overlay tree evidence is incomplete")
    if (
        not isinstance(tree["file_count"], int)
        or tree["file_count"] <= 0
        or not isinstance(tree["bytes"], int)
        or tree["bytes"] <= 0
        or not isinstance(tree["sha256"], str)
        or SHA256_PATTERN.fullmatch(tree["sha256"]) is None
    ):
        raise OverlayError("overlay tree evidence is invalid")
    return document


def validate_overlay(
    repository_root: pathlib.Path, manifest_path: pathlib.Path
) -> tuple[dict[str, Any], list[SourceRecord]]:
    manifest = load_overlay_manifest(manifest_path)
    upstream = load_json(repository_root / "pins/upstream.json")
    references = load_json(repository_root / "pins/reference-browsers.json")
    validate_upstream(upstream)
    validate_references(references)
    if manifest["chromium_revision"] != upstream["chromium"]["revision"]:
        raise OverlayError("overlay was not verified against the pinned Chromium commit")
    records = source_tree(repository_root, manifest["root"])
    if manifest["tree"] != tree_summary(records):
        raise OverlayError("overlay tree checksum or size is stale")
    validate_patch_manifest(
        repository_root / "patches/manifest.json",
        repository_root,
        upstream["chromium"]["revision"],
    )
    return manifest, records


def ensure_no_unmanaged_overrides(repository_root: pathlib.Path) -> None:
    paths = repository_file_paths(repository_root, "chromium_src")
    unmanaged = [path.as_posix() for path in paths if path.as_posix() != "chromium_src/README.md"]
    if unmanaged:
        raise OverlayError(
            "chromium_src override staging requires a signed manifest: "
            + ", ".join(unmanaged)
        )


def ensure_clean_overlay(repository_root: pathlib.Path) -> None:
    status = run_git(
        repository_root,
        "status",
        "--porcelain",
        "--untracked-files=all",
        "--",
        "fireball",
        "overlay/manifest.json",
    )
    if status.strip():
        raise OverlayError("overlay staging requires committed, clean source files")


def verify_staged_tree(
    destination: pathlib.Path, source_root: str, records: Sequence[SourceRecord]
) -> None:
    expected_paths: set[str] = set()
    for record in records:
        relative = overlay_relative_path(record.path, source_root)
        target = destination.joinpath(*relative.parts)
        if target.is_symlink() or not target.is_file():
            raise OverlayError(f"staged overlay file is missing: {record.path}")
        if target.stat().st_size != record.bytes or sha256_file(target) != record.sha256:
            raise OverlayError(f"staged overlay file failed verification: {record.path}")
        expected_paths.add(relative.as_posix())
    actual_paths = {
        path.relative_to(destination).as_posix()
        for path in destination.rglob("*")
        if path.is_file()
    }
    if actual_paths != expected_paths:
        raise OverlayError("staged overlay contains an unexpected file set")


def overlay_relative_path(value: str, source_root: str) -> pathlib.PurePosixPath:
    candidate = pathlib.PurePosixPath(value)
    if candidate.is_absolute() or ".." in candidate.parts or candidate.as_posix() != value:
        raise OverlayError(f"unsafe staged overlay path: {value}")
    try:
        relative = candidate.relative_to(source_root)
    except ValueError as error:
        raise OverlayError(f"overlay path is outside {source_root}: {value}") from error
    if not relative.parts:
        raise OverlayError("overlay path may not name the source root directory")
    return relative


def stage_tree(
    repository_root: pathlib.Path,
    chromium_checkout: pathlib.Path,
    manifest: dict[str, Any],
    records: Sequence[SourceRecord],
) -> pathlib.Path:
    destination = chromium_checkout / manifest["destination"]
    if destination.exists() or destination.is_symlink():
        raise OverlayError("Chromium overlay destination already exists")
    staging_parent = pathlib.Path(
        tempfile.mkdtemp(prefix=".fireball-overlay-", dir=chromium_checkout)
    )
    staging_tree = staging_parent / manifest["destination"]
    staging_tree.mkdir()
    try:
        for record in records:
            relative = overlay_relative_path(record.path, manifest["root"])
            source = repository_root.joinpath(*pathlib.PurePosixPath(record.path).parts)
            target = staging_tree.joinpath(*relative.parts)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
            target.chmod(0o644)
        verify_staged_tree(staging_tree, manifest["root"], records)
        os.replace(staging_tree, destination)
        staging_parent.rmdir()
        return destination
    except Exception:
        shutil.rmtree(staging_parent, ignore_errors=True)
        raise


def stage_overlay(
    repository_root: pathlib.Path,
    manifest_path: pathlib.Path,
    chromium_checkout: pathlib.Path,
) -> dict[str, Any]:
    manifest, records = validate_overlay(repository_root, manifest_path)
    ensure_no_unmanaged_overrides(repository_root)
    ensure_clean_overlay(repository_root)
    upstream = load_json(repository_root / "pins/upstream.json")
    chromium = verify_checkout(
        chromium_checkout,
        upstream["chromium"]["url"],
        upstream["chromium"]["revision"],
    )
    origin = run_git(repository_root, "remote", "get-url", "origin").strip()
    revision = run_git(repository_root, "rev-parse", "HEAD").strip()
    if origin != manifest["source_repository"] or SHA_PATTERN.fullmatch(revision) is None:
        raise OverlayError("Fireball overlay source origin or revision is invalid")
    stage_tree(repository_root, chromium_checkout, manifest, records)
    return {
        "schema_version": 1,
        "kind": "fireball-overlay-stage",
        "source_repository": origin,
        "fireball_revision": revision,
        "chromium_revision": chromium["revision"],
        "destination": "//fireball",
        "architecture_order": manifest["architecture_order"],
        "tree": tree_summary(records),
        "files": [record.__dict__ for record in records],
        "overrides_applied": 0,
    }


def validate_stage_report(
    report: dict[str, Any], manifest: dict[str, Any]
) -> None:
    required = {
        "schema_version",
        "kind",
        "source_repository",
        "fireball_revision",
        "chromium_revision",
        "destination",
        "architecture_order",
        "tree",
        "files",
        "overrides_applied",
    }
    if set(report) != required or report.get("schema_version") != 1:
        raise OverlayError("overlay stage report shape is invalid")
    if report["kind"] != "fireball-overlay-stage":
        raise OverlayError("overlay stage report kind is invalid")
    if report["source_repository"] != manifest["source_repository"]:
        raise OverlayError("overlay stage source repository changed")
    if SHA_PATTERN.fullmatch(str(report["fireball_revision"])) is None:
        raise OverlayError("overlay stage Fireball revision is invalid")
    if report["chromium_revision"] != manifest["chromium_revision"]:
        raise OverlayError("overlay stage Chromium revision changed")
    if report["destination"] != "//fireball":
        raise OverlayError("overlay stage destination changed")
    if report["architecture_order"] != EXPECTED_ARCHITECTURE_ORDER:
        raise OverlayError("overlay stage architecture order changed")
    if report["tree"] != manifest["tree"]:
        raise OverlayError("overlay stage tree evidence changed")
    if report["overrides_applied"] != 0:
        raise OverlayError("unmanifested chromium_src overrides were applied")
    if not isinstance(report["files"], list) or len(report["files"]) != manifest["tree"]["file_count"]:
        raise OverlayError("overlay stage file evidence is incomplete")
    records: list[SourceRecord] = []
    for item in report["files"]:
        if not isinstance(item, dict) or set(item) != {"path", "bytes", "sha256"}:
            raise OverlayError("overlay stage file record is invalid")
        if (
            not isinstance(item["path"], str)
            or not item["path"].startswith("fireball/")
            or not isinstance(item["bytes"], int)
            or item["bytes"] < 0
            or not isinstance(item["sha256"], str)
            or SHA256_PATTERN.fullmatch(item["sha256"]) is None
        ):
            raise OverlayError("overlay stage file record value is invalid")
        records.append(SourceRecord(**item))
    if len({record.path for record in records}) != len(records):
        raise OverlayError("overlay stage file records contain duplicates")
    if tree_summary(records) != manifest["tree"]:
        raise OverlayError("overlay stage file records do not match the tree evidence")


def build_link_evidence(
    repository_root: pathlib.Path,
    manifest_path: pathlib.Path,
    stage_report_path: pathlib.Path,
    gn_args_path: pathlib.Path,
    binary_path: pathlib.Path,
    smoke_path: pathlib.Path,
    preflight_path: pathlib.Path,
) -> dict[str, Any]:
    manifest, _ = validate_overlay(repository_root, manifest_path)
    stage_report = load_json(stage_report_path)
    validate_stage_report(stage_report, manifest)
    gn_args = parse_gn_args(gn_args_path)
    validate_control_gn_args(gn_args)
    if not binary_path.is_file() or binary_path.stat().st_size == 0:
        raise OverlayError("overlay link-test binary is missing or empty")
    smoke = load_json(smoke_path)
    if smoke != EXPECTED_SMOKE:
        raise OverlayError("overlay link smoke result is not the exact passing contract")
    preflight = load_json(preflight_path)
    if preflight.get("schema_version") != 1 or preflight.get("passed") is not True:
        raise OverlayError("overlay evidence requires a passing builder preflight")
    patch_manifest_path = repository_root / "patches/manifest.json"
    patch_manifest = load_patch_manifest(patch_manifest_path)
    return {
        "schema_version": 1,
        "kind": "fireball-overlay-component-link-evidence",
        "source": {
            "repository": manifest["source_repository"],
            "fireball_revision": stage_report["fireball_revision"],
            "chromium_revision": stage_report["chromium_revision"],
            "tree": manifest["tree"],
            "architecture_order": manifest["architecture_order"],
        },
        "build": {
            "target": "//fireball:overlay_smoke",
            "gn_args_sha256": sha256_file(gn_args_path),
            "direct_patch_count": len(patch_manifest["patches"]),
            "direct_patch_manifest_sha256": sha256_file(patch_manifest_path),
            "chromium_src_override_count": 0,
            "partition_allocator": "chromium-default",
        },
        "verification": {
            "preflight_passed": True,
            "preflight_sha256": sha256_file(preflight_path),
            "stage_report_sha256": sha256_file(stage_report_path),
            "smoke_sha256": sha256_file(smoke_path),
            "smoke": smoke,
        },
        "binary": {
            "filename": binary_path.name,
            "bytes": binary_path.stat().st_size,
            "sha256": sha256_file(binary_path),
        },
        "limitations": [
            "not-a-chromium-browser-target",
            "profile-lifecycle-hook-not-wired",
            "subresource-lifecycle-hook-not-wired",
            "keepalive-and-prefetch-policy-not-wired",
            "renderer-cosmetic-controller-bridge-not-wired",
            "renderer-cosmetic-lifecycle-owner-not-wired",
            "renderer-content-client-registration-not-wired",
            "adblock-rust-ffi-not-linked-by-this-target",
            "not-release-or-packaging-evidence",
        ],
    }


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--repository-root",
        type=pathlib.Path,
        default=pathlib.Path(__file__).resolve().parents[1],
    )
    parser.add_argument("--manifest", type=pathlib.Path)
    subparsers = parser.add_subparsers(dest="command", required=True)

    subparsers.add_parser("validate")
    subparsers.add_parser("hash")

    stage = subparsers.add_parser("stage")
    stage.add_argument("--chromium-checkout", type=pathlib.Path, required=True)
    stage.add_argument("--output", type=pathlib.Path, required=True)

    evidence = subparsers.add_parser("evidence")
    evidence.add_argument("--stage-report", type=pathlib.Path, required=True)
    evidence.add_argument("--gn-args", type=pathlib.Path, required=True)
    evidence.add_argument("--binary", type=pathlib.Path, required=True)
    evidence.add_argument("--smoke", type=pathlib.Path, required=True)
    evidence.add_argument("--preflight", type=pathlib.Path, required=True)
    evidence.add_argument("--output", type=pathlib.Path, required=True)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = create_parser().parse_args(argv)
    repository_root = args.repository_root.resolve()
    manifest_path = (args.manifest or repository_root / "overlay/manifest.json").resolve()
    try:
        if args.command == "hash":
            print(json.dumps(tree_summary(source_tree(repository_root, "fireball")), sort_keys=True))
            return 0
        if args.command == "validate":
            validate_overlay(repository_root, manifest_path)
            ensure_no_unmanaged_overrides(repository_root)
            return 0
        if args.command == "stage":
            report = stage_overlay(
                repository_root, manifest_path, args.chromium_checkout.resolve()
            )
            atomic_write_json(args.output, report)
            return 0
        if args.command == "evidence":
            document = build_link_evidence(
                repository_root,
                manifest_path,
                args.stage_report,
                args.gn_args,
                args.binary,
                args.smoke,
                args.preflight,
            )
            atomic_write_json(args.output, document)
            return 0
        raise OverlayError("unknown command")
    except (
        BuilderError,
        ManifestError,
        OverlayError,
        OSError,
        PinError,
        json.JSONDecodeError,
        subprocess.CalledProcessError,
    ) as error:
        print(f"fireball-overlay: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
