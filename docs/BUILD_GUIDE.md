# Fireball Blink Build & Optimization Guide (M1 Core Foundation)

Tài liệu này hướng dẫn chi tiết cách thiết lập môi trường biên dịch cho **Fireball Blink**, áp dụng các cờ tối ưu hóa hiệu năng lấy cảm hứng từ dự án **Thorium**, và áp dụng cấu hình gỡ bỏ theo dõi (un-Googling) theo tiêu chuẩn **Helium**.

---

## 1. Yêu cầu phần cứng & hệ thống

| Thành phần | Yêu cầu tối thiểu | Khuyến nghị sản xuất |
| :--- | :--- | :--- |
| **Hệ điều hành** | Ubuntu 24.04 LTS / macOS 14+ | Ubuntu 24.04 LTS (x86_64) / macOS 15 |
| **CPU** | 8 Cores (AVX2 / Apple Silicon) | 16+ Cores / AMD Ryzen 9 / Apple M-series |
| **RAM** | 32 GiB | 64 GiB (cần thiết khi link ThinLTO) |
| **Ổ cứng trống** | 150 GiB SSD/NVMe | 300+ GiB NVMe PCIe 4.0 |

---

## 2. Cài đặt công cụ nền tảng (`depot_tools`)

```bash
# 1. Clone depot_tools chính thức của Chromium
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git ~/depot_tools
export PATH="$HOME/depot_tools:$PATH"

# 2. Tạo thư mục chứa Chromium workspace
mkdir -p ~/chromium && cd ~/chromium

# 3. Đồng bộ mã nguồn Chromium theo mã hash đã ghim (pins/upstream.json)
fetch --nohooks --no-history chromium
cd src
git checkout tags/151.0.7922.169
gclient sync --revision "src@4b1c7520055f77780fe76d89bb89b76e4d19f64c" --jobs 16
gclient runhooks
```

---

## 3. Cấu hình GN Arguments (Thorium + Helium Presets)

Tạo thư mục build và áp dụng tệp [fireball-release.gn](file:///Users/lamndt/.gemini/antigravity-ide/scratch/fireball-blink/build-config/fireball-release.gn):

```bash
mkdir -p out/FireballRelease
cp <FIREBALL_ROOT>/build-config/fireball-release.gn out/FireballRelease/args.gn

# Tạo ninja build files
gn gen out/FireballRelease
```

### Điểm nhấn tối ưu hóa (Thorium):
- `is_official_build = true` & `use_thin_lto = true`: Tối ưu hóa toàn cục xuyên suốt các compilation units, cho phép inlining tối đa.
- `v8_enable_turbofan = true` & `v8_enable_maglev = true`: Bật trình biên dịch JIT đa tầng cao cấp của V8.
- `symbol_level = 0`: Loại bỏ debug symbols giúp giảm kích thước binary và tăng tốc độ nạp trang.

### Điểm nhấn bảo mật & gỡ bỏ theo dõi (Helium / Ungoogled):
- `enable_reporting = false` & `enable_crash_reporter = false`: Triệt tiêu 100% việc gửi log/crash về Google.
- `google_api_key = ""` & `use_official_google_api_keys = false`: Vô hiệu hóa Google Sync, Google Sign-in và Gaia authentication.
- `safe_browsing_mode = 0`: Chặn gửi URL của người dùng lên máy chủ Google SafeBrowsing.

---

## 4. Biên dịch & Kiểm thử khói (Smoke Test)

```bash
# Biên dịch trình duyệt hoàn chỉnh
autoninja -C out/FireballRelease chrome

# Kiểm tra phiên bản và chạy smoke test headless
out/FireballRelease/chrome --version
out/FireballRelease/chrome --headless=new --disable-gpu --dump-dom 'data:text/html,<h1>Fireball Blink</h1>'
```

---

## 5. Đo lường hiệu năng (Benchmarks)

Để xác nhận cải thiện tốc độ so với Chromium chuẩn:
1. **Speedometer 3.0:** Đánh giá độ phản hồi của các tác vụ Web Application (React, Vue, TodoMVC, SVG).
2. **JetStream 2.2:** Đánh giá hiệu năng JavaScript & WebAssembly.
3. **MotionMark 1.3:** Đánh giá tốc độ render đồ họa và CSS animations.
