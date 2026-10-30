## dpReader 1.2.1-vn — Metalio E-ink 4 Plus (ESP32-S31)

- **Cải thiện độ nét chữ khi không cần bật AA:** tự căn nét cho chữ đen trắng, giúp nét chữ rõ hơn khi tắt khử răng cưa.
- Cải thiện cách hiển thị chữ có AA và chữ giao diện với light hinting và độ phủ liên tục.
- Tối ưu lật trang khi bật AA bằng waveform37pha; giữ làm sạch toàn màn hình và phục hồi lỗi bằng GC48.
- Giữ bố cục, phông đã chọn, VCOM và các thiết lập đọc hiện có.

Gói OTA/TF là ứng dụng đầy đủ. Gói USB kèm FLASH_USB.command cho macOS và FLASH_USB.bat cho Windows để cập nhật ứng dụng và lựa chọn OTA, giữ dữ liệu. Các tệp cùng một lần build, kèm SHA-256 và giấy phép FreeType; không kèm font Bookerly.

Web flasher: https://dephia31.github.io/dpReader31/ — giữ bản chính thức trước 1.2.0-vn để tải/nạp lại.
