## dpReader 1.1.7-vn — Metalio E-ink 4 Plus (ESP32-S31)

- Footer gọn, chữ status/footer40px; bỏ chế độ toàn màn hình và hàng tiêu đề trùng. Thanh tiến độ có phần đã đọc dày/đen, phần còn lại mảnh/nhạt, không biến mất khi lật nhanh; hai bên cân đối.
- Tận dụng khoảng trống cuối trang; sửa nhãn và hộp chọn chồng nhau khi tăng chữ hệ thống.
- Sửa phím âm lượng không nhận đủ chống dội; menu chọn shortcut rõ ràng, thêm về Home và bật/tắt đèn.
- KoSync trong trình đọc thu hồi cache/tác vụ đọc trước và tạm nhường BLE trước khi bật Wi-Fi; đồng bộ tự động chạy nền, không chặn mở sách/về Home hoặc gửi trùng tiến độ.
- Sửa reboot khi lưu log SD: chụp bộ đệm rồi nhả khóa trước khi ghi; kiểm tra đầy đủ lỗi ghi/đóng file.
- Khóa thủ công/tự khóa vào ngủ nhẹ, chọn1–24giờ trước khi tự tắt nguồn (mặc định1giờ), lưu cài đặt qua khởi động; PIN và không PIN chung bộ đếm.
- Sửa lỗi SDK gây Core1 Cache access error khi khởi động; giữ dữ liệu NVS/sách và cấu hình riêng S31.
- Giữ font TTF chọn lại thành công sau khởi động lỗi qua đổi trang và khóa/mở máy; giới hạn cache bảng font2MiB, thu hồi cache và thử lại một lần khi thiếu RAM.

Gói ứng dụng dùng cho OTA/TF; gói USB chứa cả ảnh đầy đủ và các tệp thành phần từ cùng một lần build. `FLASH_USB.command` (macOS) / `.bat` (Windows) cập nhật ứng dụng và chọn slot mới, giữ dữ liệu, không backup. Ảnh `full.bin` dành cho nạp đầy đủ qua web flasher/esptool.

Đã build chuẩn S31 và kiểm tra host các đường sửa; bản vá lỗi cache khởi động đã chạy trên thiết bị ở RC9. Thời gian ngủ dài và giữ font dưới tải thực tế chưa được đo đầy đủ trên bản chính thức. Việc rà soát toàn diện Internal SRAM là tác vụ tiếp theo, chưa đưa thay đổi cấu hình SRAM chưa kiểm chứng vào bản này.

Web flasher: https://dephia31.github.io/dpReader31/ — giữ lựa chọn tải/nạp bản1.1.6-vn trước đó.
