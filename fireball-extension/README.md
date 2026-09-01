# 🔥 Fireball WebExtension Suite & Standalone Extensions

**Fireball Extension** cung cấp đầy đủ cả **7 tiện ích mở rộng độc lập** (Standalone Extensions) và **Gói tổng hợp 7-trong-1** (Master Suite) chuẩn **Manifest V3** dành cho mọi trình duyệt (**Chrome, Brave, Edge, Firefox, Opera, Vivaldi**):

---

## 📂 Danh Mục 9 Tiện Ích Độc Lập (Standalone Extensions)

```
fireball-extension/
├── 1️⃣ fireball-sync/               # 1. Fireball Sync (Đồng bộ BIP-39 & Dịch chuyển Tab)
├── 2️⃣ fireball-shield/             # 2. Fireball Shield (Chống rò WebRTC & Cắt URL Tracking)
├── 3️⃣ fireball-media-downloader/   # 3. Fireball Media Downloader (Bắt link HLS/DASH/MP4)
├── 4️⃣ fireball-authenticator/      # 4. Fireball Authenticator (Ví mật khẩu AES & 2FA TOTP)
├── 5️⃣ fireball-reader/             # 5. Fireball Reader (Đọc báo sạch AI & Đọc to TTS)
├── 6️⃣ fireball-remote-browser/     # 6. Fireball Remote Browser (Stream FBEAM từ xa)
├── 7️⃣ fireball-retro-player/       # 7. Fireball Retro Player (Giả lập Ruffle Flash Game)
├── 8️⃣ fireball-tampermonkey/       # 8. Fireball Tampermonkey (Quản lý UserScript .user.js)
├── 9️⃣ fireball-ublock/             # 9. Fireball uBlock Origin (Chặn quảng cáo & tracker diện rộng)
└── 📦 suite/                       # Gói tổng hợp Master Suite
```

---

## 🧩 Chi Tiết & Chức Năng Từng Tiện Ích

| Tiện ích | Thư mục | Chức năng chính |
|---|---|---|
| **1. Fireball Sync** | `fireball-sync/` | Đồng bộ 24 từ BIP-39, phím tắt `Ctrl+Shift+Y` đẩy tab sang điện thoại tức thì. |
| **2. Fireball Shield** | `fireball-shield/` | Tự động cắt tỉa tham số theo dõi URL (`utm_*`, `fbclid`), chống WebRTC leak. |
| **3. Fireball Media Downloader** | `fireball-media-downloader/` | Bắt luồng video trực tuyến HLS (`.m3u8`), DASH (`.mpd`), MP4 gửi tải đa luồng. |
| **4. Fireball Authenticator** | `fireball-authenticator/` | Lưu mật khẩu mã hóa AES-256 và sinh mã xác thực 2 bước 2FA (RFC-6238 TOTP). |
| **5. Fireball Reader** | `fireball-reader/` | Trích xuất văn bản sạch không quảng cáo và đọc to thành tiếng (TTS). |
| **6. Fireball Remote Browser** | `fireball-remote-browser/` | Kết nối và điều khiển luồng duyệt web từ xa chạy trên Fireball Server (`FBEAM`). |
| **7. Fireball Retro Player** | `fireball-retro-player/` | Giả lập WebAssembly Ruffle để chơi game Flash cổ (`.swf`) trực tiếp trên web. |
| **8. Fireball Tampermonkey** | `fireball-tampermonkey/` | Trình quản lý UserScript (.user.js) chạy script tùy biến với hỗ trợ GM_* API. |
| **9. Fireball uBlock Origin** | `fireball-ublock/` | Công cụ chặn quảng cáo, tracker phân tích, mã đào coin và popup độc hại. |

---


## 🚀 Hướng Dẫn Cài Đặt (Load Unpacked)

### Cài đặt từng tiện ích riêng lẻ hoặc Gói tổng hợp:
1. Mở trình duyệt và truy cập `chrome://extensions` (hoặc `edge://extensions`, `brave://extensions`).
2. Bật công tắc **Developer mode** (Chế độ dành cho nhà phát triển).
3. Bấm nút **Load unpacked** (Tải tiện ích đã giải nén).
4. Chọn thư mục tiện ích bạn muốn cài (Ví dụ: `fireball-extension/fireball-sync` hoặc `fireball-extension/suite`).
5. Tiện ích sẽ ngay lập tức xuất hiện trên thanh công cụ của trình duyệt!

---

## 🧪 Kiểm Thử Tự Động
```bash
node fireball-extension/test/extension_test.js
```

