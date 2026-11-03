# Vòng đời WiFi và Bluetooth

OPDS, KoSync, OTA, tải font, đồng hồ mạng và quét WiFi dùng một cơ chế bảo vệ
chung. Tác vụ giữ quyền dùng radio đến khi đã đóng HTTP/file và hoàn tất dọn dẹp.
Lời gọi lồng trong cùng tác vụ được phép; tác vụ khác phải thử lại khi mạng bận,
không được giành hoặc tắt radio của tác vụ đang chạy. Bộ bảo vệ không tạo task
hoặc cấp phát heap riêng.

Kết nối WiFi tạm do một tác vụ tự mở được đóng khi tác vụ hoàn tất, lỗi hoặc bị
hủy, ngoại trừ phiên duyệt OPDS được mô tả dưới đây. Nếu tác vụ dùng kết nối thuần WiFi đã được người dùng bật, nó không tự đóng
kết nối đó. Kết nối thủ công tiếp tục theo công tắc và thời gian chờ đã cài;
thời gian chờ bằng 0 vẫn có nghĩa là không tự tắt do rảnh.
Chạm đọc sách hoặc lật trang không gia hạn thời gian WiFi rảnh. Tác vụ mạng
đang chạy và phiên vừa hoàn tất vẫn gia hạn thời gian chờ, kể cả khi kết thúc
giữa hai lần giao diện kiểm tra trạng thái.

Yêu cầu dừng một phiên thuần WiFi trong lúc tác vụ nền đang dùng được giữ lại,
để chủ tác vụ dọn radio sau khi kết thúc. Không ngắt HTTP giữa chừng. Tác vụ nền
không mượn phiên có máy chủ truyền file do trang Truyền file sở hữu: trang đó
phải dừng và join các yêu cầu trước khi trả quyền đọc thẻ cho giao diện đọc sách.
Bộ hẹn giờ dùng kiểm tra dừng khi rảnh, nên nhận/gửi file đang chạy không bị cắt.

Icon WiFi thể hiện phiên WiFi đang hoạt động hoặc tác vụ mạng đang kết nối/xử
lý; không chỉ hiển thị sau khi có địa chỉ IP. Icon không xác nhận Internet luôn
khả dụng. Icon Bluetooth lấy trạng thái stack đang chạy thay vì chỉ lấy công tắc
đã lưu. Thanh trạng thái cập nhật khi các trạng thái này đổi, không cần đợi pin
thay đổi hoặc đổi trang. OPDS đẩy cập nhật thanh trạng thái ngay trong bước mở
kết nối đồng bộ để người dùng thấy hoạt động mạng trong lúc chờ tải.

WiFi có thể tạm nhường bộ nhớ bằng cơ chế dừng BLE vốn có. Khi kết thúc, BLE đi
theo lựa chọn trước đó của người dùng; không tự ghi bật Bluetooth nếu người dùng
đã tắt. Bluetooth đang kết nối, quét, có thiết bị kết nối hoặc bị nhường cho WiFi
không bị bộ hẹn giờ rảnh ghi tắt nhầm. Tác vụ kết nối/quét cũng chặn Sleep tự
động; liên kết đã kết nối nhưng không có đầu vào vẫn theo chính sách khóa/ngủ
của máy. Khóa/ngủ hoặc hủy tác vụ do người dùng yêu cầu vẫn có đường dọn tài nguyên
riêng và khôi phục theo lựa chọn đã lưu khi thức dậy.

Các kiểm tra nguồn/host không thay cho kiểm tra radio trên thiết bị: cần đối
chiếu icon, log và trạng thái kết nối khi thành công, lỗi, hủy và có yêu cầu đồng
thời trên máy thực tế.

Khi đang đọc nội dung sách và WiFi chưa kết nối, lịch chỉnh giờ tự động được
trì hoãn để tránh tự bật radio giữa phiên đọc. Rời nội dung sách hoặc có WiFi
đang kết nối sẽ cho phép kiểm tra lịch cũ; các chức năng mạng vẫn có đường
chỉnh giờ riêng khi cần.

OPDS giữ WiFi trong suốt thời gian giao diện duyệt còn mở, qua các lần lấy danh
mục, phân trang, làm mới và tải sách. Mỗi yêu cầu trả quyền I/O sau khi đóng
HTTP/file, nhưng kết nối do OPDS tự bật chỉ được dọn khi đóng giao diện hoặc
rời trang. Timeout WiFi rảnh không kết thúc phiên duyệt đang mở. Nếu WiFi đã
được người dùng bật trước khi vào OPDS, đóng OPDS không tự tắt kết nối đó.
Kết nối thất bại vẫn được dọn để cho phép thử lại; Sleep dùng quy trình tạm
dừng/khôi phục radio vốn có, không giữ radio chạy xuyên giấc ngủ.
