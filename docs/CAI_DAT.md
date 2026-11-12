# Điều hướng Cài đặt

Trang Cài đặt hiển thị hồ sơ thiết bị và năm nhóm cha. Chạm một nhóm để mở danh sách các tùy chọn bên trong:

| Nhóm | Nội dung |
| --- | --- |
| Kết nối | WiFi, tự tắt WiFi, Bluetooth điều khiển lật trang, tự tắt Bluetooth, KOSync, OPDS, ngày giờ |
| Hiển thị | Phông/cỡ chữ hệ thống, tương phản, làm mới Home, tự khóa, nhấn đúp mở khóa, nguồn/ngủ và đèn nền/rung trên S31 |
| Cá nhân hóa | Kiểu Tủ Sách, màn hình khóa, mật khẩu khóa, chữ ký |
| Điều khiển | Phím cứng và cử chỉ |
| Cập nhật & Khôi phục | Cập nhật firmware, khôi phục thẻ nhớ trên S31, sao lưu cấu hình, log/chẩn đoán, khử bóng mờ, BOOT và khôi phục cài đặt gốc |

Ví dụ: cập nhật firmware theo đường **Cài đặt → Cập nhật & Khôi phục → Cập nhật hệ thống**. Ghép điều khiển BLE theo đường **Cài đặt → Kết nối → Điều khiển lật trang BLE**.

Back từ trang tùy chọn quay về nhóm đang xem và giữ vị trí cuộn của nhóm. Back từ nhóm quay về trang Cài đặt đầu tiên. Nút Home và thanh điều hướng vẫn chuyển trang trực tiếp. Các lựa chọn đang lưu không thay đổi khi chuyển nhóm. Cuộn trong nhóm để xem các mục cuối ở cỡ chữ lớn.

Thành phần sách gần đây trên Home dùng ba bìa sách đứng trên thanh kệ tối, cùng kiểu thanh kệ của Tủ Sách. Vẫn dùng dữ liệu sách gần đây và các tùy chọn Home hiện có; khi chưa có sách, chỉ hiển thị thanh kệ.

## Phím cứng và cử chỉ

Vào **Điều khiển → Phím cứng & Cử chỉ**, chọn AI, âm lượng +/−, nguồn hoặc một trong ba phím mặt trước. Mỗi phím có hai mục **Nhấn ngắn** và **Nhấn giữ**; chạm mục để mở danh sách tác vụ, dấu chọn chỉ tác vụ đang lưu. Back trong danh sách quay về phím, Back từ phím quay về danh sách phím.

AI, âm lượng và ba phím mặt trước nhận giữ ở 500 ms; nguồn giữ ngưỡng 700 ms hiện có. Nhấn giữ chỉ chạy một lần và không chạy thêm nhấn ngắn lúc thả. Nhấn ngắn âm lượng và các phím mặt trước khi đọc được thực hiện lúc thả. Hai phím âm lượng cùng giữ hủy thao tác. Phím mặt trước dùng shortcut khi đọc, ngoài trình đọc vẫn giữ điều hướng hệ thống. Các tác vụ trang/chương/menu đọc/đồng bộ chỉ có tác dụng trong ngữ cảnh đọc phù hợp; Home, làm mới và đèn có thể dùng ở ngoài trang đọc. Khóa/PIN, USB, cập nhật và các lớp đang chiếm đầu vào vẫn chặn shortcut.

**Mặc định hệ thống** giữ menu gán phím cho AI và luồng khóa/menu nguồn cũ cho nút nguồn. Phím giữa giữ ánh xạ nhấn giữ đã lưu trước đây. Các ánh xạ giữ mới lưu trong NVS của S31 bằng các khóa riêng; định dạng gói sao lưu cấu hình hiện có không thay đổi.

## Truyền tệp

Ba mục nhanh trên trang Tệp là **WiFi**, **Điểm phát** và **USB**. Điểm phát có SSID **dpReader31**, không mật khẩu. Khi chọn QR WiFi, mã dùng kiểu mạng mở; chọn **Web truyền sách** để hiện mã mở trang truyền file. WiFi đã lưu và mật khẩu của mạng nhà vẫn theo cơ chế cũ, độc lập với hotspot.

