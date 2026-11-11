# dpReader 1.4.2 · Full và Love Book

Cài qua USB tại [trang Flash chính thức](https://dephia31.github.io/dpReader31/). Chọn đúng Full hoặc Love Book, kết nối máy Metalio E-Ink4 Plus S31 ở chế độ BOOT và bấm **Bắt đầu nạp**.

- Máy dùng bố cục cũ sẽ cài sạch sang NVS 256 KiB. Sao lưu Wi-Fi, cài đặt và dữ liệu `/flash` cần giữ trước khi xác nhận. Thẻ SD giữ nguyên.
- Máy đã dùng NVS 256 KiB chỉ ghi ứng dụng và vùng chọn khởi động, giữ NVS và `/flash`.
- Nếu trình duyệt gặp lỗi kết nối, tải ZIP USB đúng phiên bản, giải nén và chạy `FLASH_USB.command` trên macOS hoặc `FLASH_USB.bat` trên Windows. Cần Python với esptool >= 5.6 và pyserial.

Bản này phát hành qua USB. Firmware 1.4.2 hướng dẫn đến trang Flash khi bấm Kiểm tra cập nhật. Firmware cũ có thể hiện thông báo gói cơ sở không tương thích.

Tính năng chính gồm sáu phím Bluetooth tùy chỉnh, font family giữ đậm/nghiêng trong EPUB, đảo màu với lật trang vi sai và gán Đảo màu cho phím cứng.
