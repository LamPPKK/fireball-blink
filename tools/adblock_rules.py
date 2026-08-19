#!/usr/bin/env python3
"""Build and verify deterministic, Ed25519-signed Fireball rule artifacts."""

from __future__ import annotations

import argparse
import base64
import datetime
import hashlib
import json
import os
import pathlib
import re
import shutil
import stat
import subprocess
import tempfile
import urllib.parse
from typing import Any, NoReturn


MAX_RULES_BYTES = 16 * 1024 * 1024
MAX_MANIFEST_BYTES = 64 * 1024
MAX_SOURCE_COUNT = 16
MAX_LINE_BYTES = 64 * 1024
ENGINE_NAME = "adblock-rust"
ENGINE_VERSION = "0.13.2"
SIGNING_CONTEXT = "fireball-adblock-rules-v1-signature-v2"
TOKEN = re.compile(r"^[A-Za-z0-9._-]{1,64}$")
SHA1 = re.compile(r"^[0-9a-f]{40}$")
SHA256 = re.compile(r"^[0-9a-f]{64}$")
SEMVER = re.compile(r"^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$")
ED25519_SPKI_PREFIX = bytes.fromhex("302a300506032b6570032100")
SOURCE_FIELDS = {"name", "url", "revision", "license", "path", "sha256"}
MANIFEST_FIELDS = {
    "schema_version",
    "created_at",
    "minimum_app_version",
    "engine",
    "artifact",
    "signature",
    "sources",
}


class RulesBuildError(ValueError):
    pass


def fail(message: str) -> NoReturn:
    raise RulesBuildError(message)


