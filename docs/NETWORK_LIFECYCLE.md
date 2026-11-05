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

Sau khi mở khóa S31, thao tác KoSync, OPDS, quét WiFi và mở truyền file xử lý
trước yêu cầu dọn WiFi sau Sleep đang chờ, không phụ thuộc vòng nền sau khung
hình đọc sách. Chỉ mở lại quyền khi đã có yêu cầu khôi phục từ đường thức dậy;
nếu dừng driver thất bại thì vẫn chặn và thử lại có giới hạn. Trạng thái ERROR
chỉ được dọn khi driver, máy chủ, giao diện mạng, tải lên và cấu hình đều đã
ngừng; không dùng trạng thái lỗi để giành mạng đang hoạt động.

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
Kết nối thất bại vẫn được dọn để cho phép thử lại; Sleep tắt WiFi và không tự khôi phục, không giữ radio chạy xuyên giấc ngủ.

Khi Bluetooth được bật và stack khởi động lại (gồm khởi động máy hoặc sau
Sleep), máy tự yêu cầu kết nối thiết bị đã lưu gần nhất nếu chưa có mục tiêu
kết nối được giữ lại. Không có thiết bị đã lưu thì không tự quét thiết bị lạ.
Yêu cầu chỉ chạy sau khi host sẵn sàng; bước khởi động/poll nền nằm sau mở
sách, tick trang, chuyển trang và hiển thị. Yêu cầu mở sách, chờ thẻ, dàn trang
hoặc chưa có trang đọc được hiển thị thành công sẽ trì hoãn BLE. Tắt Bluetooth
vẫn được xử lý; liên kết đang hoạt động không bị dừng chỉ vì mở sách mới.
Không thêm task dò riêng hoặc chờ ghép đôi đồng bộ trong đường mở sách.

Khóa/Sleep giữ một chốt riêng chặn BLE khởi động, scan và kết nối lại. Mỗi
lần kiểm tra dừng phải thu được kết quả thực tế từ vòng đời NimBLE; chốt đã
đặt không đồng nghĩa task đã thoát. Nếu dừng lỗi hoặc timeout, máy không vào
Sleep với BLE chưa được xác nhận dừng; khi còn khóa sẽ tiếp tục thu kết quả
với lượt chờ ngắn, kể cả khi USB khiến máy không thể Sleep. Không ép xóa
task SDK đang hoạt động. Mở khóa hoặc hủy thao tác khóa chưa được chấp nhận
mới trả chốt; việc bật lại vẫn chờ trang đọc hiển thị thành công. Công tắc
Bluetooth đã lưu không bị ghi tắt, để máy có thể khôi phục lựa chọn sau unlock.

WiFi dùng chốt khóa/Sleep riêng: chỉ báo đã dừng sau khi HTTP, WiFi stop và
deinit trả kết quả xác nhận. Nếu SDK dừng lỗi, giữ các tài nguyên còn sống
để thử dọn lại, không báo OFF giả hoặc đưa máy vào Sleep. Tác vụ chỉnh giờ,
quét và mở WiFi không được bật radio trong lúc chốt khóa còn giữ. Lỗi dọn
WiFi tạm giữ driver/netif/quyền nhường BLE để lần dừng sau có thể thu hồi.

Sau khóa/Sleep, WiFi giữ tắt cả khi mở khóa. Không lưu phiên để tự khôi phục,
không hẹn kết nối lại. Mở khóa chỉ trả chốt sau khi trang đã hiển thị và dọn
phần tài nguyên còn sót nếu cần. Người dùng bật WiFi hoặc tác vụ mạng được gọi
rõ ràng mới mở kết nối. Bluetooth vẫn có chính sách khôi phục riêng.

Trên S31, NTP tự động chỉ dùng STA đã kết nối: mỗi 24 giờ xét một lượt khi máy rảnh.
Nếu offline thì bỏ lượt ngày đó; lỗi NTP cũng không thử lại trong chu kỳ.
Mốc lượt đã xét được lưu riêng khi giờ RTC hợp lệ, tránh lặp sau khởi động lại. Không tự bật
WiFi khi offline, sau Sleep hoặc khi đang khóa. Task chỉnh giờ có stack 6144
byte, chỉ tồn tại trong lượt chỉnh giờ rồi tự xóa; không giữ SNTP thường trú.
Chỉnh giờ thủ công vẫn được phép mở kết nối tạm và dọn khi hoàn tất.

Kiểm tra cập nhật, tải/ghi firmware OTA và file đang truyền giữ quyền sử dụng
mạng đến hết dọn HTTP/file. Khóa thủ công và tự động phải hoãn khi OTA bận,
kể cả cửa sổ worker chưa kịp chạy; timeout WiFi và khôi phục sau Sleep không
được ngắt chủ mạng. Hủy OTA do người dùng vẫn dùng đường hủy riêng của OTA.

So sánh OTA nhận dạng cả phiên bản thử `X.Y.ZRCn-vn` và hậu tố cũ `X.Y.Z-rcn`. Bản chính thức cùng số nền mới hơn mọi RC của số nền đó; RC tăng theo số, không theo thứ tự chữ. Các RC đã cài trước bản sửa bộ so sánh vẫn cần cập nhật một lần bằng TF/USB vì logic kiểm tra chạy trong firmware hiện tại.
