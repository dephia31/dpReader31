# dpReader Metalio 4.7 — v1.2.4-vn

- Sửa reboot sau khoảng một phút Sleep; giữ ngoại vi ở trạng thái ngủ qua các lần thức định kỳ, khôi phục khi đánh thức.
- Tự lưu báo cáo khởi động/reset vào thư mục `crash_log` trên thẻ nhớ, gồm nguyên nhân reset và dấu vết Sleep; chờ thẻ sẵn sàng, thử lại khi ghi lỗi.
- Font hệ thống mặc định Bitter Bold. Cỡ 100% mới tương đương 160% trước đây; chuyển về 100% một lần khi nâng cấp sang cách tính mới, sau đó lưu lựa chọn người dùng.
- Đổi các mục chuyển giá trị lần lượt và thanh trượt cỡ chữ/bố cục đọc thành hộp chọn có đánh dấu giá trị hiện tại, phân trang và chạm ngoài để hủy.
- Cỡ chữ đọc gồm 40, 42, …, 90 px. Giãn dòng: −20%, −10%, 0%, 100%, 110%, 120%, 130%; giãn đoạn từ −10% đến 75%. Thêm khoảng cách giữa từ −3 đến 20 px, độc lập với giãn ký tự.
- Sửa lỗi 2 khi lưu highlight/ghi chú trên thẻ; tự tạo đủ thư mục còn thiếu, giữ snapshot cũ khi ghi lỗi và bổ sung log chẩn đoán.

Cập nhật bằng OTA, thẻ nhớ hoặc gói USB đi kèm. Giữ sách và dữ liệu người dùng. Lần đầu chạy phiên bản mới đặt Chỉ Sleep; người dùng có thể chọn lại chế độ trong Cài đặt. Cỡ chữ hệ thống chuyển sang cơ sở mới một lần, sau đó giữ các lựa chọn đã lưu.

Bản 1.2.3 vẫn có đường tải lưu trữ để quay lại.
