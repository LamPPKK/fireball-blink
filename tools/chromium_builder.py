#!/usr/bin/env python3
"""Fail-closed tooling for the protected Chromium B0 control builder."""

from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import os
import pathlib
import platform
import re
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from typing import Any, Sequence

from check_pins import PinError, load_json, validate_upstream

GIB = 1024**3
MIN_CPU_COUNT = 8
MIN_MEMORY_BYTES = 32 * GIB
MIN_FREE_DISK_BYTES = 300 * GIB
REQUIRED_OS_ID = "ubuntu"
REQUIRED_OS_VERSION = "24.04"
REQUIRED_ARCHITECTURES = {"amd64", "x86_64"}
REQUIRED_COMMANDS = (
    "curl",
    "dpkg-deb",
    "file",
    "git",
    "lsb_release",
    "python3",
    "sha256sum",
    "tar",
    "timeout",
)
CONTROL_GN_ARGS = {
    "blink_symbol_level": "0",
    "chrome_pgo_phase": "0",
    "is_component_build": "false",
    "is_debug": "false",
    "is_official_build": "false",
    "symbol_level": "0",
    "target_cpu": '"x64"',
    "use_remoteexec": "false",
    "use_siso": "false",
    "use_thin_lto": "false",
    "v8_symbol_level": "0",
}
GN_LINE_PATTERN = re.compile(r"^([a-z][a-z0-9_]*)\s*=\s*(.+?)\s*$")


class BuilderError(ValueError):
    pass


@dataclass(frozen=True)
class SystemProbe:
    os_id: str
    os_version: str
    architecture: str
    cpu_count: int
    memory_bytes: int
    free_disk_bytes: int
    effective_uid: int
    missing_commands: tuple[str, ...]
    workspace: str