Trang web truyền tệp local được chia thành sáu tab: **Sách** (EPUB/TXT), **Thẻ nhớ** (duyệt thư mục và tệp bất kỳ), **Phông chữ** (TTF/OTF), **Hình ảnh** (tải ảnh và cắt hình nền khóa), **Đồng bộ** (KOSync/OPDS), **Cá nhân** (chữ ký). Mặc định mở Sách. Chuyển tab giữ lựa chọn tệp, thư mục đang duyệt và bản nháp cấu hình; trong lúc đang truyền/lưu, các tab tạm khóa để giữ tiến trình hiện tại. Có thể dùng phím mũi tên, Home và End khi đang focus thanh tab.

## Đèn khi đọc và thời gian tự động (1.2.0)

Trong **Điều khiển → Phím cứng & Cử chỉ**, hai công tắc độc lập bật/tắt vuốt dọc cạnh trái để chỉnh độ sáng và cạnh phải để chỉnh độ vàng. Chỉ hoạt động trong nội dung sách khi các bảng công cụ đã đóng. Vuốt tăng độ sáng bật lại đèn đang tắt; chỉnh độ vàng riêng giữ trạng thái bật/tắt. Thiết lập được lưu tại máy, chưa thêm vào định dạng sao lưu cấu hình.

Thanh độ sáng trong bảng nhanh và Cài đặt cũng bật lại đèn khi chọn mức lớn hơn 0. Phím AI hoàn tất chống dội khi thả để các lần nhấn giữ tiếp theo hoạt động; nhấn giữ không chạy thêm nhấn ngắn.

Trên S31, máy xét đồng bộ giờ tự động mỗi 24 giờ khi rảnh và chỉ dùng Wi-Fi đã kết nối, lưu giờ vào RTC và hiển thị UTC+7. Offline hoặc đồng bộ lỗi thì bỏ lượt đó, không tự bật Wi-Fi và không thử lại trong chu kỳ. Khóa/Sleep chặn đồng bộ tự động. Chỉnh giờ thủ công và nhu cầu giờ hợp lệ của tác vụ mạng vẫn có đường xử lý riêng; không ngắt truyền tệp hoặc OTA đang hoạt động.

## Lịch bật/tắt đèn trắng và đèn ấm (S31)

Vào **Hiển thị → Đèn nền & Rung → Lịch đèn trắng / Lịch đèn ấm**.
Mỗi kênh có công tắc lịch, giờ bật, giờ tắt và độ sáng riêng 1–100%.
Chọn giờ rồi phút, sau đó **Lưu lịch** để áp dụng; **Hủy** hoặc quay lại bỏ
các chỉnh sửa chưa lưu. Hai giờ phải khác nhau. Lịch lặp hằng ngày theo giờ
Việt Nam (GMT+7), mặc định tắt. Ví dụ đèn ấm bật 20:00, tắt 06:00 sáng hôm sau.

Lịch dùng giờ RTC của máy, không bật WiFi hoặc gọi NTP riêng. Nếu đồng hồ chưa
hợp lệ, lịch chờ đến khi giờ được đặt hoặc đồng bộ. Khi máy đang thức, lịch
được xét khoảng mỗi giây và chỉ đổi đèn khi đến mốc. Nếu bỏ lỡ nhiều mốc hoặc
đổi giờ, máy áp dụng khung hiện tại, không phát lại từng mốc đã qua.

Khóa/Sleep luôn tắt cả hai đèn. Lịch không đánh thức máy và không bật đèn trong
các lần kiểm tra nền khi đang ngủ. Khi thức lại, máy áp dụng khung giờ hiện tại.
Chỉnh độ sáng/nhiệt màu hoặc bật/tắt thủ công có hiệu lực đến mốc lịch kế tiếp
hoặc đến lần Sleep/wake. Tắt công tắc lịch dừng tự động hóa của kênh đó và giữ
mức đèn hiện tại để người dùng tiếp tục chỉnh tay.

