# 🔥 Fireball Server

Multi-platform headless browser streaming daemon, encrypted sync coordinator, and privacy egress hub.

Supported Platforms: **Windows**, **macOS**, **Linux**, **Docker**, **Android (Server To Go)**.

---

## 🌟 Key Capabilities

1. **Remote Browser Streaming (`FBEAM` Protocol)**:
   - Headless Chromium CDP session management.
   - Low-latency binary touch/mouse event dispatching (`< 10ms`).
   - Frame encoding and real-time streaming to Android, iOS, Windows, and Mac clients.
2. **Zero-Config Pairing**:
   - 6-word mnemonic passphrase confirmation.
   - Single-use QR token exchange with HMAC-SHA256 authenticated channels.
3. **Multi-Platform Deployment**:
   - Native Python runtime on Windows, macOS, and Linux.
   - 1-click Docker containerization (`docker-compose up -d`).

---

## 🚀 Quick Start

### 1. Docker (Recommended for Self-Hosting)
```bash
cd fireball-server
docker compose up -d
```

### 2. macOS / Linux Native
```bash
cd fireball-server
./start-server.sh
```

### 3. Windows Native
```cmd
cd fireball-server
start-server.bat
```

---

## 🧪 Testing
```bash
python3 fireball-server/test_beam_protocol.py
```
