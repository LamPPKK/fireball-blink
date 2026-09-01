# Fireball Lite for Windows (`fireball-win`)

Ultra-lightweight, high-performance desktop browser client for Windows powered by **Microsoft Edge WebView2 Evergreen Runtime** and **Modern C++20 / Win32**.

---

## 🌟 Key Architecture & Capabilities

| Feature | Description |
|---|---|
| **Engine** | Microsoft Edge WebView2 Evergreen (Pre-installed system-wide on Windows 10 & 11) |
| **Footprint** | Executable size `< 5MB`, RAM usage `< 60MB`, Instant cold boot (`< 100ms`) |
| **Spaces & Profile Isolation** | Custom `userDataFolder` per space (`Main`, `Work`, `Burner`) with zero cross-space data leakage |
| **Burner Mode** | Ephemeral temp session directory automatically destroyed upon closing |
| **Fireball Shields** | Zero-latency URL tracking parameter stripper (`utm_*`, `fbclid`, `gclid`, etc.) and cosmetic ad CSS injection |
| **Omnibox & Bangs** | Instant search shortcuts (`!g`, `!b`, `!yt`, `!gh`, `!w`, `!k`, `!sp`, `!e`, `!r`) |
| **Zero-Knowledge Vault** | Windows DPAPI & AES-256 encrypted local credential storage |
| **BIP-39 Sync & Beam** | 24-word sync phrase compatibility with Android/iOS + Remote browser streaming client |

---

## 📂 Project Structure

```
fireball-mini/windows/
├── CMakeLists.txt
├── README.md

├── include/
│   └── fireball/win/
│       ├── app_window.h
│       ├── beam_client.h
│       ├── domain_models.h
│       ├── password_vault.h
│       ├── search_engines.h
│       ├── shields_engine.h
│       ├── sync_engine.h
│       └── webview2_host.h
├── src/
│   ├── app_window.cpp
│   ├── beam_client.cpp
│   ├── domain_models.cpp
│   ├── main_win.cpp
│   ├── password_vault.cpp
│   ├── search_engines.cpp
│   ├── shields_engine.cpp
│   ├── sync_engine.cpp
│   └── webview2_host.cpp
└── tests/
    └── test_runner.cpp
```

---

## 🔨 Building and Running

### Windows (Visual Studio / MSVC / Ninja)
```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
.\build\Release\fireball_win.exe
```

### Running Unit Tests (Cross-Platform)
```bash
g++ -std=c++20 -Wall -Wextra -Werror -I./include src/*.cpp tests/test_runner.cpp -o /tmp/fireball_win_tests
/tmp/fireball_win_tests
```
