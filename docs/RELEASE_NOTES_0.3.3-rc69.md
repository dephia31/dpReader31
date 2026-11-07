# Ghi chú lịch sử Read Pico 0.3.3-rc69

Phát hành ngày 05/10/2026 cho MindReset Read Pico RDP-G01-W (ESP32-S3, flash 16 MB).

Bản này sửa lỗi màn hình khóa, tải kệ sách, khôi phục cấu hình và một số trang cài đặt. Nạp thông thường qua web giữ cài đặt và thẻ TF. Bản sao lưu có mật khẩu Wi-Fi ở dạng văn bản rõ.

## Ổn định

- Màn hình khóa kiểu vé kiểm tra nút nguồn định kỳ và tìm cách trở lại trang có thể thao tác nếu ngắt cảm ứng kéo dài hoặc liên tục không vào được chế độ ngủ. Màn hình chỉ đọc cache bìa và metadata hiện có để tránh phân tích EPUB lớn khi khóa.
- Quét thư mục nhiều cấp bằng duyệt tường minh để giảm stack. Lần liệt kê đầu ưu tiên metadata EPUB có sẵn.
- Trình quản lý tệp đưa đường dẫn lớn và bộ đệm sao chép vào bộ nhớ ngoài khi sao chép, xóa hoặc sửa thư mục. Trả lỗi khi cấp phát thất bại.

## Khôi phục cấu hình

- Kiểm tra bản sao lưu trước khi thay bản ghi đọc trên máy, tránh giữ tiến độ và danh sách đọc gần đây cũ sau khi khôi phục.
- Đọc lại thông tin Wi-Fi sau khi ghi để xác nhận. Kết nối lại Wi-Fi sau khi khôi phục thành công.

## Giao diện

- Tăng cỡ chữ và độ đậm đường viền trong cài đặt làm mới khi đọc.
- Cắt tên phông vượt chiều rộng thẻ để tránh chồng lên tùy chọn bên cạnh.

## Xác minh của đợt phát hành

- Build cấu hình sản phẩm với ESP-IDF v6.1 thành công. Kiểm thử máy tính cho sách, EPUB, sao lưu, thao tác tệp, Wi-Fi và giao diện đều qua.
- Đã xử lý các đường gọi dùng nhiều stack phát hiện được khi rà mã. Hiện tượng thỉnh thoảng khởi động lại ở kệ sách chưa tái hiện ổn định. Khóa lâu và khôi phục còn cần xác minh trên thiết bị.

Xem [ghi chú rc68](RELEASE_NOTES_0.3.3-rc68.md) và [changelog](CHANGELOG.md).
