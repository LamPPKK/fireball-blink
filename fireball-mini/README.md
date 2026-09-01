# ⚡ Fireball Mini Browser

**Fireball Mini** is the ultra-lightweight, battery-efficient, privacy-first browser edition designed for mobile, desktop, and thin-client devices.

Supported Platforms: **Android**, **Windows**, **iOS / iPadOS**, and **Java ME (J2ME)**.

---

## 🎯 The "Mini" Architecture & Philosophy

Unlike monolithic browsers that bundle a heavy 200MB Chromium engine, Fireball Mini uses **Native OS WebViews** or connects as a **Thin Streaming Client** to [Fireball Server](../fireball-server/):

| Edition | Target Devices | Tech Stack | Execution Mode | Footprint |
|---|---|---|---|---|
| **🤖 Android** | Phones, Tablets, Foldables | Kotlin, Jetpack Compose, Android WebView, NDK | Local WebView | `< 20MB APK`, `~50MB RAM` |
| **🪟 Windows** | Windows 10 & 11 PCs | Modern C++20, Win32, MS Edge WebView2 Evergreen | Local WebView2 | `< 5MB EXE`, `~60MB RAM` |
| **🍎 iOS / iPadOS** | iPhone, iPad, iPod Touch | Swift, SwiftUI, WebKit & `FBEAM` WebSocket | Local / Remote Stream | `< 15MB IPA`, `~30MB RAM` |
| **☕ Java ME** | Feature Phones, Symbian, BlackBerry | Java MIDP 2.0 / CLDC 1.1, LCDUI Canvas | Remote Server Stream | `< 60KB JAR`, `< 1.5MB RAM` |

---

## 📂 Editions & Directory Structure

```
fireball-mini/
├── app/ (android/)      # 1. Android Edition (Jetpack Compose + WebView)
├── windows/ (win/)      # 2. Windows Edition (C++20 / Win32 + WebView2)
├── ios/                 # 3. iOS & iPadOS Edition (SwiftUI + WebKit / Streaming)
└── j2me/                # 4. Java ME Edition (MIDP 2.0 Thin Streaming Client)
```

---

## 🌟 Common Capabilities Across Fireball Mini

1. **Multi-Space & Tab Tiers**: Isolate contexts (Personal, Work, Burner) without data leakage.
2. **Fireball Shields**: Strips tracking parameters (`utm_*`, `fbclid`, `gclid`), injects cosmetic CSS, and blocks ads.
3. **Zero-Knowledge Vault**: AES-256 / DPAPI local encrypted credential storage.
4. **BIP-39 Sync & Beam**: 24-word sync phrase compatibility across all devices and live browser streaming from Fireball Server.
5. **Search Bangs**: Instant search shortcuts (`!g`, `!yt`, `!gh`, `!w`, `!k`, `!sp`, `!e`, `!r`).
