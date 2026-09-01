#!/usr/bin/env bash
set -euo pipefail

# Fireball macOS DMG Bundler
VERSION="1.0.0"
OUTPUT_DIR="dist/installers/macos"
DMG_NAME="Fireball-Browser-v${VERSION}-Universal.dmg"
STAGING_DIR="dist/dmg_staging"

echo "=========================================="
echo "🍏 Building Fireball macOS DMG Installer"
echo "=========================================="

mkdir -p "${OUTPUT_DIR}"
rm -rf "${STAGING_DIR}"
mkdir -p "${STAGING_DIR}"

# Create bundle structure
APP_BUNDLE="${STAGING_DIR}/Fireball.app"
mkdir -p "${APP_BUNDLE}/Contents/MacOS"
mkdir -p "${APP_BUNDLE}/Contents/Resources"

# Copy Icon
if [ -f "Brand/Fireball.icns" ]; then
  cp "Brand/Fireball.icns" "${APP_BUNDLE}/Contents/Resources/Fireball.icns"
fi

# Copy Info.plist
cat <<EOF > "${APP_BUNDLE}/Contents/Info.plist"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>Fireball</string>
    <key>CFBundleIconFile</key>
    <string>Fireball.icns</string>
    <key>CFBundleIdentifier</key>
    <string>com.fireball.browser</string>
    <key>CFBundleName</key>
    <string>Fireball</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>${VERSION}</string>
    <key>LSMinimumSystemVersion</key>
    <string>12.0</string>
</dict>
</plist>
EOF

# Launcher stub
cat <<'EOF' > "${APP_BUNDLE}/Contents/MacOS/Fireball"
#!/bin/bash
exec "${0%/*}/../../../fireball" "$@"
EOF
chmod +x "${APP_BUNDLE}/Contents/MacOS/Fireball"

# Create /Applications symlink in staging
ln -s /Applications "${STAGING_DIR}/Applications"

# Build DMG using hdiutil
hdiutil create -volname "Fireball Browser" \
  -srcfolder "${STAGING_DIR}" \
  -ov -format UDZO \
  "${OUTPUT_DIR}/${DMG_NAME}"

echo "✅ DMG generated successfully at ${OUTPUT_DIR}/${DMG_NAME}"
rm -rf "${STAGING_DIR}"
