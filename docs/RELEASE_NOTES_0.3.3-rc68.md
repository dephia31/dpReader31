# Ghi chú lịch sử Read Pico 0.3.3-rc68

Phát hành ngày 05/10/2026 cho MindReset Read Pico RDP-G01-W (ESP32-S3, flash 16 MB).

Bản này đưa các thay đổi cục bộ từ rc59 đến rc68 vào gói nạp qua web. Gói firmware không chứa sách, lịch sử đọc hay cấu hình cá nhân. Quy trình nạp qua web thông thường giữ NVS, phân vùng sách nội bộ và thẻ TF.

## Kệ sách và trình đọc

- Quét thư mục nhiều cấp dưới `books`. Sách mới dùng cỡ chữ và dàn trang chung. Thứ tự đọc gần đây được giữ sau khi khởi động lại.
- Sách yêu thích đứng đầu ở mọi kiểu kệ. Dấu yêu thích có màu đen và thêm viền trắng trên nền tối.
- Kiểu **Bìa và gáy sách** được ghi rõ là thử nghiệm, hiển thị 13 sách mỗi trang. Kiểu thông thường hiển thị 9 sách.
- Cải thiện căn đều, thụt đầu dòng và lề dọc. Thiết bị mới dùng giãn dòng trung bình. Kiểu và vị trí dòng dẫn đọc chỉnh trong menu con.
- Chế độ toàn màn hình giữ thanh tiến độ mảnh ở đáy. Cài đặt đọc cho phép làm mới thủ công, tự động sau 5, 10, 15 hoặc 30 trang, hoặc tắt làm mới bắt buộc.
- Làm rõ trạng thái dấu trang đã lưu. Sửa quay lại, lọc và chọn nhiều mục trong quản lý kệ.

## EPUB, ảnh và giao diện

- Cải thiện tương thích ZIP và mục lục EPUB. Giải phóng bảng tạm trước khi dựng lại cache mục lục hỏng.
- Trước khi mở sách, thu hồi cache bìa và các trang dựng trước có thể tái tạo. Khi Wi-Fi bật, thu hồi cả cache glyph để chừa bộ nhớ phân tích sách. Hiển thị lý do nếu mở sách thất bại.
- Ảnh nền khóa giữ tỷ lệ và cắt giữa để phủ màn hình. Xem trước dùng cùng vị trí cắt. Ảnh ngang và vuông được thu nhỏ theo giới hạn trình giải mã.
- Tăng độ đậm của đường viền, đường phân cách trong cài đặt, Home và kệ sách.

## Truyền tệp, cài đặt và sao lưu

- Điều chỉnh thứ tự khởi động dịch vụ, bộ đệm Wi-Fi và bộ đệm ghi để cải thiện truyền sách qua điểm phát hoặc Wi-Fi dùng chung. Điểm phát vẫn giới hạn một thiết bị.
- Căn vùng cắt avatar với mặt nạ bo góc. Điều chỉnh chữ ký và chỉ báo pin. Có thể tùy chỉnh tên, avatar và chữ ký.
- `Pico-settings.backup` trên thẻ TF lưu phông hệ thống và phông đọc, cỡ chữ, dàn trang, hiển thị, khóa, mạng, hồ sơ, tiến độ, thời lượng đọc, dấu trang, sách yêu thích và tên tùy chỉnh. Có thể khôi phục bản sao lưu cũ. Mật khẩu Wi-Fi ở dạng văn bản rõ. Gói không chứa ảnh, phông hoặc sách.
- Khi chuyển sang bố cục hai khe, lần nạp đầu qua web ghi bootloader, bảng phân vùng, ứng dụng và dữ liệu OTA ban đầu. Các lần sau có thể đặt ảnh ứng dụng tên `Pico-update.bin` ở gốc thẻ TF và cập nhật trong trình quản lý tệp. Lần khởi động đầu gặp lỗi có thể quay về khe trước.

## Xác minh của đợt phát hành

- Build cấu hình sản phẩm với ESP-IDF v6.1 và kiểm thử máy tính cho sách, sao lưu, truyền tệp và giao diện đều qua.
- Một EPUB do người dùng gửi đã được đọc trên máy tính với 22 chương và 225.309 byte văn bản. Mở sách đó khi bật Wi-Fi, cắt ảnh nền, dấu yêu thích trên bìa tối và độ ổn định truyền tệp còn cần thử trên thiết bị.

Xem [changelog](CHANGELOG.md) và [thông báo giấy phép bên thứ ba](../THIRD_PARTY_NOTICES.md).
