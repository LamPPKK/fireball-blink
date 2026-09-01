# Fireball Mini for Android 🚀

**Fireball Mini** là phiên bản di động siêu nhẹ, tối ưu trải nghiệm và bảo mật của Fireball Browser dành cho hệ điều hành Android, được xây dựng dựa trên giao diện hiện đại **Jetpack Compose / Material 3** và tầng lõi C++/Rust (NDK Bridge).

---

## 🌟 Tính Năng Nổi Bật

### 1. Quản lý Tab Đa Tầng (Orbital Deck Model)
- **Spaces & Profiles:** Phân chia không gian làm việc (Personal, Work, v.v.) và hồ sơ lưu trữ cookie tách biệt.
- **3 Cấp độ lưu trữ Tab:**
  - **Favorites:** Tab ưa thích gắn với Profile, xuất hiện trên mọi Space.
  - **Pinned Tabs:** Tab ghim cố định trong từng Space.
  - **Today Tabs:** Tab lướt web hàng ngày, tự động dọn dẹp (Auto-Archive).
- **Burner Space:** Chế độ ẩn danh nghiêm ngặt (Off-the-record), tự hủy sạch dữ liệu và cache ngay khi đóng.
- **Tiết kiệm RAM thông minh (LRU Discard):** Đưa các tab ngầm không hoạt động vào trạng thái ngủ khi máy thiếu RAM.

### 2. Bộ Chặn Quảng Cáo & Quyền Riêng Tư (Fireball Shields)
- Chặn quảng cáo, theo dõi và mã độc tại tầng mạng qua FFI / C++ Bridge.
- **Cosmetic Filtering:** Tự động ẩn các khung trống quảng cáo (Ad placeholders) mà không làm vỡ giao diện website.
- **Strict URL Cleaner:** Tự động loại bỏ các tham số theo dõi chiến dịch (`utm_source`, `utm_campaign`, `fbclid`, `gclid`, v.v.).

### 3. Bắt Link Media & Tải Tệp Thông Minh (Media Sniffer)
- **Auto-Detection:** Tự động phát hiện các luồng phát video:
  - **HLS Adaptive Streams (`.m3u8`)**
  - **DASH Streams (`.mpd`)**
  - **Direct MP4 / WebM / MP3**
- Hiển thị thông báo tải về tức thì với 1 chạm.
- **Transfer Deck:** Hàng đợi tải đa luồng với khả năng Tạm dừng / Tiếp tục (Pause/Resume).

### 4. Định Tuyến & Bảo Mật Egress
- Hỗ trợ chuyển đổi nhanh giữa:
  - **Direct Internet:** Kết nối mạng chuẩn.
  - **Cloudflare WARP:** Mã hóa luồng dữ liệu di động.
  - **Tor Onion Circuit:** Ẩn danh nâng cao theo từng Profile.

---

## 🏗️ Cấu Trúc Dự Án

```
fireball-mini/
├── app/
│   ├── build.gradle.kts           # Cấu hình Gradle & NDK CMake
│   ├── src/
│   │   ├── main/
│   │   │   ├── cpp/               # C++ JNI Native Core Bridge
│   │   │   │   ├── CMakeLists.txt
│   │   │   │   ├── fireball_jni.cc
│   │   │   │   └── jni_helpers.h
│   │   │   ├── java/com/fireball/mini/
│   │   │   │   ├── MainActivity.kt
│   │   │   │   ├── FireballApp.kt
│   │   │   │   ├── core/          # Native Bridge & WebView Customization
│   │   │   │   ├── data/          # Repositories & State Handlers
│   │   │   │   └── ui/            # Jetpack Compose UI (Screens, Theme, Components)
│   │   │   └── res/               # Resources & Styles
│   │   └── test/                  # Unit Tests (Domain, URL Cleaner, Media Sniffer)
├── build.gradle.kts
└── settings.gradle.kts
```

---

## 🔨 Hướng Dẫn Biên Dịch & Cài Đặt

### Yêu cầu:
- Android SDK 35 (Android 15) & Min SDK 26 (Android 8.0+)
- Android NDK (Side by side 25+) & CMake 3.22.1+
- JDK 17+

### Lệnh biên dịch:
```bash
cd fireball-mini

# Chạy Unit Tests
./gradlew test

# Biên dịch APK Debug
./gradlew assembleDebug

# Cài đặt lên thiết bị / Emulator đã kết nối
./gradlew installDebug
```
File APK đầu ra nằm tại: `app/build/outputs/apk/debug/app-debug.apk`.
