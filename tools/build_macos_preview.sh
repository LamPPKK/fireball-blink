#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
output_root=${1:-"$repo_root/out/macos-preview"}
app="$output_root/Fireball Blink Preview.app"
executable="$app/Contents/MacOS/FireballBlinkPreview"

mkdir -p "$app/Contents/MacOS" "$app/Contents/Resources"
cp "$repo_root/preview/macos/Info.plist" "$app/Contents/Info.plist"
cp "$repo_root/Brand/FireballMeteorMark.png" \
  "$app/Contents/Resources/FireballMeteorMark.png"

DEVELOPER_DIR=${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer} \
  xcrun --sdk macosx clang++ \
  -std=c++20 \
  -fobjc-arc \
  -Wall \
  -Wextra \
  -Werror \
  -O2 \
  -I"$repo_root" \
  "$repo_root/preview/macos/main.mm" \
  "$repo_root/fireball/browser/domain_model.cc" \
  -framework Cocoa \
  -o "$executable"

printf '%s\n' "$app"
