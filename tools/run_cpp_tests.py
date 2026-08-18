#!/usr/bin/env python3
"""Compile the standalone overlay policy test without a Chromium checkout."""

from __future__ import annotations

import os
import pathlib
import shutil
import subprocess
import sys
import tempfile


def main() -> int:
    root = pathlib.Path(__file__).resolve().parents[1]
    compiler = os.environ.get("CXX") or shutil.which("c++")
    if compiler is None:
        print("fireball-cpp-tests: no C++ compiler found", file=sys.stderr)
        return 1
    with tempfile.TemporaryDirectory(prefix="fireball-blink-tests-") as temporary:
        binary = pathlib.Path(temporary) / "network_audit_test"
        subprocess.run(
            [
                compiler,
                "-std=c++20",
                "-Wall",
                "-Wextra",
                "-Werror",
                f"-I{root}",
                str(root / "fireball/components/privacy/network_audit.cc"),
                str(root / "tests/network_audit_test.cc"),
                "-o",
                str(binary),
            ],
            check=True,
        )
        subprocess.run([str(binary)], check=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