Hai kênh cùng bật vẫn chia sẻ trần công suất đèn hiện có; nếu tổng mức yêu cầu
vượt trần, hai mức được giảm theo tỷ lệ. Lịch không tạo task/timer riêng và
không ghi flash mỗi lần đến mốc; cấu hình chỉ ghi khi người dùng lưu.
Lịch lưu tại máy trong nhóm đèn nền và bị xóa bởi khôi phục cài đặt gốc;
chưa đưa vào định dạng sao lưu cấu hình hiện tại.

Nếu đọc cấu hình lịch đèn bị lỗi, máy không lưu đè lịch chưa đọc được. Mở lại
trang lịch hoặc thử Lưu để đọc lại; vòng nền không tự lặp việc đọc NVS lỗi.

Trong nội dung sách, vuốt xuống mở bảng cài đặt nhanh giống thao tác kéo từ cạnh trên. Vuốt cạnh chỉnh đèn, phóng ảnh, chọn văn bản và các bảng công cụ đang mở giữ thao tác riêng.

### Pop menu và Quick Settings trên Home

Trong giao diện Đơn giản và Đơn giản hoàn toàn, vuốt lên trên Home, kể cả ngay trên thẻ Đọc tiếp, để mở **pop menu** với Mở sách, Truyền file, OPDS và Cài đặt. Chạm ngoài hoặc vuốt xuống để đóng pop menu.

Ở cả ba giao diện, vuốt xuống tại bất kỳ vùng nào trên Home để mở **Quick Settings**. Các nút điều hướng dưới Home được thực hiện khi nhấc tay, để có thể bắt đầu vuốt ngay từ vùng nút.

### Mở file trong I Love Book

Chọn **Mở sách** để duyệt TXT/EPUB trực tiếp trên thẻ nhớ. Mỗi lần vào mở sẵn `/sdcard/Books`; nếu không có, thử `books`, thư mục sách đã cài, rồi gốc thẻ. Chạm thư mục để vào, dùng **Thư mục cha** hoặc mũi tên trở lại để lên một cấp; tại gốc, mũi tên trở về Home. **Books** trở về thư mục sách mặc định. **Tải lại** đọc lại thư mục hiện tại từ trang đầu. Dùng Trước/Tiếp hoặc vuốt trái/phải trong danh sách để chuyển trang. Danh sách chỉ giữ tám mục mỗi trang, hiển thị định dạng và dung lượng sách, theo thứ tự trên thẻ.

**Bật USB** mở trực tiếp chế độ ổ đĩa USB hiện có để chép sách từ máy tính. Tháo ổ đĩa an toàn trên máy tính rồi dừng USB trên thiết bị; trình mở sách trở lại thư mục vừa duyệt và tải lại danh sách. Khi thiếu thẻ, lỗi đọc hoặc sách đã bị chuyển/xóa, màn hình báo lỗi để thử lại.


## Sao lưu cấu hình trên thẻ nhớ

Tệp `/sdcard/dpReader-settings.backup` chứa cấu hình Wi-Fi đã lưu, tài khoản KoSync (máy chủ, tên đăng nhập, khóa xác thực và tự đồng bộ), tài khoản OPDS (địa chỉ kho, tên đăng nhập và mật khẩu), cách lật trang, ánh xạ phím, số trang giữa các lần làm mới toàn màn hình và cấu hình footer, gồm cả công tắc hiện chân trang.

Dùng Sao lưu cấu hình và Khôi phục cấu hình trong Cập nhật & Khôi phục. Gói mới vẫn đọc các bản sao lưu cũ. Bản cũ chưa có tài khoản hoặc công tắc footer sẽ giữ các giá trị hiện tại của những mục đó. Tệp sao lưu có chứa thông tin đăng nhập Wi-Fi và OPDS cùng khóa KoSync ở dạng không mã hóa.

Từ firmware 1.3.3, lần bấm Kiểm tra cập nhật OTA đầu tiên cho chọn Full hoặc Love Book. Máy lưu nhánh đã chọn và tự kiểm tra nhánh đó ở những lần sau. Nếu chọn khác firmware đang cài, hộp xác nhận nêu rõ việc chuyển nhánh trước khi tải và cài.
