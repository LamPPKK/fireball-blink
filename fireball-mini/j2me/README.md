# ☕ Fireball for Java ME (J2ME / MIDP 2.0)

Ultra-lightweight thin streaming client inspired by Opera Mini architecture for feature phones, Symbian, BlackBerry, and retro devices.

---

## 🌟 Architecture (Thin Streaming Client)

```
[ Java ME Client ]  <==== FBEAM / HTTP (JPEG Tiles) ====>  [ Fireball Server ]  <====>  [ World Wide Web ]
- Screen Canvas                                             - Headless Chromium
- Virtual Pointer Cursor                                    - Full JavaScript/CSS
- Keypad / D-Pad Input                                      - Zero-Knowledge Vault
```

- **Footprint**: `< 60KB .jar` binary, `< 1.5MB RAM`.
- **Target Specs**: MIDP 2.0 / CLDC 1.1 (Nokia S40/S60, Sony Ericsson, BlackBerry OS, Motorola).
- **Control Scheme**:
  - `D-Pad / Joystick`: Move virtual pointer cursor.
  - `5 / Fire`: Left-click / Tap at pointer.
  - `2 / 8`: Page Up / Page Down scroll.
  - `*`: New Tab.
  - `#`: Reload Page.

---

## 🔨 Packaging & Emulation

### Building with Java SDK / Ant
```bash
javac -source 1.3 -target 1.3 -d bin src/com/fireball/j2me/*.java
jar cfm fireball.jar MANIFEST.MF -C bin .
```

### Running on Modern Systems
Can be emulated via **FreeJ2ME**, **MicroEmulator**, or **KEmulator**.
