## dpReader 1.1.8-vn — Metalio E-ink 4 Plus (ESP32-S31)

- **Tối ưu hóa bộ nhớ:** Giảm hơn 60 KiB dữ liệu tĩnh SRAM, chuyển bộ đệm log, metadata font, bảng hiển thị, trạng thái KoSync, danh sách Bluetooth, CPU buffer đọc sách và cấu trúc ZIP sang PSRAM; bảo toàn DMA/ISR và vòng quét phím trong SRAM nội bộ.
- **Đồng bộ KoSync:** Hiển thị trạng thái "Đang đồng bộ" tức thì trên màn hình trước khi bật Wi-Fi; quản lý vòng đời HTTP/TLS/buffer chặt chẽ, tự động giải phóng tài nguyên sau khi hoàn tất hoặc gặp sự cố mạng.
- **Xóa bóng mờ E-Paper:** Sửa lỗi màn hình Home và trang đen trắng bỏ qua GC16 định kỳ; tự động dọn toàn màn hình bằng waveform đầy đủ sau 6 khung thay đổi và tối thiểu 3 giây từ lần dọn trước; giữ nguyên làm mới cục bộ siêu tốc khi đọc sách.
- **Giao diện chữ lớn 100–200%:** Cải thiện hiển thị ở mọi tỷ lệ chữ hệ thống; tiêu đề chừa chỗ cho nút Back/nút phụ, danh sách cài đặt tách nhãn–giá trị từ 150%, giới hạn độ rộng chữ tránh chồng lấn và tràn viền icon.

Gói ứng dụng dùng cho OTA/TF; gói USB chứa cả ảnh đầy đủ và các tệp thành phần từ cùng một lần build. `FLASH_USB.command` (macOS) / `.bat` (Windows) cập nhật ứng dụng và chọn slot mới, giữ dữ liệu, không backup. Ảnh `full.bin` dành cho nạp đầy đủ qua web flasher/esptool.

Web flasher: https://dephia31.github.io/dpReader31/ — giữ lựa chọn tải/nạp bản 1.1.7-vn trước đó.
