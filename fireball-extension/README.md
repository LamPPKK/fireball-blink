# 🔥 Fireball WebExtension Suite & Auxiliary Extensions

**Fireball WebExtension Suite** is a modular collection of companion extensions (**Manifest V3**) that bridge desktop browsers (**Google Chrome, Brave, Microsoft Edge, Mozilla Firefox, Opera, Vivaldi**) with the **Fireball Ecosystem** (Fireball Server, Fireball Mini, and Fireball Browser).

---

## 🧩 Danh Mục Các Tiện Ích & Module Phụ Trợ (Auxiliary Modules)

| Tiện ích / Module Phụ Trợ | Mã nguồn / Thành phần | Chức năng chính |
|---|---|---|
| **1. Fireball Sync** | `background/`, `popup/` | Đồng bộ hóa chuỗi 24 từ BIP-39 (Brave Sync v2), dịch chuyển tab tức thì sang điện thoại Android/iOS qua phím tắt `Ctrl+Shift+Y`, tạo mã QR chia sẻ tab. |
| **2. Fireball Shield** | `background/shields.js` | Tự động cắt tỉa tham số theo dõi URL (`utm_*`, `fbclid`, `gclid`), chặn quảng cáo, bảo vệ chống WebRTC IP leak và fingerprinting. |
| **3. Fireball Media Downloader** | `content_scripts/media_detector.js` | Tự động phát hiện và bắt link video trực tuyến **HLS (`.m3u8`)**, **DASH (`.mpd`)**, **MP4/WebM**, gửi sang Fireball Server / aria2 RPC để tải đa luồng tốc độ cao. |
| **4. Fireball Authenticator** | `lib/crypto.js` | Tự động điền tài khoản/mật khẩu mã hóa AES-256-GCM, bộ tạo mã xác thực 2 bước (TOTP Authenticator) mã hóa cục bộ an toàn. |
| **5. Fireball Reader** | `content_scripts/reader.js` | Trích xuất nội dung bài viết sạch không quảng cáo (Readability), tóm tắt ý chính bài viết, hỗ trợ đọc văn bản thành giọng nói (TTS) và dịch thuật. |
| **6. Fireball Remote Browser** | `lib/beam_ws.js` | Kết nối điều khiển và xem luồng duyệt web từ xa chạy trên Fireball Server thông qua giao thức nhị phân `FBEAM`. |
| **7. Fireball Retro Player** | `content_scripts/ruffle_interceptor.js` | Tự động phát hiện và kích hoạt giả lập Adobe Flash bằng WebAssembly (Rust/Ruffle) để chơi trực tiếp các tựa game Flash cổ điển (`.swf`). |



---

## 🚀 Hướng Dẫn Cài Đặt (Load Unpacked)

### Trên Google Chrome / Brave / Microsoft Edge / Opera:
1. Mở trình duyệt và truy cập `chrome://extensions` (hoặc `edge://extensions`, `brave://extensions`).
2. Bật công tắc **Developer mode** (Chế độ dành cho nhà phát triển).
3. Bấm nút **Load unpacked** (Tải tiện ích đã giải nén).
4. Chọn thư mục: `fireball-blink/fireball-extension`.
5. Biểu tượng **Fireball Companion** sẽ xuất hiện trên thanh công cụ!

### Trên Mozilla Firefox:
1. Mở Firefox và truy cập `about:debugging#/runtime/this-firefox`.
2. Bấm **Load Temporary Add-on...**
3. Chọn file `manifest.json` trong thư mục `fireball-extension/`.

---

## 🧪 Kiểm Thử Tự Động
```bash
node fireball-extension/test/extension_test.js
```
