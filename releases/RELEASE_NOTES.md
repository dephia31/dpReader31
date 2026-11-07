## dpReader 1.3.7-vn / 1.3.7-love-vn · 2026-11-07

- **Khởi động an toàn & chẩn đoán lỗi**: Sửa lỗi crash khi khởi động (panic do bus I2C kẹt sau reset); khôi phục đường truyền I2C và thử lại khởi tạo TCA9555 khi boot.
- **Báo cáo sự cố chi tiết**: Lưu thêm địa chỉ hàm gọi (`ra`) và thông điệp lỗi gốc của ESP-IDF vào bộ nhớ RTC và xuất log đầy đủ ra thẻ nhớ.
- **Cập nhật tiến trình báo cáo lỗi**: Kiểm tra và lưu báo cáo ngay sau màn khóa và khung hình đầu tiên, đảm bảo ghi nhận đầy đủ sự kiện và thời gian hoạt động.
- **Cài đặt đọc → Status & Footer**: Gom các tùy chọn đầu và chân trang. Có thể tắt riêng thanh trạng thái hoặc chân trang để dành thêm chỗ cho nội dung sách. Khi đổi bố cục, máy giữ nguyên vị trí đọc.
- **Phông chữ**: Đưa ô **Tùy chỉnh** lên vị trí dễ tìm hơn. Bật lại chân trang sẽ dùng các tùy chọn đã lưu.
- **I Love Book**: Dùng nền đen họa tiết ô trám và bìa tạm có logo dpReader khi chưa có ảnh bìa.

Có hai bản **Full 1.3.7-vn** và **Love Book 1.3.7-love-vn**. Cài đặt hoặc cập nhật trực tiếp qua [Web Flash dpReader](https://dephia31.github.io/dpReader31/), cập nhật qua Wi-Fi trong Cài đặt hoặc dùng gói TF đúng với bản đang cài. Trang Web Flash vẫn giữ bản 1.3.5 để chọn lại.

