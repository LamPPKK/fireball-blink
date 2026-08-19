#!/usr/bin/env python3
"""Compile the standalone overlay policy test without a Chromium checkout."""

from __future__ import annotations

import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


PAYLOAD_SIZE = 8 * 1024 * 1024
PAYLOAD = bytes((index * 31 + 17) % 251 for index in range(PAYLOAD_SIZE))


class RangeRequestHandler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def do_GET(self) -> None:  # noqa: N802 - BaseHTTPRequestHandler contract
        if self.path != "/fireball-range.bin":
            self.send_error(404)
            return

        start = 0
        end = len(PAYLOAD) - 1
        response_status = 200
        range_header = self.headers.get("Range")
        if range_header:
            match = re.fullmatch(r"bytes=(\d+)-(\d*)", range_header)
            if not match:
                self.send_error(416)
                return
            start = int(match.group(1))
            end = int(match.group(2)) if match.group(2) else end
            if start >= len(PAYLOAD) or end < start:
                self.send_error(416)
                return
            end = min(end, len(PAYLOAD) - 1)
            response_status = 206
            with self.server.counter_lock:  # type: ignore[attr-defined]
                self.server.range_requests += 1  # type: ignore[attr-defined]

        with self.server.counter_lock:  # type: ignore[attr-defined]
            self.server.total_requests += 1  # type: ignore[attr-defined]
        body = memoryview(PAYLOAD)[start : end + 1]
        self.send_response(response_status)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Accept-Ranges", "bytes")
        self.send_header("Content-Length", str(len(body)))
        if response_status == 206:
            self.send_header(
                "Content-Range", f"bytes {start}-{end}/{len(PAYLOAD)}"
            )
        self.send_header("Connection", "close")
        self.end_headers()
        try:
            for offset in range(0, len(body), 64 * 1024):
                self.wfile.write(body[offset : offset + 64 * 1024])
                self.wfile.flush()
                time.sleep(0.006)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def log_message(self, format: str, *args: object) -> None:
        del format, args


class RangeServer(ThreadingHTTPServer):
    def __init__(self) -> None:
        super().__init__(("127.0.0.1", 0), RangeRequestHandler)
        self.counter_lock = threading.Lock()
        self.range_requests = 0
        self.total_requests = 0


def main() -> int:
    root = pathlib.Path(__file__).resolve().parents[1]
    compiler = os.environ.get("CXX") or shutil.which("c++")
    if compiler is None:
        print("fireball-cpp-tests: no C++ compiler found", file=sys.stderr)
        return 1
    with tempfile.TemporaryDirectory(prefix="fireball-blink-tests-") as temporary:
        cases = {
            "network_audit_test": [
                "fireball/components/privacy/network_audit.cc",
                "tests/network_audit_test.cc",
            ],
            "browser_domain_test": [
                "fireball/browser/domain_model.cc",
                "tests/browser_domain_test.cc",
            ],
            "adblock_profile_policy_test": [
                "fireball/browser/domain_model.cc",
                "fireball/components/adblock/profile_policy.cc",
                "tests/adblock_profile_policy_test.cc",
            ],
            "adblock_ffi_header_test": [
                "tests/adblock_ffi_header_test.cc",
            ],
            "transfer_test": [
                "fireball/components/transfer/transfer_types.cc",
                "fireball/components/transfer/aria2_rpc_client.cc",
                "tests/transfer_test.cc",
            ],
        }
        for name, sources in cases.items():
            binary = pathlib.Path(temporary) / name
            subprocess.run(
                [
                    compiler,
                    "-std=c++20",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    f"-I{root}",
                    *(str(root / source) for source in sources),
                    "-o",
                    str(binary),
                ],
                check=True,
            )
            subprocess.run([str(binary)], check=True)

        aria2 = os.environ.get("ARIA2C") or shutil.which("aria2c")
        if aria2 is None:
            print("fireball-cpp-tests: aria2c is required", file=sys.stderr)
            return 1
        version = subprocess.run(
            [aria2, "--version"],
            check=True,
            capture_output=True,
            text=True,
        ).stdout.splitlines()[0]
        if version != "aria2 version 1.37.0":
            print(
                f"fireball-cpp-tests: expected aria2 1.37.0, got {version!r}",
                file=sys.stderr,
            )
            return 1
        integration_binary = pathlib.Path(temporary) / "aria2_integration_test"
        subprocess.run(
            [
                compiler,
                "-std=c++20",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-pthread",
                f"-I{root}",
                str(root / "fireball/components/transfer/transfer_types.cc"),
                str(root / "fireball/components/transfer/aria2_rpc_client.cc"),
                str(root / "fireball/components/transfer/aria2_sidecar.cc"),
                str(root / "fireball/components/privacy/network_audit.cc"),
                str(root / "tests/aria2_integration_test.cc"),
                "-o",
                str(integration_binary),
            ],
            check=True,
        )
        server = RangeServer()
        server_thread = threading.Thread(target=server.serve_forever, daemon=True)
        server_thread.start()
        try:
            url = f"http://127.0.0.1:{server.server_port}/fireball-range.bin"
            subprocess.run(
                [str(integration_binary), aria2, url, str(PAYLOAD_SIZE)],
                check=True,
                timeout=30,
            )
        finally:
            server.shutdown()
            server.server_close()
            server_thread.join(timeout=5)
        if server.range_requests < 2:
            print(
                "fireball-cpp-tests: aria2 did not make multiple HTTP range requests",
                file=sys.stderr,
            )
            return 1
        print(
            "fireball-cpp-tests: aria2 integration passed "
            f"({server.range_requests} range requests)"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