def parse_os_release(path: pathlib.Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        values[key] = value.strip().strip('"').strip("'")
    return values


def physical_memory_bytes() -> int:
    try:
        return int(os.sysconf("SC_PAGE_SIZE")) * int(os.sysconf("SC_PHYS_PAGES"))
    except (OSError, TypeError, ValueError):
        return 0


def collect_probe(workspace: pathlib.Path, os_release: pathlib.Path) -> SystemProbe:
    resolved_workspace = workspace.resolve(strict=True)
    if not resolved_workspace.is_dir():
        raise BuilderError("workspace must be an existing directory")
    os_values = parse_os_release(os_release)
    return SystemProbe(
        os_id=os_values.get("ID", ""),
        os_version=os_values.get("VERSION_ID", ""),
        architecture=platform.machine().lower(),
        cpu_count=os.cpu_count() or 0,
        memory_bytes=physical_memory_bytes(),
        free_disk_bytes=shutil.disk_usage(resolved_workspace).free,
        effective_uid=os.geteuid(),
        missing_commands=tuple(
            command for command in REQUIRED_COMMANDS if shutil.which(command) is None
        ),
        workspace=str(resolved_workspace),
    )


def evaluate_probe(probe: SystemProbe) -> list[dict[str, str]]:
    failures: list[dict[str, str]] = []

    def reject(code: str, message: str) -> None:
        failures.append({"code": code, "message": message})

    if probe.os_id != REQUIRED_OS_ID or probe.os_version != REQUIRED_OS_VERSION:
        reject("UNSUPPORTED_OS", "builder must run Ubuntu 24.04")
    if probe.architecture not in REQUIRED_ARCHITECTURES:
        reject("UNSUPPORTED_ARCH", "builder must run native x86_64")
    if probe.cpu_count < MIN_CPU_COUNT:
        reject("INSUFFICIENT_CPU", f"builder needs at least {MIN_CPU_COUNT} logical CPUs")
    if probe.memory_bytes < MIN_MEMORY_BYTES:
        reject("INSUFFICIENT_MEMORY", "builder needs at least 32 GiB physical memory")
    if probe.free_disk_bytes < MIN_FREE_DISK_BYTES:
        reject("INSUFFICIENT_DISK", "builder workspace needs at least 300 GiB free")
    if probe.effective_uid == 0:
        reject("ROOT_RUNNER", "Chromium build and smoke test must run as a non-root user")
    if probe.missing_commands:
        reject(
            "MISSING_COMMANDS",
            "missing required commands: " + ", ".join(probe.missing_commands),
        )
    return failures


def preflight_report(probe: SystemProbe) -> dict[str, Any]:
    failures = evaluate_probe(probe)
    return {
        "schema_version": 1,
        "passed": not failures,
        "requirements": {
            "os_id": REQUIRED_OS_ID,
            "os_version": REQUIRED_OS_VERSION,
            "architectures": sorted(REQUIRED_ARCHITECTURES),
            "minimum_cpu_count": MIN_CPU_COUNT,
            "minimum_memory_bytes": MIN_MEMORY_BYTES,
            "minimum_free_disk_bytes": MIN_FREE_DISK_BYTES,
            "run_as_root": False,
            "commands": list(REQUIRED_COMMANDS),
        },
        "actual": {
            "os_id": probe.os_id,
            "os_version": probe.os_version,
            "architecture": probe.architecture,
            "cpu_count": probe.cpu_count,
            "memory_bytes": probe.memory_bytes,
            "free_disk_bytes": probe.free_disk_bytes,
            "effective_uid": probe.effective_uid,
            "missing_commands": list(probe.missing_commands),
            "workspace": probe.workspace,
        },
        "failures": failures,
    }


def atomic_write_json(path: pathlib.Path, document: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(
        mode="w",
        encoding="utf-8",
        dir=path.parent,
        prefix=f".{path.name}.",
        delete=False,
    ) as handle:
        json.dump(document, handle, indent=2, sort_keys=True)
        handle.write("\n")
        temporary = pathlib.Path(handle.name)
    os.replace(temporary, path)


def load_upstream(repository_root: pathlib.Path) -> dict[str, Any]:
    document = load_json(repository_root / "pins/upstream.json")
    validate_upstream(document)
    return document


def git_output(checkout: pathlib.Path, *arguments: str) -> str:
    try:
        result = subprocess.run(
            ["git", "-C", str(checkout), *arguments],
            check=True,
            capture_output=True,
            text=True,
            timeout=30,
        )
    except (OSError, subprocess.CalledProcessError, subprocess.TimeoutExpired) as error:
        raise BuilderError(f"unable to inspect Git checkout {checkout}") from error
    return result.stdout.strip()


def verify_checkout(
    checkout: pathlib.Path, expected_url: str, expected_revision: str
) -> dict[str, str]:
    resolved = checkout.resolve(strict=True)
    if not resolved.is_dir():
        raise BuilderError(f"checkout is not a directory: {checkout}")
    actual_url = git_output(resolved, "remote", "get-url", "origin")
    actual_revision = git_output(resolved, "rev-parse", "HEAD")
    if actual_url != expected_url:
        raise BuilderError(f"unexpected origin for {checkout}: {actual_url}")
    if actual_revision != expected_revision:
        raise BuilderError(f"unexpected revision for {checkout}: {actual_revision}")
    return {
        "path": str(resolved),
        "url": actual_url,
        "revision": actual_revision,
    }


def verify_checkouts(
    repository_root: pathlib.Path,
    chromium_source: pathlib.Path,
    depot_tools: pathlib.Path,
) -> dict[str, dict[str, str]]:
    upstream = load_upstream(repository_root)
    return {
        "chromium": verify_checkout(
            chromium_source,
            upstream["chromium"]["url"],
            upstream["chromium"]["revision"],
        ),
        "depot_tools": verify_checkout(
            depot_tools,
            upstream["depot_tools"]["url"],
            upstream["depot_tools"]["revision"],
        ),
    }


def parse_gn_args(path: pathlib.Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line_number, raw_line in enumerate(
        path.read_text(encoding="utf-8").splitlines(), start=1
    ):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        match = GN_LINE_PATTERN.fullmatch(line)
        if match is None:
            raise BuilderError(f"invalid GN argument on line {line_number}")
        key, value = match.groups()
        if key in values:
            raise BuilderError(f"duplicate GN argument: {key}")
        values[key] = value
    return values


def validate_control_gn_args(values: dict[str, str]) -> None:
    if values != CONTROL_GN_ARGS:
        missing = sorted(set(CONTROL_GN_ARGS) - set(values))
        unexpected = sorted(set(values) - set(CONTROL_GN_ARGS))
        changed = sorted(
            key
            for key in set(values) & set(CONTROL_GN_ARGS)
            if values[key] != CONTROL_GN_ARGS[key]
        )
        details = []
        if missing:
            details.append("missing=" + ",".join(missing))
        if unexpected:
            details.append("unexpected=" + ",".join(unexpected))
        if changed:
            details.append("changed=" + ",".join(changed))
        raise BuilderError("control GN arguments changed: " + "; ".join(details))


def sha256_file(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def build_manifest(
    repository_root: pathlib.Path,
    checkouts: dict[str, dict[str, str]],
    gn_args_path: pathlib.Path,
    artifact: pathlib.Path,
    preflight_path: pathlib.Path,
    version_path: pathlib.Path,
) -> dict[str, Any]:
    upstream = load_upstream(repository_root)
    gn_args = parse_gn_args(gn_args_path)
    validate_control_gn_args(gn_args)
    if not artifact.is_file() or artifact.stat().st_size == 0:
        raise BuilderError("control .deb artifact is missing or empty")
    if artifact.suffix != ".deb":
        raise BuilderError("control artifact must be a Debian package")

    preflight = load_json(preflight_path)
    if preflight.get("schema_version") != 1 or preflight.get("passed") is not True:
        raise BuilderError("a passing builder preflight report is required")

    version = version_path.read_text(encoding="utf-8").strip()
    if not version.startswith("Chromium ") or len(version) > 128 or "\n" in version:
        raise BuilderError("unexpected Chromium smoke-test version")

    return {
        "schema_version": 1,
        "created_at": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "kind": "upstream-chromium-control",
        "source": {
            "chromium": {
                "url": upstream["chromium"]["url"],
                "ref": upstream["chromium"]["ref"],
                "version": upstream["chromium"]["version"],
                "revision": checkouts["chromium"]["revision"],
            },
            "depot_tools": {
                "url": upstream["depot_tools"]["url"],
                "revision": checkouts["depot_tools"]["revision"],
            },
        },
        "build": {
            "target": "chrome/installer/linux:stable_deb",
            "cpu_profile": "generic-x86_64",
            "overlay_applied": False,
            "direct_patches_applied": False,
            "partition_allocator": "chromium-default",
            "pgo": False,
            "thin_lto": False,
            "gn_args": dict(sorted(gn_args.items())),
            "gn_args_sha256": sha256_file(gn_args_path),
        },
        "verification": {
            "preflight_passed": True,
            "preflight_sha256": sha256_file(preflight_path),
            "sandbox_bypass_used": False,
            "smoke_test_passed": True,
            "version": version,
        },
        "artifact": {
            "filename": artifact.name,
            "bytes": artifact.stat().st_size,
            "sha256": sha256_file(artifact),
        },
    }


def export_pins(repository_root: pathlib.Path, github_env: pathlib.Path) -> None:
    upstream = load_upstream(repository_root)
    values = {
        "CHROMIUM_REF": upstream["chromium"]["ref"],
        "CHROMIUM_REVISION": upstream["chromium"]["revision"],
        "CHROMIUM_URL": upstream["chromium"]["url"],
        "CHROMIUM_VERSION": upstream["chromium"]["version"],
        "DEPOT_TOOLS_REVISION": upstream["depot_tools"]["revision"],
        "DEPOT_TOOLS_URL": upstream["depot_tools"]["url"],
    }
    with github_env.open("a", encoding="utf-8") as handle:
        for key, value in sorted(values.items()):
            if "\n" in value or "\r" in value:
                raise BuilderError(f"unsafe newline in pin value: {key}")
            handle.write(f"{key}={value}\n")


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    preflight = subparsers.add_parser("preflight")
    preflight.add_argument("--workspace", type=pathlib.Path, required=True)
    preflight.add_argument(
        "--os-release", type=pathlib.Path, default=pathlib.Path("/etc/os-release")
    )
    preflight.add_argument("--output", type=pathlib.Path, required=True)

    verify = subparsers.add_parser("verify-checkouts")
    verify.add_argument("--repository-root", type=pathlib.Path, required=True)
    verify.add_argument("--chromium-source", type=pathlib.Path, required=True)
    verify.add_argument("--depot-tools", type=pathlib.Path, required=True)
    verify.add_argument("--output", type=pathlib.Path, required=True)

    manifest = subparsers.add_parser("manifest")
    manifest.add_argument("--repository-root", type=pathlib.Path, required=True)
    manifest.add_argument("--chromium-source", type=pathlib.Path, required=True)
    manifest.add_argument("--depot-tools", type=pathlib.Path, required=True)
    manifest.add_argument("--gn-args", type=pathlib.Path, required=True)
    manifest.add_argument("--artifact", type=pathlib.Path, required=True)
    manifest.add_argument("--preflight", type=pathlib.Path, required=True)
    manifest.add_argument("--version", type=pathlib.Path, required=True)
    manifest.add_argument("--output", type=pathlib.Path, required=True)

    pins = subparsers.add_parser("export-pins")
    pins.add_argument("--repository-root", type=pathlib.Path, required=True)
    pins.add_argument("--github-env", type=pathlib.Path, required=True)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = create_parser().parse_args(argv)
    try:
        if args.command == "preflight":
            report = preflight_report(collect_probe(args.workspace, args.os_release))
            atomic_write_json(args.output, report)
            print(json.dumps(report, sort_keys=True))
            return 0 if report["passed"] else 1
        if args.command == "verify-checkouts":
            report = {
                "schema_version": 1,
                "checkouts": verify_checkouts(
                    args.repository_root, args.chromium_source, args.depot_tools
                ),
            }
            atomic_write_json(args.output, report)
            return 0
        if args.command == "manifest":
            checkouts = verify_checkouts(
                args.repository_root, args.chromium_source, args.depot_tools
            )
            manifest = build_manifest(
                args.repository_root,
                checkouts,
                args.gn_args,
                args.artifact,
                args.preflight,
                args.version,
            )
            atomic_write_json(args.output, manifest)
            return 0
        if args.command == "export-pins":
            export_pins(args.repository_root, args.github_env)
            return 0
        raise BuilderError("unknown command")
    except (BuilderError, PinError, OSError, json.JSONDecodeError) as error:
        print(f"chromium-builder: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
