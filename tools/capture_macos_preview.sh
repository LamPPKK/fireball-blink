#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
app=$($repo_root/tools/build_macos_preview.sh)
executable="$app/Contents/MacOS/FireballBlinkPreview"
asset_root="$repo_root/docs/assets"

mkdir -p "$asset_root"

for layout in classic floating vertical grid; do
  "$executable" \
    --layout "$layout" \
    --capture "$asset_root/fireball-blink-macos-$layout.png"
done

"$executable" \
  --layout floating \
  --panel transfers \
  --capture "$asset_root/fireball-blink-macos-transfers.png"

printf '%s\n' "$asset_root"
