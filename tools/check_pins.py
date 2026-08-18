#!/usr/bin/env python3
from __future__ import annotations

import json
import pathlib
import re

root = pathlib.Path(__file__).resolve().parents[1]
document = json.loads((root / "pins/upstream.json").read_text(encoding="utf-8"))
if document.get("schema_version") != 1:
    raise SystemExit("unsupported pin schema")
for name in ("chromium", "depot_tools"):
    pin = document.get(name, {})
    if not str(pin.get("url", "")).startswith("https://chromium.googlesource.com/"):
        raise SystemExit(f"{name}: untrusted upstream URL")
    if not re.fullmatch(r"[0-9a-f]{40}", str(pin.get("revision", ""))):
        raise SystemExit(f"{name}: revision must be an exact SHA-1")
