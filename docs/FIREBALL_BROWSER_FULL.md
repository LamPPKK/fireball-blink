# 🌐 Fireball Browser (Full Chromium/Blink Engine)

Full-featured, standalone web browser built on a hardened **Chromium / Blink GN overlay tree**.

Supported Platforms: **Android**, **Windows**, **macOS**, **Linux**.

---

## 🌟 Core Engine Architecture

1. **Chromium / Blink Hardened Overlay**:
   - Upstream telemetry removed at compile time with default-deny network policy.
   - Profile-owned policy bundle, primary-main-frame `NavigationThrottle`, sequence-safe subresource `URLLoaderThrottle`.
2. **Native Rust Adblock Engine (`adblock-rust`)**:
   - Panic-safe C ABI integration with compiled EasyList, EasyPrivacy, and cosmetic rule trees.
3. **Advanced Transfer Engine**:
   - Parallel HLS / DASH video stream sniffer and segmented downloader backed by foreground aria2 RPC.
4. **Orbital Command Deck UI**:
   - 4 tab presentation styles: Classic Chromium, Safari Floating, Vertical Sidebar, and Tab Grid.
5. **Multi-Platform Build Outputs**:
   - **Android**: Full Blink `apk`
   - **Windows**: `fireball.exe` (x64)
   - **macOS**: `Fireball.app` / `.dmg` (Universal / Apple Silicon & Intel)
   - **Linux**: `fireball` (`.deb` / `.rpm` / `.tar.gz`)
