# QUY TRÌNH COMMIT VÀ RELEASE FIRMWARE LÊN GITHUB (BAO GỒM WEB FLASH & OTA)
### Tài liệu chuẩn mực dành cho AI Agent và Nhà phát triển thực hiện tự động

Tài liệu này quy định chi tiết, từng bước quy trình từ khi hoàn thành thay đổi mã nguồn, tăng số hiệu phiên bản, biên dịch, đóng gói firmware, cập nhật Web Flasher (GitHub Pages), kênh OTA, commit git, tạo tag và phát hành GitHub Release cho dự án **dpReader Metalio 4.7 (ESP32-S31)** trên repository `dephia31/dpReader31`.

---

## 1. TỔNG QUAN KIẾN TRÚC PHÁT HÀNH (RELEASE ARCHITECTURE)

Dự án cung cấp 3 kênh tiếp cận firmware cho người dùng:
1. **Web Flasher (GitHub Pages - `https://dephia31.github.io/dpReader31/`)**: Cho phép người dùng cắm cáp USB Type-C vào máy tính và nạp firmware trực tiếp bằng trình duyệt (Chrome/Edge/Opera) qua Web Serial API. Sử dụng file `dpReader_metalio47_full.bin` (nạp từ offset `0x0`) hoặc `dpReader_metalio47_firmware.bin` (offset `0x10000`).
2. **Kênh OTA qua Wi-Fi**: Thiết bị máy đọc sách kết nối Wi-Fi, kiểm tra bản cập nhật mới từ file manifest `metalio-s31-update.json` và tải `dpReader_metalio47_firmware.bin` trực tiếp.
3. **Kênh cập nhật qua thẻ nhớ TF/MicroSD**: Người dùng tải `dpReader_metalio47_firmware.bin`, chép vào thẻ nhớ và thực hiện cập nhật trong menu Cài đặt của máy.

