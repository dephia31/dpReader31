# Tác vụ nền trong phiên đọc

Thiết kế và hợp đồng bảo trì của vòng UI xem [UI_EVENT_DEADLINE.md](UI_EVENT_DEADLINE.md).

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

Trang đọc S31 có hợp đồng `next_tick_ms`: trả0 để giữ nhịp8/40ms, hoặc hạn
monotonic gần nhất khi đã hiển thị thành công, không có thao tác/tác vụ chờ và
cache trang liền kề đã đủ. Kiểm tra cache chỉ thử khóa với timeout0, không chờ
worker hoặc cấp phát. Trang có ảnh/nền, hộp thoại/nhấn giữ/double-tap, mở sách,
đổi font, phục hồi thẻ, lỗi lưu, chuẩn bị bìa/chương hoặc cache thiếu vẫn giữ
nhịp cũ. Có ít nhất400ms yên sau input trước khi được kéo dài lượt chờ.

Chỉ trang đọc tĩnh offline, Bluetooth tắt và KoSync tự động tắt mới opt-in ở
bước này. Vòng UI chọn hạn sớm nhất giữa trang/đồng hồ/thống kê, phím nguồn,
media500ms, dịch vụ mạng/lịch đèn500ms, font750ms, tự khóa và anti-fade; trần
mỗi lượt chờ200ms. Thời gian đã qua khi push màn hình được trừ bằng đồng hồ
monotonic mới, không cộng lại vào deadline. Trang khác và trạng thái mạng/BLE
vẫn giữ nhịp dự phòng cũ đến khi có đầy đủ hợp đồng tương ứng.

Input đang có trong chốt hoặc hàng đợi AI/âm lượng khiến thời gian chờ
bằng0, không ngủ giữa các input đã xếp hàng. Input mới ngắt semaphore đang chờ;
phím nguồn có sự kiện được kiểm tra ngay thay vì chờ kỳ200ms. Trong lúc giữ/
debounce phím hoặc chạm, sampler vẫn chạy nhịp cũ. Chưa có
số đo dòng pin/độ trễ phần cứng hoặc khẳng định mọi trang đều giảm số wakeup.

Bản thử từRC22 giữ CPU tối đa320MHz khi xử lý; chỉ cho phép SDK xuống160MHz
trong cửa sổ đọc tĩnh đủ điều kiện. Có khóa xuyên giao dịch màn hình/I²C/SD/
worker và lấy lại khóa trước khi báo input. Không bật automatic light sleep
hoặc tickless; chi tiết, đường lỗi và cách đo xem [CPU_POWER_POLICY.md](CPU_POWER_POLICY.md).
Đồng hồ theo phút và anti-fade giữ lựa chọn đã lưu. Hiệu quả pin và độ trễ
chuyển xung chưa được đo trên thiết bị.

Host tests và build kiểm tra logic, giới hạn bộ nhớ và đường lỗi; chúng không
đo dòng pin, độ trễ quang học, peak SRAM hay xác nhận chất lượng trên thiết bị.