def sha256_hex(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def strict_https_url(value: object, field: str) -> str:
    if not isinstance(value, str) or len(value.encode("utf-8")) > 8192:
        fail(f"{field}: bounded HTTPS URL is required")
    try:
        parsed = urllib.parse.urlsplit(value)
        port = parsed.port
    except ValueError as error:
        raise RulesBuildError(f"{field}: valid HTTPS URL is required") from error
    if (
        parsed.scheme != "https"
        or not parsed.hostname
        or parsed.username is not None
        or parsed.password is not None
        or "\\" in value
        or parsed.query
        or parsed.fragment
        or (port is not None and not 1 <= port <= 65535)
        or any(ord(character) <= 0x20 or ord(character) == 0x7F for character in value)
    ):
        fail(f"{field}: credentials, query, fragment and controls are forbidden")
    return value


def strict_timestamp(value: object) -> str:
    if not isinstance(value, str) or len(value) != 20 or not value.endswith("Z"):
        fail("created_at: canonical UTC timestamp is required")
    try:
        parsed = datetime.datetime.strptime(value, "%Y-%m-%dT%H:%M:%SZ")
    except ValueError as error:
        raise RulesBuildError("created_at: invalid UTC calendar timestamp") from error
    if parsed.strftime("%Y-%m-%dT%H:%M:%SZ") != value:
        fail("created_at: non-canonical UTC timestamp")
    return value


def strict_semver(value: object, field: str) -> str:
    if not isinstance(value, str) or SEMVER.fullmatch(value) is None:
        fail(f"{field}: canonical semantic version is required")
    return value


def load_json_object(path: pathlib.Path, maximum_bytes: int) -> dict[str, Any]:
    try:
        payload = path.read_bytes()
    except OSError as error:
        raise RulesBuildError(f"{path.name}: cannot read file") from error
    if not payload or len(payload) > maximum_bytes or b"\x00" in payload:
        fail(f"{path.name}: invalid size or NUL byte")
    try:
        document = json.loads(payload.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise RulesBuildError(f"{path.name}: valid UTF-8 JSON is required") from error
    if not isinstance(document, dict):
        fail(f"{path.name}: root must be an object")
    return document


def source_path(lock_root: pathlib.Path, value: object) -> pathlib.Path:
    if not isinstance(value, str) or not value or "\\" in value:
        fail("source.path: non-empty relative POSIX path is required")
    relative = pathlib.PurePosixPath(value)
    if relative.is_absolute() or any(part in {"", ".", ".."} for part in relative.parts):
        fail("source.path: traversal and absolute paths are forbidden")
    candidate = lock_root.joinpath(*relative.parts)
    current = lock_root
    for part in relative.parts:
        current = current / part
        if current.is_symlink():
            fail("source.path: symlinks are forbidden")
    try:
        resolved = candidate.resolve(strict=True)
        resolved.relative_to(lock_root.resolve(strict=True))
    except (OSError, ValueError) as error:
        raise RulesBuildError("source.path: file must stay under lock directory") from error
    if not resolved.is_file():
        fail("source.path: regular file is required")
    return resolved


def validate_source_lock(path: pathlib.Path) -> list[dict[str, str]]:
    document = load_json_object(path, MAX_MANIFEST_BYTES)
    if set(document) != {"schema_version", "sources"} or document.get("schema_version") != 1:
        fail("source lock: exact schema_version 1 shape is required")
    raw_sources = document["sources"]
    if (
        not isinstance(raw_sources, list)
        or not raw_sources
        or len(raw_sources) > MAX_SOURCE_COUNT
    ):
        fail("source lock: one to sixteen sources are required")
    sources: list[dict[str, str]] = []
    names: set[str] = set()
    for index, raw in enumerate(raw_sources):
        if not isinstance(raw, dict) or set(raw) != SOURCE_FIELDS:
            fail(f"source[{index}]: exact field set is required")
        name = raw["name"]
        revision = raw["revision"]
        license_value = raw["license"]
        checksum = raw["sha256"]
        if not isinstance(name, str) or TOKEN.fullmatch(name) is None or name in names:
            fail(f"source[{index}].name: unique ASCII token is required")
        if not isinstance(revision, str) or SHA1.fullmatch(revision) is None:
            fail(f"source[{index}].revision: exact SHA-1 is required")
        if not isinstance(license_value, str) or TOKEN.fullmatch(license_value) is None:
            fail(f"source[{index}].license: SPDX-style ASCII token is required")
        if not isinstance(checksum, str) or SHA256.fullmatch(checksum) is None:
            fail(f"source[{index}].sha256: exact SHA-256 is required")
        source = {
            "name": name,
            "url": strict_https_url(raw["url"], f"source[{index}].url"),
            "revision": revision,
            "license": license_value,
            "path": str(raw["path"]),
            "sha256": checksum,
        }
        source["resolved_path"] = str(source_path(path.parent, raw["path"]))
        names.add(name)
        sources.append(source)
    sources.sort(key=lambda source: source["name"])
    return sources


def normalize_source(source: dict[str, str]) -> list[str]:
    path = pathlib.Path(source["resolved_path"])
    try:
        payload = path.read_bytes()
    except OSError as error:
        raise RulesBuildError(f"{source['name']}: cannot read source") from error
    if not payload or len(payload) > MAX_RULES_BYTES or sha256_hex(payload) != source["sha256"]:
        fail(f"{source['name']}: source size/checksum mismatch")
    try:
        text = payload.decode("utf-8-sig")
    except UnicodeDecodeError as error:
        raise RulesBuildError(f"{source['name']}: source must be UTF-8") from error
    if "\x00" in text:
        fail(f"{source['name']}: NUL byte is forbidden")
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    lines = text.split("\n")
    if lines and lines[0].startswith("[Adblock Plus ") and lines[0].endswith("]"):
        lines.pop(0)
    for line in lines:
        if len(line.encode("utf-8")) > MAX_LINE_BYTES:
            fail(f"{source['name']}: rule line exceeds {MAX_LINE_BYTES} bytes")
        if line.lstrip().lower().startswith(("!#include", "!#if", "!#endif")):
            fail(f"{source['name']}: unresolved preprocessor directive")
    while lines and lines[-1] == "":
        lines.pop()
    if not any(
        line.lstrip()
        and not line.lstrip().startswith("!")
        and not line.lstrip().startswith("[")
        for line in lines
    ):
        fail(f"{source['name']}: no filter rules found")
    return lines


def compile_rules(sources: list[dict[str, str]]) -> bytes:
    lines = [
        "[Adblock Plus 2.0]",
        "! Fireball deterministic rules artifact v1",
    ]
    for source in sources:
        lines.append(f"! fireball-source {source['name']} {source['revision']}")
        lines.extend(normalize_source(source))
    artifact = ("\n".join(lines) + "\n").encode("utf-8")
    if not artifact or len(artifact) > MAX_RULES_BYTES:
        fail("compiled rules: artifact exceeds 16 MiB")
    return artifact


def signing_message(manifest: dict[str, Any]) -> bytes:
    fields: list[str] = [
        SIGNING_CONTEXT,
        str(manifest["schema_version"]),
        manifest["created_at"],
        manifest["minimum_app_version"],
        manifest["engine"]["name"],
        manifest["engine"]["version"],
        manifest["artifact"]["url"],
        str(manifest["artifact"]["size"]),
        manifest["artifact"]["sha256"],
        manifest["signature"]["algorithm"],
        manifest["signature"]["key_id"],
        str(len(manifest["sources"])),
    ]
    for source in manifest["sources"]:
        fields.extend(
            [source["name"], source["url"], source["revision"], source["license"]]
        )
    return ("\n".join(fields) + "\n").encode("utf-8")


def openssl_command(openssl: str, arguments: list[str], payload: bytes | None = None) -> bytes:
    operation = " ".join(
        argument for argument in arguments[:2] if not argument.startswith("-")
    ) or (arguments[0] if arguments else "command")
    if "-sign" in arguments:
        operation = "pkeyutl sign"
    elif "-verify" in arguments:
        operation = "pkeyutl verify"
    try:
        result = subprocess.run(
            [openssl, *arguments],
            input=payload,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
            timeout=15,
            env={**os.environ, "LC_ALL": "C"},
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        raise RulesBuildError(f"OpenSSL {operation} operation failed") from error
    if result.returncode != 0:
        fail(f"OpenSSL {operation} operation failed")
    return result.stdout


def validate_private_key(path: pathlib.Path) -> None:
    if path.is_symlink():
        fail("signing key: symlinks are forbidden")
    try:
        metadata = path.stat()
    except OSError as error:
        raise RulesBuildError("signing key: cannot stat file") from error
    if (
        not stat.S_ISREG(metadata.st_mode)
        or metadata.st_size <= 0
        or metadata.st_size > 16 * 1024
        or metadata.st_mode & 0o077
    ):
        fail("signing key: regular root/user-only file is required")


def public_key_from_private(openssl: str, key_path: pathlib.Path) -> bytes:
    validate_private_key(key_path)
    encoded = openssl_command(
        openssl,
        ["pkey", "-in", str(key_path), "-passin", "pass:", "-pubout", "-outform", "DER"],
    )
    if len(encoded) != len(ED25519_SPKI_PREFIX) + 32 or not encoded.startswith(
        ED25519_SPKI_PREFIX
    ):
        fail("signing key: unencrypted Ed25519 key is required")
    return encoded[len(ED25519_SPKI_PREFIX) :]


def sign_message(openssl: str, key_path: pathlib.Path, message: bytes) -> bytes:
    with tempfile.TemporaryDirectory(prefix="fireball-adblock-sign-") as temporary:
        message_path = pathlib.Path(temporary) / "message.bin"
        message_path.write_bytes(message)
        signature = openssl_command(
            openssl,
            [
                "pkeyutl",
                "-sign",
                "-rawin",
                "-inkey",
                str(key_path),
                "-passin",
                "pass:",
                "-in",
                str(message_path),
            ],
        )
    if len(signature) != 64:
        fail("signing key: Ed25519 signature must be 64 bytes")
    return signature


def manifest_sources(sources: list[dict[str, str]]) -> list[dict[str, str]]:
    return [
        {
            "name": source["name"],
            "url": source["url"],
            "revision": source["revision"],
            "license": source["license"],
        }
        for source in sources
    ]


def validate_manifest(document: dict[str, Any], rules: bytes, public_key: bytes) -> None:
    if set(document) != MANIFEST_FIELDS or document.get("schema_version") != 1:
        fail("manifest: exact schema_version 1 shape is required")
    strict_timestamp(document.get("created_at"))
    strict_semver(document.get("minimum_app_version"), "minimum_app_version")
    engine = document.get("engine")
    if engine != {"name": ENGINE_NAME, "version": ENGINE_VERSION}:
        fail("manifest.engine: pinned engine is required")
    artifact = document.get("artifact")
    if not isinstance(artifact, dict) or set(artifact) != {"url", "size", "sha256"}:
        fail("manifest.artifact: exact field set is required")
    strict_https_url(artifact["url"], "manifest.artifact.url")
    if (
        not isinstance(artifact["size"], int)
        or isinstance(artifact["size"], bool)
        or not 1 <= artifact["size"] <= MAX_RULES_BYTES
        or not isinstance(artifact["sha256"], str)
        or SHA256.fullmatch(artifact["sha256"]) is None
    ):
        fail("manifest.artifact: bounded size and canonical SHA-256 are required")
    if artifact["size"] != len(rules) or artifact["sha256"] != sha256_hex(rules):
        fail("manifest.artifact: size/checksum mismatch")
    signature = document.get("signature")
    if not isinstance(signature, dict) or set(signature) != {"algorithm", "key_id", "value"}:
        fail("manifest.signature: exact field set is required")
    if signature["algorithm"] != "Ed25519" or signature["key_id"] != sha256_hex(public_key):
        fail("manifest.signature: algorithm/key mismatch")
    try:
        signature_bytes = base64.b64decode(signature["value"], validate=True)
    except (KeyError, TypeError, ValueError) as error:
        raise RulesBuildError("manifest.signature: canonical Base64 is required") from error
    if len(signature_bytes) != 64 or base64.b64encode(signature_bytes).decode("ascii") != signature["value"]:
        fail("manifest.signature: canonical 64-byte signature is required")
    sources = document.get("sources")
    if not isinstance(sources, list) or not sources or len(sources) > MAX_SOURCE_COUNT:
        fail("manifest.sources: one to sixteen entries are required")
    previous_name = ""
    for index, source in enumerate(sources):
        if not isinstance(source, dict) or set(source) != {"name", "url", "revision", "license"}:
            fail(f"manifest.sources[{index}]: exact field set is required")
        name = source["name"]
        if not isinstance(name, str) or TOKEN.fullmatch(name) is None or name <= previous_name:
            fail("manifest.sources: names must be sorted and unique")
        strict_https_url(source["url"], f"manifest.sources[{index}].url")
        if not isinstance(source["revision"], str) or SHA1.fullmatch(source["revision"]) is None:
            fail(f"manifest.sources[{index}].revision: exact SHA-1 is required")
        if not isinstance(source["license"], str) or TOKEN.fullmatch(source["license"]) is None:
            fail(f"manifest.sources[{index}].license: ASCII token is required")
        previous_name = name


def verify_signature(
    openssl: str, manifest: dict[str, Any], public_key: bytes
) -> None:
    signature = base64.b64decode(manifest["signature"]["value"], validate=True)
    with tempfile.TemporaryDirectory(prefix="fireball-adblock-verify-") as temporary:
        root = pathlib.Path(temporary)
        public_der = root / "public.der"
        signature_path = root / "signature.bin"
        message_path = root / "message.bin"
        public_der.write_bytes(ED25519_SPKI_PREFIX + public_key)
        signature_path.write_bytes(signature)
        message_path.write_bytes(signing_message(manifest))
        openssl_command(
            openssl,
            [
                "pkeyutl",
                "-verify",
                "-rawin",
                "-pubin",
                "-keyform",
                "DER",
                "-inkey",
                str(public_der),
                "-sigfile",
                str(signature_path),
                "-in",
                str(message_path),
            ],
        )


def write_new(path: pathlib.Path, payload: bytes, mode: int) -> None:
    if path.exists() or path.is_symlink() or not path.parent.is_dir():
        fail(f"{path.name}: output must be new and parent must exist")
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{path.name}.", dir=path.parent
    )
    temporary = pathlib.Path(temporary_name)
    try:
        os.fchmod(descriptor, mode)
        with os.fdopen(descriptor, "wb") as handle:
            handle.write(payload)
            handle.flush()
            os.fsync(handle.fileno())
        os.link(temporary, path, follow_symlinks=False)
        temporary.unlink()
    except BaseException:
        temporary.unlink(missing_ok=True)
        raise


def build(arguments: argparse.Namespace) -> None:
    openssl = arguments.openssl or shutil.which("openssl")
    if not openssl:
        fail("OpenSSL 3 with Ed25519 support is required")
    sources = validate_source_lock(arguments.source_lock.resolve())
    rules = compile_rules(sources)
    public_key = public_key_from_private(openssl, arguments.signing_key)
    manifest: dict[str, Any] = {
        "schema_version": 1,
        "created_at": strict_timestamp(arguments.created_at),
        "minimum_app_version": strict_semver(
            arguments.minimum_app_version, "minimum_app_version"
        ),
        "engine": {"name": ENGINE_NAME, "version": ENGINE_VERSION},
        "artifact": {
            "url": strict_https_url(arguments.artifact_url, "artifact_url"),
            "size": len(rules),
            "sha256": sha256_hex(rules),
        },
        "signature": {
            "algorithm": "Ed25519",
            "key_id": sha256_hex(public_key),
            "value": "",
        },
        "sources": manifest_sources(sources),
    }
    manifest["signature"]["value"] = base64.b64encode(
        sign_message(openssl, arguments.signing_key, signing_message(manifest))
    ).decode("ascii")
    validate_manifest(manifest, rules, public_key)
    verify_signature(openssl, manifest, public_key)
    encoded_manifest = (
        json.dumps(manifest, ensure_ascii=True, separators=(",", ":"), sort_keys=True)
        + "\n"
    ).encode("utf-8")
    if len(encoded_manifest) > MAX_MANIFEST_BYTES:
        fail("manifest: output exceeds 64 KiB")
    outputs = {
        arguments.output_rules.resolve(),
        arguments.output_manifest.resolve(),
        arguments.output_public_key.resolve(),
    }
    if len(outputs) != 3:
        fail("outputs: rules, manifest and public key paths must differ")
    created_outputs: list[pathlib.Path] = []
    try:
        write_new(arguments.output_rules, rules, 0o644)
        created_outputs.append(arguments.output_rules)
        write_new(arguments.output_manifest, encoded_manifest, 0o644)
        created_outputs.append(arguments.output_manifest)
        write_new(arguments.output_public_key, public_key, 0o644)
        created_outputs.append(arguments.output_public_key)
    except BaseException:
        for output in created_outputs:
            output.unlink(missing_ok=True)
        raise


def verify(arguments: argparse.Namespace) -> None:
    openssl = arguments.openssl or shutil.which("openssl")
    if not openssl:
        fail("OpenSSL 3 with Ed25519 support is required")
    try:
        rules = arguments.rules.read_bytes()
        public_key = arguments.public_key.read_bytes()
    except OSError as error:
        raise RulesBuildError("verification input cannot be read") from error
    if not rules or len(rules) > MAX_RULES_BYTES:
        fail("rules: artifact size is invalid")
    if len(public_key) != 32:
        fail("public key: exact 32-byte Ed25519 key is required")
    manifest = load_json_object(arguments.manifest, MAX_MANIFEST_BYTES)
    validate_manifest(manifest, rules, public_key)
    verify_signature(openssl, manifest, public_key)


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    subparsers = result.add_subparsers(dest="command", required=True)
    build_parser = subparsers.add_parser("build")
    build_parser.add_argument("--source-lock", required=True, type=pathlib.Path)
    build_parser.add_argument("--signing-key", required=True, type=pathlib.Path)
    build_parser.add_argument("--artifact-url", required=True)
    build_parser.add_argument("--minimum-app-version", required=True)
    build_parser.add_argument("--created-at", required=True)
    build_parser.add_argument("--output-rules", required=True, type=pathlib.Path)
    build_parser.add_argument("--output-manifest", required=True, type=pathlib.Path)
    build_parser.add_argument("--output-public-key", required=True, type=pathlib.Path)
    build_parser.add_argument("--openssl")
    build_parser.set_defaults(handler=build)
    verify_parser = subparsers.add_parser("verify")
    verify_parser.add_argument("--rules", required=True, type=pathlib.Path)
    verify_parser.add_argument("--manifest", required=True, type=pathlib.Path)
    verify_parser.add_argument("--public-key", required=True, type=pathlib.Path)
    verify_parser.add_argument("--openssl")
    verify_parser.set_defaults(handler=verify)
    return result


def main() -> int:
    arguments = parser().parse_args()
    try:
        arguments.handler(arguments)
    except (OSError, RulesBuildError) as error:
        print(f"fireball-adblock-rules: {error}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
