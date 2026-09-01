# 🔥 Fireball WebExtension Companion

**Fireball WebExtension Companion** is a cross-browser extension (**Manifest V3**) that seamlessly connects any desktop browser (**Google Chrome, Brave, Microsoft Edge, Mozilla Firefox, Opera, Vivaldi**) with **Fireball Mini Browser** on Android.

---

## 🌟 Key Features

1. **Brave Sync Chain (BIP-39 24 Words)**:
   - Zero-knowledge end-to-end encrypted synchronization without requiring account registration.
   - Sync Open Tabs, Bookmarks, and History between Desktop and Android.
2. **Space & Tab Teleport**:
   - One-click teleporting of current desktop tabs or entire workspace groups directly to Fireball Mini Android.
3. **Smart Media Sniffer**:
   - Automatically sniffs HLS master playlists (`.m3u8`), DASH streams (`.mpd`), MP4/WebM videos, and audio.
   - Provides instant download or stream URL copying.
4. **Strict URL Tracker Cleaner**:
   - Automatically strips intrusive tracking query parameters (`utm_*`, `fbclid`, `gclid`, `mc_eid`, etc.) upon copying or navigating.
5. **Netscape HTML Bookmarks & E2EE Backup**:
   - Export standard Netscape Bookmark HTML files compatible with Fireball Mini import.
   - Generate password-protected AES-256-GCM encrypted backup files (`.fireball`).
6. **Cyber Dark OLED Popup UI**:
   - Modern glassmorphic interface styled with Fireball’s signature Obsidian Black, Electric Lime, and Meteor Orange palette.

---

## 🚀 How to Install & Load Extension

### In Google Chrome / Brave / Microsoft Edge / Opera:
1. Open your browser and navigate to `chrome://extensions` (or `edge://extensions`, `brave://extensions`).
2. Enable **Developer mode** (toggle in the top right corner).
3. Click **Load unpacked** (Tải tiện ích đã giải nén).
4. Select the folder: `/path/to/fireball-blink/fireball-extension`.
5. The **Fireball Companion** icon will appear on your browser toolbar!

### In Mozilla Firefox:
1. Open Firefox and go to `about:debugging#/runtime/this-firefox`.
2. Click **Load Temporary Add-on...**
3. Select the `manifest.json` file inside `fireball-extension/`.

---

## 🧪 Testing

Run automated cryptographic, sync packet, and HTML parser unit tests:
```bash
node fireball-extension/test/extension_test.js
```
