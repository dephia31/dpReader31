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

Máy tự lấy giờ khi rảnh qua Wi-Fi đã lưu, lưu giờ vào RTC và hiển thị UTC+7. Cần mạng Internet để lấy giờ mới. Khi chưa kết nối, tác vụ có thể tạm kết nối mạng để đồng bộ; không chạy trong lúc truyền file, cập nhật hoặc các trạng thái đang chiếm kết nối. Đồng bộ khi đang online theo chu kỳ một giờ, khi offline theo chu kỳ sáu giờ; lỗi có thời gian chờ trước khi thử lại.
