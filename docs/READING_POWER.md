# Tác vụ nền trong phiên đọc

Trình đọc giữ cache bốn trang và cache chữ để tránh dựng lại nội dung khi lật.
Khi thiếu chỗ để cấp thêm cache trang, hệ thống có thể thu hồi các raster bìa có
thể dựng lại trước khi từ chối cấp phát. Trang đang đọc, lịch sử framebuffer
màn hình và các ô trang sẵn có không bị xóa vì lần thu hồi này.

WiFi rảnh theo hoạt động mạng, không theo chạm/lật sách. Một tác vụ mạng ngắn
kết thúc giữa hai lượt UI kiểm tra vẫn gia hạn timeout. Timeout bằng0 giữ ý
nghĩa không tự tắt. Khi đọc nội dung offline, chỉnh giờ tự động chờ đến khi
rời nội dung hoặc WiFi đã kết nối; yêu cầu mạng vẫn có đường đồng bộ giờ riêng.

Sau khi mất remote, BLE tự thử lại theo các khoảng4/8/16/32/60giây, sau đó giữ
trần60giây. Liên kết thành công hoặc yêu cầu nối lại thiết bị gần nhất đặt lại
khoảng chờ. Vì vậy remote quay lại vùng phủ sóng sau nhiều lỗi có thể cần chờ
lâu hơn; thao tác nối thủ công không phải chờ chu kỳ tối đa.

SOC/điện áp cập nhật mỗi15giây; trạng thái sạc vẫn mỗi2giây. SOC không hợp lệ
được thử lại ở lượt2giây kế tiếp. Kiểm tra CMD13 khi thẻ tốt cách nhau10giây;
chuỗi lỗi vẫn thử lại500ms và giữ quy trình dừng consumer trước phục hồi thẻ.
Kiểm tra hiện diện và lỗi I/O thực tế tiếp tục theo đường riêng. Lỗi bus khi
không có I/O và thẻ vẫn báo hiện diện có thể được phát hiện muộn hơn trước.

Font SD đang hoạt động ổn định được kiểm tra metadata tối đa mỗi30giây. Đổi
lựa chọn, lỗi font, phục hồi thẻ hoặc wake vẫn dùng đường xử lý ngay. File thay
đổi ngoài các đường thông báo có thể chờ đến kỳ kiểm tra tiếp theo.

Checkpoint định kỳ mỗi8lượt được ghi sau khi scan và prefetch trả quyền, thay
vì chặn trước khi hiện trang. Cửa sổ mất tiến độ khi tắt điện đột ngột kéo dài
thêm thời gian của lần hiển thị đó; Sleep/thoát sách vẫn lưu bắt buộc. Lỗi lưu
vẫn giữ dữ liệu chờ và thử lại theo chính sách cũ. Thống kê vẫn flush theo
chu kỳ30giây tích lũy để giữ giới hạn mất dữ liệu hiện có.

Task lấy mẫu cảm ứng thông báo trực tiếp cho UI. UI chờ thông báo hoặc timeout
8/40ms cũ, giúp nhận mẫu mới sớm mà không kéo dài deadline phím/BLE/gesture.
Đây là chờ có đánh thức theo sự kiện với timeout dự phòng; chưa phải cơ chế
ngủ vô hạn khi đứng yên và không có tuyên bố giảm số wakeup idle tương ứng.
Không thêm task hoặc cache framebuffer cho cơ chế này.

CPU vẫn dùng320MHz; PM/DFS/automatic light sleep không được bật thêm. Đường
hiển thị S31 chưa có bộ khóa PM đầy đủ cho scan/TPS/DMA trong cây này; bật DFS
cần kiểm tra clock ngoại vi, underrun và độ trễ thức dậy trên thiết bị trước.
Không dùng quy trình lock-Sleep để thay thế một khoảng chờ UI ngắn. Đồng hồ
hiển thị theo phút và anti-fade tiếp tục theo lựa chọn đã lưu, không tự thay
chu kỳ hoặc tắt nhằm đạt số tiết kiệm điện.

Host tests và build kiểm tra logic, giới hạn bộ nhớ và đường lỗi; chúng không
đo dòng pin, độ trễ quang học, peak SRAM hay xác nhận chất lượng trên thiết bị.