Tất cả các tệp nhị phân chính thức được lưu trữ tập trung tại:
- Thư mục `releases/` trên nhánh `main`.
- Tài nguyên đính kèm (Assets) tại mục [GitHub Releases](https://github.com/dephia31/dpReader31/releases).
- Trang tĩnh GitHub Pages.

---

## 2. QUY ƯỚC ĐẶT TÊN PHIÊN BẢN (VERSIONING CONVENTION)

- **Định dạng Tag Git**: Bắt đầu bằng chữ `v`, theo sau là SemVer và hậu tố `-vn` (Ví dụ: `v1.1.5-vn`, `v1.1.6-vn`).
- **Định dạng trong mã nguồn và file cấu hình**: Bỏ chữ `v`, chỉ giữ `X.Y.Z-vn` (Ví dụ: `1.1.5-vn`, `1.1.6-vn`).
- **Nguyên tắc tăng phiên bản**:
  - Tăng số PATCH (`v1.1.x-vn`): Sửa lỗi, tối ưu hiệu năng nhỏ, cải thiện giao diện, bổ sung cài đặt.
  - Tăng số MINOR (`v1.x.0-vn`): Bổ sung tính năng lớn (hệ thống truyền file mới, quản lý phông chữ mới, v.v.).

---

## 3. BƯỚC 1: ĐỒNG BỘ PHIÊN BẢN TRÊN TOÀN BỘ DỰ ÁN (VERSION BUMP CHECKLIST)

Khi chuẩn bị phát hành phiên bản mới (giả sử là `v1.1.6-vn` / `1.1.6-vn`), AI Agent **bắt buộc** phải cập nhật chuỗi phiên bản tại tất cả các tệp sau:

| STT | Đường dẫn tệp | Vị trí cần sửa | Giá trị mẫu |
|:---:|:---|:---|:---|
| 1 | `boards/metalio47/board.json` | Trường `"version"` | `"version": "1.1.6-vn"` |
| 2 | `main/ota_update.c` | Cấu trúc cấu hình OTA `.version` | `.version = "1.1.6-vn"` |
| 3 | `main/apps/app_device_settings.c` | Chuỗi fallback hiển thị phiên bản firmware | `app_d ? app_d->version : "1.1.6-vn"` |
| 4 | `web/index.html` | Thẻ hiển thị phiên bản mới nhất trên Web Flasher | `<span id="latest-version-name" ...>v1.1.6-vn</span>` |
| 5 | `docs/CHANGELOG.md` | Thêm tiêu đề mục phát hành mới ở đầu file | `## dpReader 1.1.6-vn · YYYY-MM-DD` |
| 6 | `README.md` | Phiên bản hiển thị ở câu mở đầu (nếu có) | `**dpReader** (\`v1.1.6-vn\`)` |
| 7 | `tools/publish_release.py` | Biến `tag_name`, tiêu đề `name`, và `body_text` | `'tag_name': 'v1.1.6-vn'` |

> [!IMPORTANT]
> **LƯU Ý CỰC KỲ QUAN TRỌNG VỀ MANIFEST OTA (`metalio-s31-update.json`):**
> Trường `"notes"` trong `metalio-s31-update.json` **KHÔNG ĐƯỢC VƯỢT QUÁ 384 BYTES (khuyến nghị dưới 200 ký tự tiếng Việt)**.
> Thiết bị ESP32 có giới hạn bộ đệm tĩnh khi parse JSON OTA. Nếu chuỗi `notes` quá dài sẽ gây lỗi parse JSON trên máy đọc sách khiến máy không cập nhật OTA được. Chi tiết dài hãy ghi vào `docs/CHANGELOG.md` và GitHub Release body.

---

## 4. BƯỚC 2: BIÊN DỊCH VÀ ĐÓNG GÓI FIRMWARE (BUILD & PACKAGING)

### 4.1. Lệnh biên dịch firmware
Kích hoạt ESP-IDF v6.1 (nếu cần) và chạy công cụ build chuẩn của dự án:
```bash
python3 tools/build_firmware.py --board metalio47 \
  --idf-metalio47 ~/esp/v6.1 \
  --python ~/.espressif/python_env/idf6.1_py3.12_env/bin/python
```
> [!NOTE]
> `--board` nhận `metalio47` (chuẩn), `all`, hoặc các alias `dpReader_metalio47` / `metalio4.7` / `metalio_s31`.
> Hai tham số `--idf-metalio47` và `--python` là **bắt buộc** ngoài shell ESP-IDF: nếu thiếu, driver sẽ
> tìm SDK trong `$IDF_PATH`/repo root rồi báo `requires an ESP-IDF SDK supporting esp32s31`, hoặc gọi
> `idf.py` bằng Python hệ thống và chết với `No module named 'rich_click'`.

Công cụ `build_firmware.py` sẽ thực hiện:
1. Chuẩn bị source view độc lập cho board `metalio47` (ESP32-S31).
2. Biên dịch firmware với cấu hình tối ưu.
3. Tạo 2 tệp nhị phân cốt lõi trong thư mục `build/boards/packages/metalio47/`:
   - **`dpReader_metalio47_firmware.bin`**: Tệp ứng dụng (nạp tại offset `0x10000`), dùng cho OTA và cập nhật thẻ nhớ TF.
   - **`dpReader_metalio47_full.bin`**: Tệp gộp toàn bộ từ offset `0x0` (gồm Bootloader S31 tại `0x2000`, Partition Table tại `0x8000`, Otadata tại `0xd000` và Ứng dụng tại `0x10000`).

### 4.2. Sao chép và tính toán mã băm SHA-256
Sau khi build xong:
```bash
# 1. Chép 2 file nhị phân vào thư mục releases/
cp build/boards/packages/metalio47/dpReader_metalio47_firmware.bin releases/
cp build/boards/packages/metalio47/dpReader_metalio47_full.bin releases/

# 2. Tạo bảng mã băm SHA256SUMS.txt
#    macOS không có `sha256sum` dù /sbin/sha256sum tồn tại — lệnh đó trả rỗng, tạo file checksum rỗng.
#    Dùng `shasum -a 256` (có sẵn trên macOS) hoặc `sha256sum` trên Linux/CI.
cd releases
shasum -a 256 dpReader_metalio47_firmware.bin dpReader_metalio47_full.bin > SHA256SUMS.txt
cd ..

# 3. Lấy kích thước byte và mã sha256 của file app:
APP_SIZE=$(stat -f%z releases/dpReader_metalio47_firmware.bin 2>/dev/null || stat -c%s releases/dpReader_metalio47_firmware.bin)
APP_SHA=$(shasum -a 256 releases/dpReader_metalio47_firmware.bin | awk '{print $1}')
echo "Size: $APP_SIZE"
echo "SHA256: $APP_SHA"
```

Kiểm tra lại ngay sau khi tạo (phải in `OK` cho cả hai file):
```bash
cd releases && shasum -a 256 -c SHA256SUMS.txt && cd ..
```

### 4.3. Cập nhật Manifest OTA
Cập nhật nội dung cho **CẢ HAI** file:
1. `metalio-s31-update.json` (tại thư mục gốc)
2. `releases/metalio-s31-update.json`

Cấu trúc chuẩn:
```json
{
  "schema": 1,
  "version": "1.1.6-vn",
  "project": "Kiikoread_Metalio_S31",
  "board": "Metalio-EINK4-Plus-S31",
  "layout": "metalio-s31-dual-5m-v1",
  "minimum_base_version": "1.0.0",
  "url": "https://dephia31.github.io/dpReader31/releases/dpReader_metalio47_firmware.bin",
  "sha256": "<MÃ_SHA256_VỪA_TÍNH>",
  "size": <KÍCH_THƯỚC_BYTE_VỪA_LẤY>,
  "notes": "dpReader v1.1.6-vn: <Tóm tắt thay đổi ngắn gọn dưới 200 ký tự>."
}
```

---

## 5. BƯỚC 3: CẬP NHẬT WEB FLASHER (GITHUB PAGES)

Trang Web Flasher được đặt trong thư mục `web/`:
- `web/index.html`: Giao diện chính của Web Flasher.
- `web/app.js`: Xử lý Web Serial nạp firmware bằng thư viện `esptool-js`.
- `web/style.css`: Giao diện phong cách hiện đại.

**Kiểm tra trước khi commit Web Flasher:**
1. Trong `web/index.html`:
   - Xác nhận thẻ `<span id="latest-version-name" ...>v1.1.6-vn</span>` đã mang đúng số hiệu phiên bản mới.
2. Cơ chế triển khai tự động (GitHub Pages):
   - GitHub Actions workflow `.github/workflows/pages.yml` đã được cấu hình tự động kích hoạt khi có thay đổi trong `web/**`, `releases/**`, hoặc `metalio-s31-update.json`.
   - Workflow này sẽ gom các file trong `web/`, `releases/`, và các file `.bin` vào thư mục `_site/` và tự động deploy lên GitHub Pages (`https://dephia31.github.io/dpReader31/`).
   - Do đó, **chỉ cần push các file này lên nhánh `main` là Web Flasher sẽ tự động có bản mới nhất**.

---

## 6. BƯỚC 4: GIT COMMIT, TẠO TAG VÀ PUSH LÊN GITHUB

### 6.1. Kiểm tra trạng thái Git
```bash
git status
```
Đảm bảo các file thay đổi bao gồm:
- Mã nguồn tính năng/sửa lỗi
- Cấu hình phiên bản (`board.json`, `app_device_settings.c`, `ota_update.c`, `web/index.html`)
- Tài liệu (`docs/CHANGELOG.md`, `README.md`)
- File nhị phân và manifest (`releases/dpReader_metalio47_firmware.bin`, `releases/dpReader_metalio47_full.bin`, `releases/SHA256SUMS.txt`, `metalio-s31-update.json`, `releases/metalio-s31-update.json`)
- Công cụ phát hành (`tools/publish_release.py`)

### 6.2. Tạo Commit
Đặt thông điệp commit rõ ràng theo quy chuẩn Conventional Commits:
```bash
git add .
git commit -m "feat(system): <tóm tắt tính năng chính> v1.1.6-vn"
# Hoặc:
# git commit -m "release: dpReader Metalio 4.7 Firmware v1.1.6-vn"
```

> [!NOTE]
> `releases/dpReader_metalio47_firmware.bin`, `releases/dpReader_metalio47_full.bin` và
> `releases/SHA256SUMS.txt` **đang được Git track** (không nằm trong `.gitignore`), nên `git add .`
> sẽ commit lại chúng ở mỗi bản phát hành (~8 MB nhị phân). Đó là hiện trạng của repo; chỉ cần lưu ý
> dung lượng lịch sử tăng dần. `releases/<version>/` chỉ lưu `SHA256SUMS.txt` +
> `metalio-s31-update.json` đã archive, không chứa `.bin`.

### 6.3. Tạo Git Tag
Tạo tag có chữ `v` tương ứng:
```bash
git tag -a v1.1.6-vn -m "dpReader Metalio 4.7 Firmware v1.1.6-vn"
```

### 6.4. Đẩy (Push) lên GitHub
Đẩy cả nhánh `main` và `tag` lên origin:
```bash
git push origin main
git push origin v1.1.6-vn
```

---

## 7. BƯỚC 5: PHÁT HÀNH GITHUB RELEASE

Có 2 phương thức để tạo GitHub Release chính thức:

### Cách 1: Sử dụng công cụ `tools/publish_release.py` (Khuyên dùng cho AI)
Dự án đã có sẵn script `tools/publish_release.py`. Script này sử dụng GitHub Token từ git credential helper cục bộ để:
1. Tạo Release trên GitHub với tag `v1.1.6-vn` và nội dung ghi chú tiếng Việt đầy đủ (`body_text`).
2. Tự động upload 4 tệp tài nguyên:
   - `releases/dpReader_metalio47_firmware.bin`
   - `releases/dpReader_metalio47_full.bin`
   - `releases/SHA256SUMS.txt`
   - `releases/metalio-s31-update.json`

Thực hiện:
```bash
python3 tools/publish_release.py
```
*Kết quả in ra: `Release v1.1.6-vn published successfully!`*

### Cách 2: Tự động qua GitHub Actions
Nếu không chạy `publish_release.py`, workflow `.github/workflows/release.yml` sẽ tự động kích hoạt khi nhận được push tag `v*`, tạo Release và đính kèm 4 file từ thư mục `releases/`.

---

## 8. BƯỚC 6: HẬU KIỂM & XÁC MINH SAU PHÁT HÀNH (VERIFICATION CHECKLIST)

Sau khi hoàn thành các bước trên, AI Agent thực hiện kiểm tra các tiêu chí sau:

- [ ] **GitHub Release**: Truy cập `https://github.com/dephia31/dpReader31/releases/tag/v1.1.6-vn`, kiểm tra Release đã tồn tại và có đầy đủ 4 file đính kèm (`dpReader_metalio47_firmware.bin`, `dpReader_metalio47_full.bin`, `SHA256SUMS.txt`, `metalio-s31-update.json`).
- [ ] **Web Flasher (GitHub Pages)**:
  - Kiểm tra tab Actions xem workflow `Publish web flasher` (`pages.yml`) đã hoàn thành (màu xanh).
  - Truy cập `https://dephia31.github.io/dpReader31/`:
    + Bản phát hành mới hiển thị đúng `v1.1.6-vn`.
    + Các nút nạp USB sẵn sàng hoạt động.
- [ ] **Manifest OTA**:
  - Truy cập `https://raw.githubusercontent.com/dephia31/dpReader31/main/releases/metalio-s31-update.json` và kiểm tra `version`, `sha256`, `size` đã khớp với file firmware mới.
  - Đảm bảo trường `notes` ngắn gọn, không làm crash bộ phân tích JSON trên máy.
- [ ] **Tính nhất quán của Git**:
  - `git status` sạch sẽ (`working tree clean`).
  - HEAD đang ở nhánh `main`, trùng commit với tag `v1.1.6-vn`.

---

## Quy trình kiểm tra hiện tại

GitHub Pages của repository hiện dùng nguồn legacy `gh-pages`. Workflow Pages sao chép tệp web và gói đã kiểm tra sang đúng nhánh đó, giữ lịch sử, rồi yêu cầu Pages build. Không deploy môi trường `github-pages` từ `main` hoặc thay chính sách bảo vệ môi trường.


- Chạy `tools/archive_current_release.py releases --source-commit <commit-bản-cũ>` trước khi thay gói chính thức; giữ nguyên bytes và checksum của phiên bản trước.
- `python3 tools/publish_release.py --dry-run` kiểm tra descriptor S31/version, hash/size app, ảnh full với bootloader0x2000, USB/TF ZIP và bản trước.
- Tag phải trùng HEAD và manifest. Workflow release dùng `tools/publish_release.py`: tạo draft, tải đủ app/full/USB ZIP/TF ZIP/manifest/verification/checksum, kiểm tra digest GitHub từng tệp rồi mới công khai. Lỗi tải không được báo thành công hoặc ghi đè tài sản đã công khai.
- Sau khi release và tải lại các tệp được kiểm tra, dọn RC cũ theo AGENTS.md; không dọn trước khi phát hành thành công.

## 9. TÓM TẮT DÀNH CHO AI AGENT (QUICK CHEATSHEET)

```
KHI ĐƯỢC YÊU CẦU COMMIT VÀ RELEASE BẢN MỚI:
1. Sửa code tính năng / bugfix.
2. Cập nhật version X.Y.Z-vn vào:
   - boards/metalio47/board.json
   - main/ota_update.c
   - main/apps/app_device_settings.c
   - web/index.html (thêm tiền tố v)
   - docs/CHANGELOG.md
   - tools/publish_release.py (tag_name, name, body_text)
3. Chạy build firmware:
   python3 tools/build_firmware.py --board dpReader_metalio47
4. Copy binary vào releases/ và tạo checksum:
   cp build/boards/packages/metalio47/dpReader_metalio47_firmware.bin releases/
   cp build/boards/packages/metalio47/dpReader_metalio47_full.bin releases/
   cd releases && sha256sum dpReader_metalio47_firmware.bin dpReader_metalio47_full.bin > SHA256SUMS.txt && cd ..
5. Cập nhật size & sha256 vào metalio-s31-update.json & releases/metalio-s31-update.json (notes < 384 bytes!).
6. Commit & Tag:
   git add .
   git commit -m "feat/release: <mô tả> vX.Y.Z-vn"
   git tag -a vX.Y.Z-vn -m "dpReader Metalio 4.7 Firmware vX.Y.Z-vn"
   git push origin main
   git push origin vX.Y.Z-vn
7. Chạy publish:
   python3 tools/publish_release.py
8. Hậu kiểm GitHub Release, Web Flasher và OTA JSON.
```
