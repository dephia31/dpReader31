# Phông chữ trực tuyến trên dpReader S31

Mục **Phông chữ trực tuyến** ở góc trên bên phải của trang chọn phông hệ thống và phông đọc sách dùng chung danh sách năm mục. Mỗi dòng hiển thị tên, kích thước tệp và nút tải. Kích thước là số byte trên thẻ nhớ.

Trình tải dùng Wi-Fi đã lưu. Thiết bị cần ngày giờ chính xác để xác thực HTTPS. Nếu chưa có mạng, hãy cấu hình Wi-Fi. Nếu giờ sai, hãy đồng bộ giờ qua Wi-Fi. Tác vụ giải phóng tài nguyên của kết nối tạm khi xong và giữ kết nối được dùng chung. Trong khi tải, tác vụ tạm mượn tài nguyên BLE và khôi phục cấu hình của người dùng sau đó.

Phông được ghi theo khối vào `/sdcard/assets/fonts`. Phiên bản cố định, kích thước và SHA-256 nằm trong `main/font/online_fonts.json`. Dùng tệp do tác giả phát hành và chỉ chấp nhận chuyển hướng HTTPS đến các tên miền GitHub đã chỉ định. Có thể thử lại khi nguồn tạm thời không truy cập được.

| Phông chữ | Nguồn cố định |
| --- | --- |
| Literata | Google Fonts |
| Lora | Google Fonts |
| Noto Serif | Google Fonts |
| Merriweather | Google Fonts |
| Source Serif 4 | Google Fonts |

Cả năm phông có ký tự tiếng Việt. Công cụ kiểm tra trực tiếp tệp TTF gốc tại commit cố định. Phông cũ trên thẻ tiếp tục sử dụng được.

Mỗi phông có tệp `.OFL.txt` đi kèm chứa thông báo bản quyền và toàn văn OFL 1.1. Danh sách không có nút giấy phép riêng. Khả năng tải qua dịch vụ Moon hoặc WeRead chưa xác lập quyền phân phối lại.

Phông đang chọn giữ nguyên dù tải thành công, lỗi hoặc hủy. Tải xong, quay lại danh sách để chọn phông. Khi chọn phông hệ thống, thiết bị vẫn hỏi có dùng cùng phông cho trình đọc hay không. Khi rời trang, khóa máy hoặc mất thẻ, tác vụ được hủy và trả lại handle trước khi tiếp tục. Tệp tạm của trình tải được dọn hoặc ghi lại vào lần tải sau. Tên theo phiên bản giúp bảo toàn phông của người dùng.

Tài liệu này mô tả tính năng phông chữ trên S31. Cập nhật firmware qua OTA theo [quy trình phát hành dpReader](QUY_TRINH_COMMIT_VA_RELEASE.md).
