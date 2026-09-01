#!/usr/bin/env python3
"""
Fireball Unified Automated Test Suite Runner (Unit, Integration, and UI/UX State Tests)
Runs automated verification across all 5 ecosystem components:
1. Fireball Chromium Browser (Python, C++, Rust Adblock)
2. Fireball Server (Beam Protocol, Sync Relay, Media Remuxer)
3. Fireball Client (Desktop Thin Client, Web Client Frame Protocol)
4. Fireball Mini Browser (Android Kotlin, Apple Swift, Windows C++20)
5. Fireball Extension Suite (9 WebExtensions + Ruffle + Tampermonkey + uBlock)
"""

from __future__ import annotations

import os
import pathlib
import subprocess
import sys
import time
from typing import List, Tuple


def run_stage(title: str, command: List[str], cwd: pathlib.Path) -> Tuple[bool, float, str]:
    print(f"\n==========================================")
    print(f"🚀 Running Stage: {title}")
    print(f"📂 Working Dir: {cwd.relative_to(pathlib.Path.cwd()) if cwd != pathlib.Path.cwd() else '.'}")
    print(f"⚙️ Command: {' '.join(command)}")
    print(f"==========================================")

    start = time.perf_counter()
    try:
        proc = subprocess.run(
            command,
            cwd=cwd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            check=False
        )
        duration = time.perf_counter() - start
        print(proc.stdout)
        if proc.returncode == 0:
            print(f"✅ PASSED ({duration:.2f}s): {title}")
            return True, duration, ""
        else:
            print(f"❌ FAILED with code {proc.returncode} ({duration:.2f}s): {title}")
            return False, duration, proc.stdout
    except Exception as e:
        duration = time.perf_counter() - start
        print(f"❌ ERROR ({duration:.2f}s): {e}")
        return False, duration, str(e)


def main() -> int:
    repo_root = pathlib.Path(__file__).resolve().parent.parent

    stages = [
        # 1. Chromium Hardened Architecture & Security Tests (Python)
        ("Fireball Architecture & Security Unit Tests", ["python3", "-m", "unittest", "discover", "-s", "tests"], repo_root),
        
        # 2. C++ Hardened Engine Tests (Aria2 + HLS + DASH + Proxy Tunnels)
        ("C++ Networking & Aria2 VOD Unit Tests", ["python3", "tools/run_cpp_tests.py"], repo_root),
        
        # 3. Rust Adblock Engine Tests (adblock-rust + C ABI)
        ("Rust Adblock Engine & C ABI Tests", ["python3", "tools/run_adblock_tests.py"], repo_root),
        
        # 4. Fireball Server & Streaming Daemons
        ("Fireball Server Beam Protocol Codec Tests", ["python3", "fireball-server/test_beam_protocol.py"], repo_root),
        ("Fireball Server BIP-39 Sync Relay Self-Test", ["python3", "fireball-server/sync_relay.py"], repo_root),
        ("Fireball Server HLS/MP4 Media Remuxer Self-Test", ["python3", "fireball-server/media_remuxer.py"], repo_root),
        
        # 5. Fireball Client Tests
        ("Fireball Desktop Client Tests", ["python3", "fireball-client/desktop/test_client.py"], repo_root),
        
        # 6. Fireball Extension & Web Client Tests (Node.js)
        ("Fireball Extension & Web Client UI/UX Tests", ["node", "fireball-extension/test/extension_test.js"], repo_root),
        
        # 7. Fireball Mini Apple iOS / macOS Tests (Swift 6)
        ("Fireball Mini Apple Swift Core Tests", ["swift", "run", "--package-path", "fireball-mini/ios", "FireballTestRunner"], repo_root),
        
        # 8. Fireball Mini Windows Lite Tests (C++20 Win32 / WebView2)
        ("Fireball Mini Windows Lite C++20 Tests", ["sh", "-c", "mkdir -p out && c++ -std=c++20 -Wall -Wextra -Werror -I./fireball-mini/windows/include fireball-mini/windows/src/domain_models.cpp fireball-mini/windows/src/search_engines.cpp fireball-mini/windows/src/shields_engine.cpp fireball-mini/windows/src/password_vault.cpp fireball-mini/windows/src/sync_engine.cpp fireball-mini/windows/src/beam_client.cpp fireball-mini/windows/src/webview2_host.cpp fireball-mini/windows/src/app_window.cpp fireball-mini/windows/tests/test_runner.cpp -o out/fireball_win_tests && ./out/fireball_win_tests"], repo_root),
        
        # 9. Fireball Mini Android Edition Unit & UI State Tests (Kotlin Gradle)
        ("Fireball Mini Android Unit & UI State Tests", ["./gradlew", "testDebugUnitTest"], repo_root / "fireball-mini"),
        
        # 10. Multi-Platform Packaging & Installer Generator
        ("Multi-Platform Distribution Packager", ["python3", "tools/package_ecosystem.py"], repo_root),
        ("Multi-Platform Installer Builders", ["python3", "tools/build_installers.py"], repo_root),
    ]

    print("================================================================")
    print("🔥 FIREBALL MASTER AUTOMATED TEST SUITE (UNIT + UI/UX + PACKAGING)")
    print("================================================================")

    results = []
    total_start = time.perf_counter()

    for title, cmd, cwd in stages:
        passed, duration, err = run_stage(title, cmd, cwd)
        results.append((title, passed, duration))
        if not passed:
            print(f"\n🚨 ABORTING: Stage '{title}' failed.")
            break

    total_duration = time.perf_counter() - total_start

    print("\n================================================================")
    print("📊 AUTOMATED TEST RESULTS SUMMARY REPORT")
    print("================================================================")
    all_passed = True
    for title, passed, duration in results:
        status_icon = "✅ PASS" if passed else "❌ FAIL"
        if not passed:
            all_passed = False
        print(f"{status_icon} | {duration:6.2f}s | {title}")

    print("================================================================")
    print(f"⏱️ Total Execution Time: {total_duration:.2f}s")
    if all_passed:
        print("🎉 100% OF ALL UNIT, INTEGRATION, AND UI/UX AUTOMATED TESTS PASSED!")
    else:
        print("⚠️ SOME TESTS FAILED. PLEASE REVIEW LOGS ABOVE.")
    print("================================================================")

    return 0 if all_passed else 1


if __name__ == "__main__":
    sys.exit(main())
