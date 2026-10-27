# Release Checklist for dpReader v1.0.1-vn

## Git Push

Bạn cần push commit và tag lên GitHub:

```bash
cd /Users/phanhuy/Docs/dpReader/Metalio-S31-kiikoread

# Push commit CI fix
git push origin main

# Push tag 1.0.1-vn
git push origin v1.0.1-vn
```

**LƯU Ý:** Nếu SSH key không hoạt động, bạn có thể:
1. Thêm SSH key: `ssh-add ~/.ssh/id_rsa`
2. Hoặc chuyển remote sang HTTPS với token:
   ```bash
   git remote set-url origin https://github.com/dephia31/dpReader31.git
   ```

## GitHub Release

Sau khi push thành công, tạo GitHub Release:

1. Vào https://github.com/dephia31/dpReader31/releases/new
2. Chọn tag: **v1.0.1-vn**
3. Release title: **dpReader v1.0.1-vn cho Metalio E-ink 4 Plus**
4. Description (copy từ CHANGELOG.md):

```
## dpReader 1.0.1-vn · 2026-10-27

Rà soát an toàn trước khi nạp: đối chiếu cây nguồn với repo gốc nhà sản xuất [CloudZao/Metalio-E-INK4-Plus](https://github.com/CloudZao/Metalio-E-INK4-Plus) (bản 2.0.66) và với Kconfig ESP-IDF v6.1.

### Sửa lỗi quan trọng

- **VCOM mặc định 3000 mV → 1200 mV, dải hiệu chuẩn 4000 → 2000 mV.** Giá trị cũ nằm ngoài dải nhà sản xuất và được áp dụng ngay lần khởi động đầu tiên, tức là máy chạy sai điện áp bias màn hình. Nay máy chưa hiệu chuẩn khởi động trên 1200 mV của nhà sản xuất. Vẫn chỉ ghi thanh ghi runtime, không ghi OTP vĩnh viễn.
- **Gỡ nhánh VCOM chết.** Hàm đọc VCOM luôn trả `true` nên trang hiệu chuẩn không bao giờ mở. Nay trang hiệu chuẩn nằm trong debug panel, mở bằng cách **giữ phím âm lượng lúc bật máy**.
- **`CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_300` là cấu hình vô hiệu.** ESP32-S31 chỉ có 40/240/320 MHz. Nay ghi rõ `MHZ_320` cho khớp thực tế và nhà sản xuất.
- **Main task stack 12288 → 16384 và Task WDT 5 s → 20 s.** Firmware nhà sản xuất đã nâng stack để tránh lỗi stack protection. Làm mới GC16 toàn màn hình chặn vài giây nên mốc 5 s báo nhầm timeout.
- **Sửa cảnh báo nạp firmware.** `full.bin` trải liền từ `0x0` nên ghi đè vùng NVS, làm mất cài đặt và VCOM đã hiệu chuẩn. Nay nêu rõ và chỉ ra lệnh nạp tại `0x10000` để giữ NVS.
- **Ghi log dung lượng PSRAM thực lúc khởi động.**
- **Cập nhật host test VCOM** theo dải 900..2000 mV và mặc định 1200 mV.

### Không thay đổi

Bảng phân vùng, waveform/LUT (đã đối chiếu md5 khớp nhà sản xuất), toàn bộ pinout EPD/SD/I2C, ánh xạ bit TCA9555, và đường nạp USB Serial/JTAG cùng phím cứng BOOT.

### Lưu ý

`full.bin` xoá NVS như mô tả ở trên. Sau khi nạp, nếu máy từng được hiệu chuẩn riêng thì cần hiệu chuẩn lại VCOM.

### Checksums

```
4349fe9fa300f8b32001ddd2ea1b463158b6c080866933e17dc57d40ecf844b4  dpReader-metalio47-1.0.1-vn-full.bin
3aa0961e5b1cdfd1d943fa54b685f2bb0a1301476ed2215c2c13635ea1188b50  dpReader-metalio47-1.0.1-vn.bin
```

🤖 Generated with [Claude Code](https://claude.com/claude-code)
```

5. Upload các file sau:
   - `dpReader-metalio47-1.0.1-vn.bin` (TF/OTA update)
   - `dpReader-metalio47-1.0.1-vn-full.bin` (USB full flash)
   - `SHA256SUMS.txt` (checksums)
   - `HUONG_DAN_FLASH.txt` (hướng dẫn)

6. Publish release

## Verification

Sau khi publish, kiểm tra:
- [ ] Tag v1.0.1-vn visible trên GitHub
- [ ] Release có đầy đủ 4 artifacts
- [ ] SHA256 checksums khớp
- [ ] Release notes đầy đủ và rõ ràng

## Files Ready

Tất cả file đã sẵn sàng trong directory:
`/Users/phanhuy/Docs/dpReader/Metalio-S31-kiikoread/releases/1.0.1-vn/`

```
dpReader-metalio47-1.0.1-vn.bin          (3.8M) - TF/OTA firmware
dpReader-metalio47-1.0.1-vn-full.bin     (3.9M) - USB full flash
SHA256SUMS.txt                                   - Checksums
HUONG_DAN_FLASH.txt                             - Flash instructions
```
