# 🤖 Fireball Client for Android

The **Fireball Client for Android** is an ultra-lightweight (`< 2MB`) thin-streaming client dedicated to rendering live sessions streamed from [Fireball Server](../../fireball-server/).

---

## 🌟 Highlights
- **Footprint**: APK size `< 2MB`, RAM usage `< 20MB`.
- **Low-Latency Hardware Renderer**: Jetpack Compose `Canvas` / `SurfaceView` backed by hardware-accelerated JPEG/H.264 decoders.
- **Normalized Touch Forwarding**: Multi-touch, scroll gestures, and pinch-to-zoom normalized to 0.0-1.0 coordinates and sent to server via WebSockets.
- **QR Pairing**: Instant pairing by scanning the server's single-use pairing QR code.

---

## 🔨 Running Locally
```bash
# Connects to localhost:9090 on Android emulator
adb forward tcp:9090 tcp:9090
```
