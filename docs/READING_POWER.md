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

Cảm ứng, sự kiện phím nguồn/AI/âm lượng, dữ liệu BLE đã vào hàng đợi và hoàn
thành worker chuẩn bị trang cùng báo một chốt đánh thức UI. Chốt dùng semaphore
nhị phân cấp phát tĩnh và bit lý do; dữ liệu phím vẫn nằm trong hàng đợi gốc,
không bị gộp cùng thông báo. Không dùng chung task notification với sampler
hoặc renderer, không tạo task/heap riêng. Sự kiện đến trước khi UI chờ vẫn
được giữ. Có bộ đếm vòng lặp, post, lượt chờ, lượt thức có sự kiện, timeout
và lượt thức rỗng qua `metalio_ui_wake_stats()` để lấy chênh lệch khi đo.

UI vẫn giữ timeout dự phòng8/40ms, gồm các trang chưa có hợp đồng deadline.
Đây mới là bước đầu nối nguồn đánh thức, chưa giảm nhịp kiểm tra khi đứng yên.
Các đường hoàn tất SD/USB/mạng/ảnh và deadline từng trang/dịch vụ còn cần
chuyển đổi trước khi kéo dài thời gian chờ. Trong lúc giữ/debounce phím hoặc
chạm, sampler vẫn chạy theo nhịp cũ; không kéo dài chờ IRQ khi đang giữ.

CPU vẫn dùng320MHz; PM/DFS/automatic light sleep không được bật thêm. Đường
hiển thị S31 chưa có bộ khóa PM đầy đủ cho scan/TPS/DMA trong cây này; bật DFS
cần kiểm tra clock ngoại vi, underrun và độ trễ thức dậy trên thiết bị trước.
Không dùng quy trình lock-Sleep để thay thế một khoảng chờ UI ngắn. Đồng hồ
hiển thị theo phút và anti-fade tiếp tục theo lựa chọn đã lưu, không tự thay
chu kỳ hoặc tắt nhằm đạt số tiết kiệm điện.

Host tests và build kiểm tra logic, giới hạn bộ nhớ và đường lỗi; chúng không
đo dòng pin, độ trễ quang học, peak SRAM hay xác nhận chất lượng trên thiết bị.
