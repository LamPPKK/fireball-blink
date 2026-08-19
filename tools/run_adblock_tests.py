#!/usr/bin/env python3
"""Build and exercise Fireball's real adblock-rust C ABI."""

from __future__ import annotations

import ctypes
import json
import os
import pathlib
import shutil
import subprocess
import sys


class Decision(ctypes.Structure):
    _fields_ = [
        ("status", ctypes.c_int32),
        ("flags", ctypes.c_uint32),
        ("redirect", ctypes.c_void_p),
        ("rewritten_url", ctypes.c_void_p),
    ]


def byte_buffer(value: bytes) -> ctypes.Array[ctypes.c_uint8]:
    return (ctypes.c_uint8 * len(value)).from_buffer_copy(value)


def shared_library(target: pathlib.Path) -> pathlib.Path:
    if sys.platform == "darwin":
        return target / "debug" / "libfireball_adblock.dylib"
    if sys.platform.startswith("linux"):
        return target / "debug" / "libfireball_adblock.so"
    raise RuntimeError(f"unsupported adblock FFI test platform: {sys.platform}")


def main() -> int:
    root = pathlib.Path(__file__).resolve().parents[1]
    manifest = root / "fireball/components/adblock/rust/Cargo.toml"
    cargo = os.environ.get("FIREBALL_CARGO") or shutil.which("cargo")
    if cargo is None:
        fallback = pathlib.Path.home() / ".cargo/bin/cargo"
        cargo = str(fallback) if fallback.is_file() else None
    if cargo is None:
        print("fireball-adblock-tests: cargo is required", file=sys.stderr)
        return 1

    target = root / "out/cargo-adblock"
    environment = os.environ.copy()
    environment["CARGO_TARGET_DIR"] = str(target)
    subprocess.run([cargo, "fmt", "--manifest-path", str(manifest), "--", "--check"], check=True)
    subprocess.run(
        [cargo, "clippy", "--manifest-path", str(manifest), "--all-targets", "--all-features", "--locked", "--", "-D", "warnings"],
        check=True,
        env=environment,
    )
    subprocess.run(
        [cargo, "test", "--manifest-path", str(manifest), "--all-features", "--locked"],
        check=True,
        env=environment,
    )
    subprocess.run(
        [cargo, "build", "--manifest-path", str(manifest), "--features", "ffi-test", "--locked"],
        check=True,
        env=environment,
    )

    library_path = shared_library(target)
    library = ctypes.CDLL(str(library_path))
    resolver_type = ctypes.CFUNCTYPE(
        ctypes.c_bool,
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_size_t),
        ctypes.POINTER(ctypes.c_size_t),
    )

    @resolver_type
    def resolver(host_data, host_length, domain_start, domain_end):
        hostname = ctypes.string_at(host_data, host_length)
        labels = hostname.split(b".")
        domain = b".".join(labels[-2:]) if len(labels) >= 2 else hostname
        domain_start[0] = len(hostname) - len(domain)
        domain_end[0] = len(hostname)
        return True

    library.fireball_adblock_set_domain_resolver.argtypes = [resolver_type]
    library.fireball_adblock_set_domain_resolver.restype = ctypes.c_bool
    if not library.fireball_adblock_set_domain_resolver(resolver):
        raise RuntimeError("domain resolver registration failed")

    library.fireball_adblock_engine_create_unverified_for_testing.argtypes = [
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.c_size_t,
    ]
    library.fireball_adblock_engine_create_unverified_for_testing.restype = ctypes.c_void_p
    library.fireball_adblock_engine_destroy.argtypes = [ctypes.c_void_p]
    library.fireball_adblock_check_network.argtypes = [
        ctypes.c_void_p,
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.c_size_t,
        ctypes.c_bool,
    ]
    library.fireball_adblock_check_network.restype = Decision
    library.fireball_adblock_cosmetic_resources.argtypes = [
        ctypes.c_void_p,
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.c_size_t,
    ]
    library.fireball_adblock_cosmetic_resources.restype = ctypes.c_void_p
    library.fireball_adblock_hidden_selectors.argtypes = [
        ctypes.c_void_p,
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.c_size_t,
    ]
    library.fireball_adblock_hidden_selectors.restype = ctypes.c_void_p
    library.fireball_adblock_string_destroy.argtypes = [ctypes.c_void_p]

    rules = b"\n".join(
        [
            b"||ads.example^",
            b"@@||ads.example/allowed.js$script,domain=publisher.example",
            b"||tracker.example^$third-party",
            b"publisher.example##.sponsored",
            b"##.global-ad",
        ]
    )
    rules_buffer = byte_buffer(rules)
    engine = library.fireball_adblock_engine_create_unverified_for_testing(
        rules_buffer, len(rules)
    )
    if not engine:
        raise RuntimeError("test engine creation failed")

    def check(url: bytes, hostname: bytes, third_party: bool) -> Decision:
        source = b"publisher.example"
        request_type = b"script"
        method = b"GET"
        buffers = [byte_buffer(value) for value in (url, hostname, source, request_type, method)]
        return library.fireball_adblock_check_network(
            engine,
            buffers[0],
            len(url),
            buffers[1],
            len(hostname),
            buffers[2],
            len(source),
            buffers[3],
            len(request_type),
            buffers[4],
            len(method),
            third_party,
        )

    try:
        blocked = check(b"https://ads.example/banner.js", b"ads.example", True)
        assert blocked.status == 0 and blocked.flags & 1
        allowed = check(b"https://ads.example/allowed.js", b"ads.example", True)
        assert allowed.status == 0 and not (allowed.flags & 1) and allowed.flags & 2
        tracker = check(b"https://tracker.example/pixel.js", b"tracker.example", True)
        first_party = check(b"https://tracker.example/pixel.js", b"tracker.example", False)
        assert tracker.flags & 1 and not (first_party.flags & 1)
        invalid = check(b"file:///etc/passwd", b"localhost", True)
        assert invalid.status == 1 and invalid.flags == 0

        page_url = b"https://publisher.example/article"
        page_buffer = byte_buffer(page_url)
        cosmetic_pointer = library.fireball_adblock_cosmetic_resources(
            engine, page_buffer, len(page_url)
        )
        assert cosmetic_pointer
        try:
            cosmetic = json.loads(ctypes.string_at(cosmetic_pointer))
        finally:
            library.fireball_adblock_string_destroy(cosmetic_pointer)
        assert ".sponsored" in cosmetic["hide_selectors"]

        classes = json.dumps(["global-ad"]).encode()
        ids = b"[]"
        exceptions = json.dumps(cosmetic["exceptions"]).encode()
        class_buffer, id_buffer, exception_buffer = (
            byte_buffer(classes),
            byte_buffer(ids),
            byte_buffer(exceptions),
        )
        selectors_pointer = library.fireball_adblock_hidden_selectors(
            engine,
            class_buffer,
            len(classes),
            id_buffer,
            len(ids),
            exception_buffer,
            len(exceptions),
        )
        assert selectors_pointer
        try:
            selectors = json.loads(ctypes.string_at(selectors_pointer))
        finally:
            library.fireball_adblock_string_destroy(selectors_pointer)
        assert ".global-ad" in selectors
    finally:
        library.fireball_adblock_engine_destroy(engine)

    print("fireball-adblock-tests: Rust engine and C ABI passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
