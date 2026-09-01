# ⚡ Fireball Mini Browser (Lightweight Editions)

**Fireball Mini** is the ultra-lightweight, battery-efficient, privacy-first browser edition utilizing native operating system WebViews.

Supported Platforms: **Android** & **Windows**.

---

## 🎯 The "Mini" Philosophy

Unlike monolithic browsers that ship with a bundled 200MB Chromium binary, Fireball Mini uses the host OS's pre-installed Evergreen WebView:
- **Binary Size**: `< 5MB - 20MB`
- **RAM Footprint**: `< 50MB - 80MB`
- **Cold Boot Time**: `< 100ms`
- **Full Privacy Stack**: Fireball Shields, Tracking URL stripper, Cosmetic CSS adblock, Multi-Space session isolation, and Zero-Knowledge Vault.

---

## 📱 Platforms & Editions

### 1. 🤖 Android Edition (`fireball-mini/app/`)
- **Tech Stack**: Kotlin, Jetpack Compose, Material Design 3 (Material You), Android System WebView, Android NDK C++/Rust.
- **Features**: Tablet Tab Strip, Bottom Command Capsule, AI Assistant & Readability, Live Tab Thumbnail Previews, Media Stream Sniffer (HLS/DASH).
- **Build**:
  ```bash
  cd fireball-mini
  ./gradlew assembleDebug
  ```

### 2. 🪟 Windows Edition (`fireball-win/` or `fireball-mini/windows/`)
- **Tech Stack**: Modern C++20, Win32 API, Microsoft Edge WebView2 Evergreen runtime.
- **Features**: Custom User Data Folder per Space (`Main`, `Work`, `Burner`), Zero-Knowledge Password Vault, Search Bangs (`!g`, `!yt`, `!gh`, `!w`, `!k`), BIP-39 Sync, Beam Streaming Client.
- **Build**:
  ```powershell
  cd fireball-win
  cmake -B build
  cmake --build build --config Release
  ```
