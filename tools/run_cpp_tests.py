#!/usr/bin/env python3
"""Compile the standalone overlay policy test without a Chromium checkout."""

from __future__ import annotations

import os
import pathlib
import re
import select
import shutil
import socket
import socketserver
import subprocess
import sys
import tempfile
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


PAYLOAD_SIZE = 8 * 1024 * 1024
PAYLOAD = bytes((index * 31 + 17) % 251 for index in range(PAYLOAD_SIZE))
HLS_SEGMENT_SIZE = 128 * 1024
HLS_SEGMENT_COUNT = 3
HLS_SEGMENTS = tuple(
    bytes(
        (offset * 17 + index * 29 + 3) % 251
        for offset in range(HLS_SEGMENT_SIZE)
    )
    for index in range(HLS_SEGMENT_COUNT)
)


class RangeRequestHandler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def do_GET(self) -> None:  # noqa: N802 - BaseHTTPRequestHandler contract
        payload = PAYLOAD
        content_type = "application/octet-stream"
        if self.path.startswith("/hls/segment-") and self.path.endswith(".ts"):
            match = re.fullmatch(r"/hls/segment-(\d+)\.ts", self.path)
            if not match or int(match.group(1)) >= len(HLS_SEGMENTS):
                self.send_error(404)
                return
            payload = HLS_SEGMENTS[int(match.group(1))]
            content_type = "video/mp2t"
        elif self.path != "/fireball-range.bin":
            self.send_error(404)
            return

        start = 0
        end = len(payload) - 1
        response_status = 200
        range_header = self.headers.get("Range")
        if range_header:
            match = re.fullmatch(r"bytes=(\d+)-(\d*)", range_header)
            if not match:
                self.send_error(416)
                return
            start = int(match.group(1))
            end = int(match.group(2)) if match.group(2) else end
            if start >= len(payload) or end < start:
                self.send_error(416)
                return
            end = min(end, len(payload) - 1)
            response_status = 206
            with self.server.counter_lock:  # type: ignore[attr-defined]
                self.server.range_requests += 1  # type: ignore[attr-defined]

        with self.server.counter_lock:  # type: ignore[attr-defined]
            self.server.total_requests += 1  # type: ignore[attr-defined]
        body = memoryview(payload)[start : end + 1]
        self.send_response(response_status)
        self.send_header("Content-Type", content_type)
        self.send_header("Accept-Ranges", "bytes")
        self.send_header("Content-Length", str(len(body)))
        if response_status == 206:
            self.send_header(
                "Content-Range", f"bytes {start}-{end}/{len(payload)}"
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


class ConnectProxyHandler(socketserver.StreamRequestHandler):
    def handle(self) -> None:
        request_line = self.rfile.readline(4096)
        try:
            method, target, version = request_line.decode("ascii").strip().split()
        except (UnicodeDecodeError, ValueError):
            return
        if method != "CONNECT" or version not in {"HTTP/1.0", "HTTP/1.1"}:
            self.wfile.write(b"HTTP/1.1 405 Method Not Allowed\r\n\r\n")
            return
        header_bytes = 0
        for _ in range(64):
            line = self.rfile.readline(4096)
            header_bytes += len(line)
            if not line or line in {b"\r\n", b"\n"}:
                break
            if header_bytes > 32 * 1024:
                return
        expected = f"127.0.0.1:{self.server.target_port}"  # type: ignore[attr-defined]
        if target != expected:
            self.wfile.write(b"HTTP/1.1 403 Forbidden\r\n\r\n")
            return
        try:
            upstream = socket.create_connection(
                ("127.0.0.1", self.server.target_port), timeout=2  # type: ignore[attr-defined]
            )
        except OSError:
            self.wfile.write(b"HTTP/1.1 502 Bad Gateway\r\n\r\n")
            return
        with self.server.counter_lock:  # type: ignore[attr-defined]
            self.server.connect_requests += 1  # type: ignore[attr-defined]
        self.wfile.write(b"HTTP/1.1 200 Connection Established\r\n\r\n")
        self.wfile.flush()
        with upstream:
            peers = {self.connection: upstream, upstream: self.connection}
            while True:
                readable, _, _ = select.select(list(peers), [], [], 5)
                if not readable:
                    return
                for source in readable:
                    try:
                        data = source.recv(64 * 1024)
                    except (ConnectionResetError, OSError):
                        return
                    if not data:
                        return
                    try:
                        peers[source].sendall(data)
                    except (BrokenPipeError, ConnectionResetError, OSError):
                        return


class ConnectProxyServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True

    def __init__(self, target_port: int) -> None:
        super().__init__(("127.0.0.1", 0), ConnectProxyHandler)
        self.target_port = target_port
        self.counter_lock = threading.Lock()
        self.connect_requests = 0


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
            "egress_test": [
                "fireball/browser/domain_model.cc",
                "fireball/components/privacy/network_audit.cc",
                "fireball/components/egress/egress_route.cc",
                "fireball/components/egress/egress_controller.cc",
                "fireball/components/egress/runtime_backend.cc",
                "fireball/components/egress/socks5_probe.cc",
                "fireball/components/egress/tor_config.cc",
                "fireball/components/egress/tor_sidecar.cc",
                "fireball/components/egress/warp_local_proxy.cc",
                "tests/egress_test.cc",
            ],
            "tor_sidecar_integration_test": [
                "fireball/browser/domain_model.cc",
                "fireball/components/privacy/network_audit.cc",
                "fireball/components/egress/egress_route.cc",
                "fireball/components/egress/egress_controller.cc",
                "fireball/components/egress/runtime_backend.cc",
                "fireball/components/egress/socks5_probe.cc",
                "fireball/components/egress/tor_config.cc",
                "fireball/components/egress/tor_sidecar.cc",
                "tests/tor_sidecar_integration_test.cc",
            ],
            "transfer_test": [
                "fireball/components/egress/egress_route.cc",
                "fireball/components/transfer/transfer_types.cc",
                "fireball/components/transfer/aria2_rpc_client.cc",
                "fireball/components/transfer/media_discovery.cc",
                "fireball/components/transfer/transfer_queue.cc",
                "fireball/components/transfer/egress_transfer_policy.cc",
                "tests/transfer_test.cc",
            ],
            "hls_vod_test": [
                "fireball/components/transfer/transfer_types.cc",
                "fireball/components/transfer/aria2_rpc_client.cc",
                "fireball/components/transfer/hls_vod.cc",
                "tests/hls_vod_test.cc",
            ],
            "navigation_policy_test": [
                "fireball/browser/domain_model.cc",
                "fireball/components/adblock/profile_policy.cc",
                "fireball/components/egress/egress_route.cc",
                "fireball/components/egress/egress_controller.cc",
                "fireball/components/navigation/url_cleaner.cc",
                "fireball/components/navigation/request_policy.cc",
                "fireball/components/privacy/network_audit.cc",
                "tests/navigation_policy_test.cc",
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
                    "-pthread",
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
                str(root / "fireball/components/transfer/hls_vod.cc"),
                str(root / "fireball/components/transfer/transfer_queue.cc"),
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
        proxy = ConnectProxyServer(server.server_port)
        proxy_thread = threading.Thread(target=proxy.serve_forever, daemon=True)
        proxy_thread.start()
        try:
            url = f"http://127.0.0.1:{server.server_port}/fireball-range.bin"
            subprocess.run(
                [
                    str(integration_binary),
                    aria2,
                    url,
                    str(PAYLOAD_SIZE),
                    str(proxy.server_address[1]),
                    str(HLS_SEGMENT_COUNT),
                    str(HLS_SEGMENT_SIZE),
                ],
                check=True,
                timeout=45,
            )
        finally:
            proxy.shutdown()
            proxy.server_close()
            proxy_thread.join(timeout=5)
            server.shutdown()
            server.server_close()
            server_thread.join(timeout=5)
        if server.range_requests < 2:
            print(
                "fireball-cpp-tests: aria2 did not make multiple HTTP range requests",
                file=sys.stderr,
            )
            return 1
        if proxy.connect_requests < 2:
            print(
                "fireball-cpp-tests: proxied aria2 did not use HTTP CONNECT",
                file=sys.stderr,
            )
            return 1
        print(
            "fireball-cpp-tests: aria2 queue + HLS VOD + HTTP CONNECT passed "
            f"({server.range_requests} range requests, "
            f"{proxy.connect_requests} proxy tunnels)"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
