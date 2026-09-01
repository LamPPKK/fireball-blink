#!/usr/bin/env bash
set -euo pipefail

echo "🔥 Starting Fireball Server on macOS/Linux..."
PORT="${PORT:-9090}"
HOST="${HOST:-0.0.0.0}"

python3 beam_server.py --host "$HOST" --port "$PORT"
