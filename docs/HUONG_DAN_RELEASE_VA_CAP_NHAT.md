# HƯỚNG DẪN QUY TRÌNH PHÁT HÀNH (RELEASE) & CÁC KÊNH CẬP NHẬT FIRMWARE

Tài liệu này cung cấp toàn bộ hướng dẫn về:
1. **Quy trình đóng gói & phát hành (Release) firmware lên GitHub** dành cho quản trị viên/lập trình viên.
2. **Các kênh cập nhật (Update Channels)** dành cho người dùng thiết bị máy đọc sách **dpReader Metalio 4.7 (ESP32-S31)**.

---

## MỤC LỤC
1. [Tổng Quan Kiến Trúc Kênh Cập Nhật](#1-tổng-quan-kiến-trúc-kênh-cập-nhật)
2. [Quy Trình Phát Hành Release Lên GitHub (Dành Cho Dev)](#2-quy-trình-phát-hành-release-lên-github-dành-cho-dev)
   - [Cách 1: Tự động hóa bằng lệnh CLI duy nhất (Khuyên dùng)](#cách-1-tự-động-hóa-bằng-lệnh-cli-duy-nhất-khuyên-dùng)
   - [Cách 2: Phát hành tự động qua GitHub Actions](#cách-2-phát-hành-tự-động-qua-github-actions)
3. [Các Kênh Cập Nhật Cho Người Dùng Thiết Bị](#3-các-kênh-cập-nhật-cho-người-dùng-thiết-bị)
   - [Kênh 1: Cập nhật Trực tiếp OTA qua Wi-Fi (Thuận tiện nhất)](#kênh-1-cập-nhật-trực-tiếp-ota-qua-wi-fi-thuận-tiện-nhất)
   - [Kênh 2: Cập nhật Thủ công bằng Thẻ Nhớ TF / MicroSD](#kênh-2-cập-nhật-thủ-công-bằng-thẻ-nhớ-tf--microsd)
   - [Kênh 3: Nạp Firmware Toàn Diện qua USB Type-C (Khôi phục / Cài mới)](#kênh-3-nạp-firmware-toàn-diện-qua-usb-type-c-khôi-phục--cài-mới)
4. [Cấu Trúc Tệp Tin & Mã Kiểm Tra An Toàn](#4-cấu-trúc-tệp-tin--mã-kiểm-tra-an-toàn)

---

## 1. TỔNG QUAN KIẾN TRÚC KÊNH CẬP NHẬT

Thiết bị **dpReader Metalio 4.7** sử dụng cơ chế cập nhật an toàn kép (**Dual-slot OTA partition**):
- Khi cập nhật, hệ thống ghi bản mới vào phân vùng ứng dụng nhàn rỗi (`app0` hoặc `app1`).
- Quá trình tải luôn kiểm tra đầy đủ mã băm SHA-256 từng block và kích thước tệp.
- Chỉ khi nạp thành công 100% và qua kiểm tra chữ ký/tính toàn vẹn, phân vùng khởi động mới được chuyển đổi. Nếu mất điện hoặc đứt mạng giữa chừng, thiết bị vẫn khởi động an toàn từ phiên bản cũ.

### Các địa chỉ kênh cấp phát (Feed URLs):
- **Kênh OTA Chính (GitHub Raw)**:  
  `https://raw.githubusercontent.com/dephia31/dpReader31/main/releases/metalio-s31-update.json`
- **Kênh OTA Dự Phòng (GitHub Pages)**:  
  `https://dephia31.github.io/dpReader31/metalio-s31-update.json`
- **Kênh Tải Firmware Trực Tiếp**:  
  `https://raw.githubusercontent.com/dephia31/dpReader31/main/releases/dpReader_metalio47_firmware.bin`

---

## 2. QUY TRÌNH PHÁT HÀNH RELEASE LÊN GITHUB (DÀNH CHO DEV)

### Cách 1: Tự động hóa bằng lệnh CLI duy nhất (Khuyên dùng)
Dự án đã tích hợp sẵn công cụ tự động hóa `tools/publish_release.py`. Công cụ này tự động:
- Kiểm tra bản build firmware.
- Đóng gói file OTA `dpReader_metalio47_firmware.bin` và file USB toàn diện `dpReader_metalio47_full.bin`.
- Tính mã băm SHA-256 và cập nhật `releases/SHA256SUMS.txt`.
- Cập nhật manifest kênh OTA `releases/metalio-s31-update.json` và `metalio-s31-update.json`.
- Lưu trữ lịch sử phiên bản vào thư mục `releases/<version>/`.
- Thực hiện commit git, tạo Git Tag `v<version>`, và tự động đẩy lên GitHub.

#### Các bước thực hiện:
1. Mở Terminal tại thư mục gốc của repository.
2. (Tùy chọn) Kích hoạt môi trường ESP-IDF v6.1 nếu muốn biên dịch mới:
   ```bash
   . $HOME/esp/v6.1/export.sh
   ```
3. Chạy lệnh phát hành:
   ```bash
   python3 tools/publish_release.py --version 1.0.1-vn --notes "dpReader v1.0.1-vn: Hỗ trợ tiếng Việt Bitter font toàn hệ thống, tùy biến phím tắt và cử chỉ đọc sách, cải thiện ghosting E-ink, kênh OTA GitHub trực tiếp." --push
   ```
   *(Nếu đã có bản build sẵn trong `releases/`, script sẽ tự dùng file đó; nếu muốn tự động biên dịch lại từ đầu, thêm cờ `--build`)*.

4. **Kết quả**:
   - Nhánh `main` và thẻ tag `v1.0.1-vn` được đẩy lên GitHub.
   - GitHub Actions kích hoạt `.github/workflows/release.yml` tạo **GitHub Release** chính thức đính kèm đầy đủ tệp tin.
   - GitHub Actions kích hoạt `.github/workflows/pages.yml` triển khai kênh OTA dự phòng lên **GitHub Pages**.

---

### Cách 2: Phát hành tự động qua GitHub Actions (Trên giao diện Web)
Nếu bạn không muốn chạy lệnh từ máy tính:
1. Vào tab **Actions** trên GitHub: `https://github.com/dephia31/dpReader31/actions`.
2. Chọn workflow **Publish GitHub Release** ở menu bên trái.
3. Nhấp vào nút **Run workflow**:
   - **Tên Tag**: Điền phiên bản (ví dụ: `v1.0.1-vn`).
   - **Tiêu đề Release**: Điền tiêu đề phát hành.
   - **Ghi chú phát hành**: Điền nội dung các tính năng mới / sửa lỗi.
4. Bấm **Run workflow**. Hệ thống sẽ tự động tạo Release đính kèm đầy đủ firmware và tài nguyên!

---

## 3. CÁC KÊNH CẬP NHẬT CHO NGƯỜI DÙNG THIẾT BỊ

### Kênh 1: Cập nhật Trực tiếp OTA qua Wi-Fi (Thuận tiện nhất)
Áp dụng cho mọi máy đang kết nối Wi-Fi.

1. Bật máy đọc sách và kết nối vào mạng **Wi-Fi** (hoặc phát Wi-Fi từ điện thoại).
2. Vào màn hình **Cài đặt** (Settings) -> Cuộn xuống mục **Cập nhật hệ thống**.
3. Chọn **Kiểm tra cập nhật**:
   - Thiết bị sẽ tự động kết nối đến GitHub để kiểm tra phiên bản mới.
   - Nếu có bản cập nhật, màn hình sẽ hiển thị: số phiên bản mới, dung lượng tải về, và chi tiết các cải tiến.
4. Chọn **Cập nhật ngay**:
   - Máy sẽ tải firmware trực tiếp qua Wi-Fi với thanh tiến trình rõ ràng.
   - Sau khi tải và kiểm tra mã SHA-256 hoàn tất, thiết bị sẽ tự động chuyển slot khởi động và khởi động lại vào hệ điều hành mới.
   - **Lưu ý**: Giữ mức pin trên 20% khi cập nhật.

---

### Kênh 2: Cập nhật Thủ công bằng Thẻ Nhớ TF / MicroSD
Dành cho trường hợp máy không có Wi-Fi hoặc mạng chập chờn.

1. Tải tệp firmware **`dpReader_metalio47_firmware.bin`** từ mục [Releases](https://github.com/dephia31/dpReader31/releases) hoặc thư mục `releases/` của dự án.
2. Cắm thẻ nhớ MicroSD (định dạng FAT32) vào máy tính.
3. Chép tệp `dpReader_metalio47_firmware.bin` vào **thư mục gốc** của thẻ nhớ (ví dụ: `E:\dpReader_metalio47_firmware.bin`).
4. Cắm thẻ nhớ vào máy đọc sách.
5. Trên máy đọc sách: Vào **Cài đặt** -> **Cập nhật hệ thống** -> Chọn **Cập nhật từ thẻ nhớ**.
6. Máy sẽ quét tệp, xác thực phiên bản và tự động nâng cấp nhanh chóng.

---

### Kênh 3: Nạp Firmware Toàn Diện qua USB Type-C (Khôi phục / Cài mới)
Dành cho máy mới xuất xưởng, máy cần phân vùng lại, hoặc máy bị lỗi phần mềm không thể khởi động.

#### Cách A: Dùng Trình duyệt Web (Chrome / Edge / Opera)
1. Mở trang Web Flasher tại: `https://dephia31.github.io/dpReader31/`
2. Cắm cáp USB Type-C nối máy đọc sách với máy tính.
3. Nhấn giữ phím Nguồn + Phím AI (hoặc vào chế độ BOOT) để máy tính nhận cổng nạp.
4. Chọn cổng COM/Serial và bấm **Cài đặt Firmware**.

#### Cách B: Nạp bằng công cụ dòng lệnh `esptool`
1. Tải tệp firmware toàn diện **`dpReader_metalio47_full.bin`** (đã bao gồm Bootloader, Partition Table, Otadata, App).
2. Chạy lệnh nạp từ offset `0x0`:
   ```bash
   python3 -m esptool --chip esp32s31 -p /dev/cu.usbmodem* -b 460800 write_flash 0x0 dpReader_metalio47_full.bin
   ```
   *(Hoặc nếu chỉ nâng cấp phần mềm giữ nguyên dữ liệu, nạp vào offset `0x10000` kèm xoá `otadata` để bootloader chọn đúng slot vừa ghi)*:
   ```bash
   python3 -c "open('ota_clear.bin','wb').write(b'\xff'*8192)"
   python3 -m esptool --chip esp32s31 -p /dev/cu.usbmodem* -b 460800 write_flash 0xd000 ota_clear.bin 0x10000 dpReader_metalio47_firmware.bin
   ```
   > Nếu máy từng cập nhật qua Wi-Fi, `otadata` có thể đang trỏ `ota_1`; thiếu bước xoá `otadata` thì máy khởi động lại vào bản cũ dù `ota_0` đã ghi xong.

---

## 4. CẤU TRÚC TỆP TIN & MÃ KIỂM TRA AN TOÀN

| Tệp tin | Kích thước | Mục đích sử dụng |
|:---|:---:|:---|
| **`dpReader_metalio47_firmware.bin`** | ~3.9 MB | Bản cập nhật App dành cho OTA Wi-Fi và cập nhật Thẻ nhớ TF. |
| **`dpReader_metalio47_full.bin`** | ~4.0 MB | Bản toàn diện nạp từ offset `0x0` qua USB Type-C (cứu máy / cài mới). |
| **`metalio-s31-update.json`** | ~600 B | Manifest thông tin phiên bản phục vụ kênh kiểm tra cập nhật OTA tự động. |
| **`SHA256SUMS.txt`** | ~180 B | Bảng mã băm SHA-256 để đối chiếu tính toàn vẹn của tệp tin. |

### Ví dụ nội dung Manifest OTA (`metalio-s31-update.json`):
```json
{
  "schema": 1,
  "version": "1.0.1-vn",
  "project": "Kiikoread_Metalio_S31",
  "board": "Metalio-EINK4-Plus-S31",
  "layout": "metalio-s31-dual-5m-v1",
  "minimum_base_version": "1.0.0",
  "url": "https://raw.githubusercontent.com/dephia31/dpReader31/main/releases/dpReader_metalio47_firmware.bin",
  "sha256": "ff0b7e907dea7bb1b2568b4eba8530abf310485aa3d3c3702ac4d4b3f0235694",
  "size": 3913872,
  "notes": "dpReader Metalio 4.7 v1.0.1-vn: Hỗ trợ tiếng Việt Bitter font toàn hệ thống, tùy biến phím tắt và cử chỉ đọc sách, cải thiện ghosting E-ink, kênh OTA GitHub trực tiếp."
}
```
Mỗi khi phát hành bản mới, chỉ cần chạy công cụ `tools/publish_release.py` là thiết bị của tất cả người dùng sẽ nhận được thông báo cập nhật tự động!
