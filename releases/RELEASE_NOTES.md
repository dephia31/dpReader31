## dpReader 1.3.5-vn / 1.3.5-love-vn · 2026-11-07

- Full và Love Book dùng chung hộp thông báo mở/đóng sách giữa màn hình, thu gọn còn 320 × 76 px.

- Khôi phục quyền bật Wi-Fi sau khi thoát màn khóa lúc khởi động hoặc hủy chuyển sang chế độ ngủ/nạp qua USB. Sửa lỗi truyền tệp cùng mạng hết thời gian chờ trước khi Wi-Fi được bật.
- Trình mở sách trên máy chỉ liệt kê thư mục và sách TXT/EPUB. Ẩn thư mục hồ sơ Wi-Fi, tệp cấu hình và log hệ thống.

- Full và Love Book lưu tối đa 6 mạng Wi-Fi trên thẻ nhớ. Khi cần Wi-Fi, máy ưu tiên mạng vừa lưu và tự thử mạng kế tiếp nếu kết nối thất bại.

- Lưu tiến độ dự phòng trên thẻ khi NVS đầy, kiểm tra checksum và giữ snapshot trước để phục hồi sau ghi gián đoạn. Hỗ trợ tiếp tục đọc, sách gần nhất, danh sách và xóa tiến độ. Bỏ trạng thái “Chưa lưu” khỏi footer.

- Ưu tiên lật trang liên tục. Ghi tiến độ theo mốc 8 trang và thống kê được dời tới khi ngừng đọc 1,2 giây, vẫn lưu khi thoát sách hoặc khóa máy. Đọc pin, ghi báo lỗi và khởi động mạng tự động nhường thao tác đọc.
- Lật trang khi thanh công cụ mở giữ cache chữ đã hoàn thành và dùng đường chuyển ảnh theo thay đổi thực tế. Chạm trang không có tham chiếu chú thích bỏ qua lượt duyệt lại chữ.
- Log đọc bổ sung thời điểm nhấc tay và lý do chọn lượt quét thường hoặc làm mới toàn trang.

- S31 chờ tác vụ mạng đang dùng Wi-Fi bàn giao trước khi mở truyền sách, cho thử lại từ màn báo lỗi và kiểm tra dung lượng thẻ ở tác vụ nền sau khi mạng sẵn sàng.
- Love Book dùng cập nhật theo vùng thay đổi khi mở hoặc đóng thanh công cụ trên trang chữ.

- Giữ điểm cuối của cú vuốt khi máy đang quét màn hình. Vuốt chân trang về Home thực hiện khi nhấc tay, cùng nhịp với thao tác mở công cụ đọc.

- Tắt Bluetooth ở tác vụ riêng để trình đọc tiếp tục nhận cảm ứng. Cài đặt báo trạng thái đang tắt, đã tắt hoặc chưa dừng được dựa trên kết quả thu hồi thực tế.

- Ưu tiên cử chỉ đang thực hiện trong trình đọc. Lệnh lật trang BLE đến cùng lúc được bỏ qua để tránh ngắt cử chỉ hoặc lật thêm trang sau khi nhấc tay.

- Thu hẹp vùng chạm chú thích EPUB để chạm vào chữ bên cạnh vẫn lật trang bình thường.

- Love Book: vuốt sang trái trên màn hình I Love Book, kể cả vuốt chéo, để mở Pop menu.


Cài Full hoặc Love Book qua [Web Flash dpReader](https://dephia31.github.io/dpReader31/).

Bản Love Book mở TXT/EPUB trực tiếp, giữ KoSync, OPDS, USB, Wi-Fi và BLE.

Đã build hai bản và kiểm thử trên máy tính. Các thay đổi chưa được nghiệm thu trên thiết bị thật.
