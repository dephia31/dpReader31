## dpReader 1.1.9-vn — Metalio E-ink 4 Plus (ESP32-S31)

- **Chữ TTF:** cải thiện mức xám, giữ khử răng cưa qua các lần lật trang. Khi tắt khử răng cưa, dùng TrueType hinting và cache 1 bit; tăng/giảm độ đậm trực tiếp trên đường nét trước khi vẽ, giữ độ rộng ký tự. Khắc phục bộ nhớ raster gây khởi động lại lúc mở sách trong các bản thử.
- **Home và Cài đặt:** kệ sách đen cho ba cuốn gần đây; Cài đặt có năm nhóm cha để dễ tìm mục cần chỉnh.
- **Truyền file:** nhãn gọn, sửa chữ tràn khung; hotspot `dpReader31` không mật khẩu; trang web local có sáu tab Sách, Thẻ nhớ, Phông chữ, Hình ảnh, Đồng bộ, Cá nhân.
- **Phím cứng & Cử chỉ:** mở từng phím để chọn riêng tác vụ nhấn ngắn và nhấn giữ; tránh tác vụ nhấn ngắn chạy thêm sau nhấn giữ.
- **Thẻ SD:** khôi phục đầy đủ trạng thái chân SD trước khi khởi tạo lại sau ngủ/reset và trong đường USB.

Khử răng cưa vẫn có thể còn bóng hình tùy chế độ làm mới. Bản này không thay waveform và chưa bổ sung hinting cho đường khử răng cưa. Bản sửa phục hồi SD đã qua kiểm tra mô phỏng; cần tiếp tục theo dõi sau flash/ngủ–thức trên thiết bị.

Gói ứng dụng dùng cho OTA/TF. Gói USB chứa ứng dụng, ảnh đầy đủ và các thành phần từ cùng một lần build; `FLASH_USB.command` (macOS) / `FLASH_USB.bat` (Windows) cập nhật ứng dụng với lựa chọn OTA tương ứng, giữ sách/cài đặt, không backup. `full.bin` dành cho cài đầy đủ. Kèm giấy phép FreeType; không kèm font Bookerly.

Web flasher: https://dephia31.github.io/dpReader31/ — giữ bản chính thức trước 1.1.8-vn để tải/nạp lại.
