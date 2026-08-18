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
        cases = {
            "network_audit_test": [
                "fireball/components/privacy/network_audit.cc",
                "tests/network_audit_test.cc",
            ],
            "browser_domain_test": [
                "fireball/browser/domain_model.cc",
                "tests/browser_domain_test.cc",
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
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
