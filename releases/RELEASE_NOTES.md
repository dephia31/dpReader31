## dpReader 1.2.0-vn — Metalio E-ink 4 Plus (ESP32-S31)

- Chữ TTF có hinting cho cả đen trắng và khử răng cưa; khử răng cưa dùng bốn mức xám, giữ lõi chữ đen và chỉnh độ đậm trên đường nét.
- Làm sạch chọn lọc tại chữ khi bật khử răng cưa, giữ nền trắng không đổi; giữ chu kỳ toàn làm mới người dùng đã chọn. VCOM chạy ở 2000 mV theo cấu hình đã thử, không ghi đè hiệu chuẩn đã lưu hoặc OTP.
- Sửa tràn stack trong bộ tự căn nét FreeType; tăng stack chính S31 lên 24 KiB và vẽ trước trang lên 16 KiB, giữ giới hạn bộ nhớ PSRAM.
- Sửa thanh độ sáng không bật lại đèn đang tắt và phím AI không nhận các lần nhấn giữ tiếp theo.
- Thêm vuốt cạnh trái/phải trang đọc để chỉnh độ sáng/đèn vàng, có hai công tắc riêng trong Điều khiển và cử chỉ.
- Tự đồng bộ giờ khi rảnh qua Wi-Fi đã lưu, lưu RTC và hiển thị giờ Việt Nam UTC+7.

Khử răng cưa vẫn có thể nhấp chữ và còn bóng tùy nội dung/nhiệt độ; bản này không cam kết loại bỏ hoàn toàn ghosting hoặc tăng tốc lật trang. Mức VCOM 2000 mV đã được người dùng thử và báo giảm nền xám; chưa có xác nhận hiệu chuẩn hoặc độ bền dài hạn cho mọi panel. Các kiểm tra build và mô phỏng đã qua; cần tiếp tục theo dõi Bluetooth/thoát sách và dung lượng stack trên thiết bị.

Gói OTA/TF là ứng dụng đầy đủ. USB ZIP kèm FLASH_USB.command (macOS) và FLASH_USB.bat (Windows), cập nhật ứng dụng và lựa chọn OTA, giữ dữ liệu; full.bin dành cho cài đầy đủ. Các file cùng một lần build, kèm SHA-256 và giấy phép FreeType; không kèm Bookerly.

Web flasher: https://dephia31.github.io/dpReader31/ — giữ bản chính thức trước 1.1.9-vn để tải/nạp lại.
