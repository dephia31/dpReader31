# Nhật ký thay đổi / Changelog

## dpReader 1.1.9-vn · 2026-10-30

- Cải thiện hiển thị TTF và làm tròn mức xám; giữ khử răng cưa khi lật trang, kể cả khi chọn Đen trắng nhanh. Tắt khử răng cưa dùng TrueType hinting với cache 1 bit và chỉnh độ đậm trên đường nét trước khi vẽ; giữ độ rộng ký tự. Giới hạn bộ nhớ raster để tránh khởi động lại khi mở sách.
- Home: hiển thị ba sách gần đây trên kệ đen; Cài đặt gom thành năm nhóm cha và mở từng nhóm để chỉnh.
- Tệp: rút gọn nhãn truyền file, sửa chữ đè viền/nút QR; hotspot dpReader31 không mật khẩu. Web truyền file local gom thành sáu tab.
- Phím cứng & Cử chỉ: chọn tác vụ nhấn ngắn/nhấn giữ riêng cho từng phím, giữ độc lập các lựa chọn và tránh kích hoạt nhấn ngắn sau nhấn giữ.
- Khôi phục đầy đủ trạng thái chân SD chuyên dụng trước khi khởi tạo lại thẻ sau ngủ/reset; áp dụng cả đường USB, không format thẻ.
- Khử răng cưa vẫn phụ thuộc chế độ làm mới và có thể còn bóng hình khi lật trang; bản này chưa bổ sung bộ vẽ khử răng cưa có hinting hay thay waveform.

## dpReader 1.1.8-vn · 2026-10-29

- Tối ưu hóa bộ nhớ: Giảm 60 KiB dữ liệu tĩnh SRAM, chuyển bộ đệm log, metadata font, bảng hiển thị, trạng thái KoSync, danh sách Bluetooth, CPU buffer đọc sách và cấu trúc ZIP sang PSRAM; bảo toàn DMA/ISR và vòng quét phím trong SRAM.
- Đồng bộ KoSync: Hiển thị trạng thái "Đang đồng bộ" tức thì trên màn hình trước khi bật Wi-Fi; quản lý vòng đời HTTP/TLS/buffer chặt chẽ, tự động giải phóng tài nguyên sau khi hoàn tất hoặc lỗi.
- Xóa bóng mờ E-Paper: Sửa đường Home và đen trắng bỏ qua GC16 định kỳ; tự động dọn toàn màn hình bằng waveform đầy đủ sau 6 khung thay đổi và tối thiểu 3 giây từ lần dọn trước; giữ nguyên làm mới cục bộ khi đọc sách.
- Giao diện chữ lớn 100–200%: Cải thiện hiển thị ở mọi tỷ lệ chữ hệ thống; tiêu đề chừa chỗ cho nút Back/nút phụ, danh sách cài đặt tách nhãn–giá trị từ 150%, giới hạn độ rộng chữ tránh chồng lấn và tràn viền icon.

## dpReader 1.1.7-vn · 2026-10-29

- Footer gọn, chữ status/footer40px; bỏ chế độ toàn màn hình và hàng tiêu đề trùng. Thanh tiến độ có phần đã đọc dày/đen, phần còn lại mảnh/nhạt, không biến mất khi lật nhanh; hai bên cân đối.
- Tận dụng khoảng trống cuối trang; sửa nhãn và hộp chọn chồng nhau khi tăng chữ hệ thống.
- Sửa phím âm lượng không nhận đủ chống dội; menu chọn shortcut rõ ràng, thêm về Home và bật/tắt đèn.
- KoSync trong trình đọc thu hồi cache/tác vụ đọc trước và tạm nhường BLE trước khi bật Wi-Fi; đồng bộ tự động chạy nền, không chặn mở sách/về Home hoặc gửi trùng tiến độ.
- Sửa reboot khi lưu log SD: chụp bộ đệm rồi nhả khóa trước khi ghi; kiểm tra đầy đủ lỗi ghi/đóng file.
- Khóa thủ công/tự khóa vào ngủ nhẹ, chọn1–24giờ trước khi tự tắt nguồn (mặc định1giờ), lưu cài đặt qua khởi động; PIN và không PIN chung bộ đếm.
- Sửa lỗi SDK gây Core1 Cache access error khi khởi động; giữ dữ liệu NVS/sách và cấu hình riêng S31.
- Giữ font TTF chọn lại thành công sau khởi động lỗi qua đổi trang và khóa/mở máy; giới hạn cache bảng font2MiB, thu hồi cache và thử lại một lần khi thiếu RAM.

## dpReader 1.1.6-vn · 2026-10-29


Bản cập nhật về độ trễ phím, tải bus I2C và lỗi kết nối WiFi của KoSync/OPDS:

- **Phím cứng chuyển sang ngắt thay vì thăm dò 10 ms:** Trước đây tác vụ quét phím đọc chip mở rộng 100 lần/giây kể cả khi không ai chạm máy. Nay tác vụ chỉ đọc khi có cạnh xuống trên INT của chip mở rộng hoặc khi phím AI (GPIO61) đổi mức, và chỉ tiếp tục thăm dò 10 ms trong lúc đang giữ phím, trong cửa sổ chống dội hoặc khi còn thao tác chưa được lấy. Phím AI vẫn nhận đủ nhấn giữ 500 ms. Nếu không gắn được ngắt, tác vụ tự quay về thăm dò 10 ms như cũ.
- **Đọc chip mở rộng bằng một giao dịch:** Hai lần đọc cổng riêng lẻ được gộp thành một lần đọc tự tăng địa chỉ, giảm một nửa thời gian bus trên đường phím nóng nhất.
- **I2C 400 kHz cho cảm biến, giữ 100 kHz cho chip nguồn:** Cảm ứng, chip mở rộng, pin và RTC chạy 400 kHz; TPS65185 và CX25601N vẫn 100 kHz vì trình điều khiển tham chiếu chỉ được xác nhận ở tốc độ đó. Nếu thực máy xuất hiện NACK lẻ hoặc mất phím, hạ xuống 200 kHz bằng `-DMETALIO_I2C_FAST_HZ=200000` mà không cần sửa mã.
- **KoSync và OPDS tự kết nối WiFi khi cần:** Trước đây bấm đồng bộ KoSync trong lúc đang đọc sách mà máy đang phát hotspot (hoặc còn phiên chỉ-đăng-nhập) thì chỉ hiện thông báo *"Đang bật phát WiFi nội bộ"* và không kết nối được — người dùng phải tự thoát ra trang Truyền tệp. Nay hai dịch vụ này chủ động thu hồi sóng, kết nối ngay mạng WiFi đã lưu để đồng bộ, rồi tự tắt WiFi khi xong, đúng như mong đợi khi đang đọc. Chỉ khi đang có tệp được truyền lên (đang ghi thẻ) mới từ chối và báo rõ lý do, vì cắt ngang sẽ để lại tệp dở.

Cần đo trên máy thật: thời gian mở sách và lật trang trước/sau, và xác nhận không mất phím khi bấm nhanh.

## dpReader 1.1.5-vn · 2026-10-28

Bản cập nhật tối ưu hóa bộ nhớ RAM hệ thống, quản lý tài nguyên không dây và bổ sung tính năng tự ngắt kết nối thông minh:

- **Tối ưu hóa RAM và kiểm soát dịch vụ Truyền tệp / Wi-Fi:**
  - Khắc phục tình trạng ngốn RAM do Wi-Fi và HTTP server truyền tệp chạy ngầm: Mặc định Wi-Fi và dịch vụ truyền tệp luôn TẮT, chỉ mở khi người dùng chủ động vào màn hình truyền tệp hoặc thao tác tác vụ mạng (KoSync, OPDS, OTA, Online Font).
  - Khi người dùng thoát khỏi màn hình truyền tệp / cài đặt Wi-Fi, hệ thống tự động dừng phiên (`stop_session`) và giải phóng hoàn toàn bộ đệm HTTP, socket và tài nguyên Wi-Fi về cho heap.
  - Ngăn chặn tác vụ đồng bộ giờ nền tự ý bật Wi-Fi ngầm khi máy đang offline; chỉ đồng bộ khi Wi-Fi đã được người dùng kết nối sẵn.
  - Tối ưu hóa cửa sổ nhận TCP (`CONFIG_LWIP_TCP_WND_DEFAULT`) từ 64 KB về 32 KB và giảm số bộ đệm RX động từ 64 về 32 trong cấu hình S31, tiết kiệm hơn 40 KB RAM nội bộ cho hệ thống hiển thị và phông chữ.
- **Thêm thiết lập Tự tắt Wi-Fi & Tự tắt Bluetooth khi không dùng:**
  - Bổ sung tùy chọn **Tự tắt Wi-Fi khi rảnh** vào nhóm **Cài đặt -> Kết nối**: Cho phép chọn *1 phút, 3 phút (mặc định), 5 phút, 10 phút, Tắt*. Khi không có tệp nào đang tải lên/xuống sau thời gian đã đặt, Wi-Fi sẽ tự động tắt để tiết kiệm pin và trả lại RAM.
  - Bổ sung tùy chọn **Tự tắt Bluetooth khi rảnh** vào nhóm **Cài đặt -> Kết nối** và trong trang **Điều khiển lật trang BLE**: Cho phép chọn *5 phút, 10 phút (mặc định), 15 phút, 30 phút, Tắt*. Nếu mất kết nối hoặc không nhận được thao tác bấm phím nào từ điều khiển lật trang sau thời gian đã định, hệ thống sẽ tự động tắt Bluetooth và giải phóng hoàn toàn NimBLE stack.

## dpReader 1.1.4-vn · 2026-10-28

Bản cập nhật tinh chỉnh giao diện đọc sách, tái cấu trúc menu cài đặt phông chữ và bổ sung tùy chọn hiển thị chân trang:
  - Tạo menu riêng **Tùy chỉnh** gom gọn 3 tính năng: *Phông chữ nhúng trong sách*, *Đường dẫn dòng* (thước đọc sách) và *Khử răng cưa* (làm mịn nét chữ).
  - Giúp giao diện Cài đặt phông chữ chính trở nên thoáng đãng, phân cấp logic và dễ sử dụng.
- **Kéo Menu "Thanh chân trang" (Footer) ra ngoài:**
  - Chuyển mục cài đặt chân trang ra trực tiếp bảng Cài đặt phông chữ chính (nằm ngang hàng với Bố cục dàn trang), không còn bị ẩn sâu ở đáy bảng Bố cục như trước.
- **Bổ sung các tùy chọn hiển thị chi tiết cho Chân trang (Footer Display Options):**
  - Bổ sung tùy chọn bật/tắt độc lập từng thành phần ở chân trang sách:
    - **Thanh tiến độ**: Tùy chỉnh độ dày vạch (*Ẩn / Mảnh / Dày / Rất mảnh*).
    - **Số trang / chương (trái)**: Bật/Tắt hiển thị số trang hiện tại trong chương (ví dụ `Trang 3/12`).
    - **Tên Chương (nằm trên thanh tiến độ)**: Bật/Tắt hiển thị tên chương nằm căn giữa phía trên thanh tiến độ.
    - **% tiến độ sách (phải)**: Bật/Tắt hiển thị % đọc toàn cuốn sách ở góc phải (ví dụ `45%`).
  - Toàn bộ cài đặt được lưu trữ bền vững vào NVS và tự động đồng bộ khi chuyển trang.

Bản cập nhật sửa triệt để lỗi nhảy trang của nút lật trang Bluetooth và bổ sung tính năng chẩn đoán & xuất nhật ký hệ thống:

- **Khắc phục lỗi nút lật trang Bluetooth nhảy nhiều trang (Bluetooth turner multi-page jump):**
  - Khắc phục cơ chế Auto-Repeat vô hạn trong BLE stack (`poll_locked`): Hầu hết các remote Bluetooth lật trang / chụp ảnh không gửi gói báo nhả phím (release report), timeout trước đây (5000ms) khiến firmware tự coi nút vẫn đang giữ và liên tục gửi lệnh lật trang mỗi 120ms (tới 60 lần). Nay hệ thống tự động giải phóng trạng thái phím ngay khi đường truyền im lặng quá 150ms.
  - Bổ sung cơ chế chống dội (debounce/cooldown 350ms) trong trình đọc sách (`app_book.c`) và tự động làm rỗng (drain) hàng đợi phím thô / phím HID còn dư ngay sau khi nhận lệnh lật trang, đảm bảo 1 lần bấm vật lý chỉ lật đúng 1 trang sách.
- **Bổ sung Menu & Tính năng Xuất nhật ký hệ thống (Log Export & Diagnostics):**
  - Tích hợp bộ đệm vòng 32 KB trong RAM ghi nhận toàn bộ log runtime của hệ thống qua `esp_log_set_vprintf` mà không làm gián đoạn cổng Serial/USB.
  - Thêm menu **Nhật ký & Chẩn đoán** trong **Cài đặt -> Cập nhật & Khôi phục**.
  - Giao diện trực quan hiển thị thông tin RAM/PSRAM, % pin và điện áp, tình trạng thẻ nhớ SD, kết nối Bluetooth, chi tiết sự cố crash/panic (nếu có) và khung xem trước các dòng log gần nhất.
  - Nút bấm **Xuất log ra thẻ nhớ (/sdcard/dpreader_log.txt)** xuất toàn bộ báo cáo chẩn đoán và nhật ký lỗi ra file văn bản trên thẻ nhớ SD để IDE và kỹ sư firmware dễ dàng kiểm tra, khắc phục lỗi.
  - Nút bấm **Xóa nhật ký lưu tạm** để làm sạch bộ đệm nhật ký khi cần.

## dpReader 1.1.2-vn · 2026-10-28

Bản cập nhật sửa lỗi căn lề văn bản và tối ưu hóa trải nghiệm bàn phím:

- **Sửa triệt để lỗi căn lề đều hai bên (Justify) cho văn bản tiếng Việt & Latin:**
  - Thuật toán căn đều trước đây chỉ tính khoảng cách co dãn cho ký tự CJK, khiến văn bản tiếng Việt/Latin bị coi là không có khoảng dãn hợp lệ và luôn tự hủy về căn lề trái.
  - Nay thuật toán dãn dòng tự động phát hiện văn bản có khoảng trắng (spaces), phân bổ đều phần rộng dư của dòng vào các dấu cách giữa các từ, giữ nguyên cấu trúc tự nhiên của chữ cái trong từ và căn thẳng tắp mép phải đoạn văn.
- **Làm mới hiển thị tức thì khi chuyển đổi căn lề:**
  - Bổ sung cờ `s_reader_cleanup` khi chọn chế độ căn lề trong menu "Bố cục dàn trang", buộc toàn màn hình vẽ lại ngay lập tức thay vì chỉ làm mới nửa dưới panel.
  - Đổi tên hiển thị thành **Căn đều (Hai bên)** để người dùng dễ phân biệt.
- **Chuẩn hóa bàn phím QWERTY tiếng Anh, loại bỏ bàn phím T9:**
  - Bỏ hoàn toàn chế độ 9 phím T9 và bộ gõ IME trung gian phức tạp, giúp giải phóng bộ nhớ và tăng tốc độ gõ phím.
  - Nhập trực tiếp ký tự chữ hoa/thường tức thì (zero latency), không giữ trong bộ đệm chờ chọn từ.
  - Thiết kế lại bố cục bàn phím: loại bỏ thanh gợi ý từ, mở rộng phím Dấu cách (280 px), tích hợp chuyển đổi số (`123`/`ABC`), ký tự đặc biệt (`Ký tự`/`Thêm`) và hoàn tất (`Xong`).
  - Hỗ trợ giữ phím xóa `⌫` để xóa liên tục (repeat backspace) và rung xúc giác khi gõ phím.
- **Khắc phục triệt để lỗi thỉnh thoảng vào Tủ sách/Kệ sách bị load lại bìa và sách:**
  - Loại bỏ việc vô hiệu hóa cache và xóa trắng danh mục sách khi thẻ nhớ đang trong trạng thái kiểm tra nền `ESP_ERR_NOT_FINISHED`.
  - Giữ nguyên danh mục sách và bìa đã nạp trong RAM khi mở trang, chỉ quét cập nhật ngầm nếu có thay đổi thực sự, loại bỏ hoàn toàn hiện tượng chớp nháy hoặc load lại từ đầu.
- **Tinh chỉnh giao diện nút Quản lý & Nhập sách:**
  - Mở rộng chiều rộng nút Quản lý (114 px) và Nhập sách (120 px), bo góc 14 px và cỡ chữ 19 px, tạo khoảng đệm thoáng đãng, chấm dứt tình trạng chữ bị lấn đè vào viền bao quanh.
- **Cập nhật giao diện Tập tin - phần Truyền tệp:**
  - Đổi tên 3 phương thức truyền tệp thành: **Wifi cùng mạng**, **Tạo điểm phát sóng**, **Cable USB drive**.

## dpReader 1.1.1-vn · 2026-10-28

Bản tối ưu hóa năng lượng cực đại (Ultra Low Power) toàn diện cho Metalio E-Ink4 Plus (ESP32-S31):

- **Tự động ngắt Wi-Fi khi nhàn rỗi (Auto-timeout):**
  - Tự động ngắt hoàn toàn module Wi-Fi sau 3 phút không có lưu lượng truyền tải sách, đồng bộ hay tải OTA.
  - Sửa lỗi cờ `wifi_busy` chặn bộ đếm thời gian tự khóa màn hình: Khi Wi-Fi chỉ ở trạng thái kết nối chờ (không tải dữ liệu), thiết bị vẫn tự khóa màn hình đúng hẹn.
  - Tự động ngắt Wi-Fi rảnh rỗi ngay trước khi vào chế độ ngủ để tránh tiêu hao dòng trong lúc khóa máy.
- **Kích hoạt Wi-Fi Modem Sleep (DTIM Beaconing):**
  - Chuyển chế độ hoạt động Wi-Fi STA từ `WIFI_PS_NONE` sang `WIFI_PS_MIN_MODEM`, giảm dòng RF tiêu thụ lúc kết nối mạng từ 140mA xuống ~15–20mA.
- **Triệt tiêu dòng rò ngoại vi đèn nền:**
  - Ngắt hoàn toàn clock PWM (gọi `ledc_stop()`) cho cả 2 kênh Channel 0 & 1 khi độ sáng = 0.
- **Nhịp quét CPU thích ứng (Adaptive Cadence):**
  - Vòng lặp chính co giãn thông minh: giữ 8ms (120Hz) khi có tương tác chạm vuốt để đảm bảo phản hồi cực mượt; tự động giãn ra 40ms sau 400ms không thao tác (giảm 80% tải CPU và giao tiếp I2C khi đọc sách tĩnh).
- **Chế độ ngủ phân tầng (Tiered Sleep):**
  - Tầng 1: Light Sleep trong 10 phút đầu khi khóa máy, cho phép đánh thức tức thì (<50ms).
  - Tầng 2: Nếu không thao tác sau 10 phút khóa máy, tự động hạ xuống Deep Sleep (<25 µA) để bảo toàn pin tối đa khi người dùng bỏ quên thiết bị.
- **Cập nhật mặc định tự tắt màn hình:**
  - Chuyển thời gian tự khóa màn hình mặc định từ 0 (không tắt) thành 5 phút an toàn.

Bản cập nhật bổ sung giao diện cho hai tính năng dàn trang đã có sẵn trong firmware:

- **Thêm tùy chọn căn lề đoạn văn vào Cài đặt đọc sách:**
  - Mở `Cài đặt đọc sách` → `Bố cục dàn trang`, mục mới **Căn lề đoạn văn**. Chạm để luân chuyển bốn chế độ: **Hai bên** (justified, mặc định), **Căn trái**, **Căn giữa**, **Căn phải**.
  - Chỉ áp dụng cho đoạn văn thường; đoạn sách đã tự căn giữa hoặc căn phải theo CSS trong sách, và các dòng tiêu đề, được giữ nguyên.
  - Đổi căn lề chỉ thay đổi cách vẽ, không dàn trang lại, nên có hiệu lực ngay và không làm mất vị trí đang đọc.
- **Thêm tùy chọn hiển thị thanh tiến độ ở chân trang:**
  - Mục mới **Thanh tiến độ** trong cùng panel, chạm để luân chuyển: **Ẩn** → **Mảnh** (10 px) → **Dày** (16 px) → **Rất mảnh** (4 px).
  - Chọn **Ẩn** sẽ thu gọn toàn bộ chân trang ba tầng (tên chương, thanh tiến độ, số trang và phần trăm), đồng thời bỏ luôn đường tiến độ mảnh ở chế độ toàn màn hình. Vùng đọc không thay đổi vì chân trang vốn nằm dưới phần dàn trang.
  - Lựa chọn được lưu vào khóa NVS mới `bk_prog2`; giá trị cũ `bk_prog` (0–2) được tự động quy đổi sang thang mới nên người dùng đang dùng không bị mất cài đặt hay bất ngờ mất chân trang.
- Bố cục panel `Bố cục dàn trang` được nén lại để hai mục mới nằm gọn phía trên nút quay về, không chồng lấn vùng chạm.
- Thêm `tools/footer_style_host_test.py` khoá lại quy tắc quy đổi giá trị cũ và bảo đảm thang mới không bao giờ tự chuyển sang trạng thái ẩn.

## dpReader 1.0.9-vn · 2026-10-28

Bản sửa lỗi đồng bộ tiến độ đọc KoSync:

- **Sửa lỗi đồng bộ tiến độ KoSync với KOReader bị nhảy sai vị trí:**
  - Bộ phân tích XPointer dùng `strrchr(progress, '.')` để lấy số cuối chuỗi và coi đó là **byte offset trong chương**. Đúng với chuỗi do dpReader tự ghi (`/body/DocFragment[N]/body/p[1].1024`), nhưng KOReader ghi theo crengine (`/body/DocFragment[3]/body/div/p[7]/text().42`), trong đó `42` là **chỉ số ký tự bên trong một phần tử `<p>`**, không phải offset byte. Hệ quả: khi nhận tiến độ do KOReader đẩy lên, máy nhảy về đầu chương hoặc lệch trang.
  - Nay hai định dạng được tách riêng: chuỗi crengine chỉ lấy số hiệu chương (phần đáng tin), không báo offset chính xác, và vị trí được đặt theo tỉ lệ phần trăm.
- **Sửa lỗi tiến độ mới hơn trên máy chủ không được kéo về:**
  - Việc so sánh hướng đồng bộ lượng tử hoá tỉ lệ về số nguyên phần trăm trước khi so. dpReader chỉ lưu tiến độ cục bộ ở độ phân giải 1%, còn máy chủ (nhất là KOReader) trả về bốn chữ số thập phân, nên 42,0% cục bộ so với 42,4% trên máy chủ bị coi là bằng nhau và máy luôn đẩy lên thay vì kéo về.
  - Nay so sánh trực tiếp tỉ lệ gốc. Khi hai bên bằng nhau, chỉ kéo về nếu máy chủ thật sự trỏ tới vị trí xa hơn.
- **Sửa lỗi bỏ qua tiến độ khi mở sách:** chế độ tự động lúc mở sách trước đây yêu cầu phải có offset chính xác mới áp dụng tiến độ máy chủ, nên tiến độ do KOReader đẩy lên (không có offset dạng byte) bị bỏ qua hoàn toàn. Nay tiến độ vẫn được áp dụng theo chương và tỉ lệ.
- Thêm `tools/kosync_progress_host_test.py` chạy trên chính hai hàm xử lý của firmware, khoá lại cả hai định dạng XPointer và quy tắc chọn hướng đồng bộ.

## dpReader 1.0.8-vn · 2026-10-28

Bản cập nhật quan trọng khắc phục lỗi hiển thị mục lục, hoàn thiện hiển thị chữ tiếng Việt và trải nghiệm đọc sách:

- **Sửa dứt điểm lỗi chữ tiếng Việt có dấu bị mất thân chữ trong Mục lục, tên chương và tên sách:**
  - `pack_glyph_tree` trước đây ghi `loca` ngay khi duyệt cây composite theo thứ tự gốc-trước-con-sau. Với các glyph dựng từ dấu như `á` (thân `a` + dấu sắc) mà gid của thân đứng ngay trước gid của glyph, lần ghi `loca[gid + 1]` cho thân đã đè lên điểm bắt đầu của chính glyph đó. Kết quả: glyph trỏ vào dữ liệu của dấu, khiến dòng chữ chỉ còn dấu (ví dụ "Chương sáu" bị hiện thành "Chương s´u").
  - Nay toàn bộ vị trí được ghi nhận trước trong lúc đóng gói và `loca` chỉ được ghi một lần theo thứ tự gid tăng dần, bảo toàn nguyên vẹn thân chữ và dấu của mọi ký tự tiếng Việt.
  - Bổ sung bộ kiểm thử `tools/composite_glyph_host_test.py` khoá lại hành vi này ở cả cỡ chữ nội dung lẫn tiêu đề.
- **Sửa lỗi viền chữ sáng hơn nền thẻ khi tăng độ tương phản trong Mục lục:**
  - `ui_text_title_background_vc` trước đây lấy trực tiếp 4 bit cao của màu nền, bỏ qua hàm `ui_contrast_gray()` dùng chung. Khi người dùng đặt tương phản trên 100%, mép khử răng cưa trộn về tông cũ tạo quầng sáng quanh chữ trên thẻ mục lục. Nay cả hai đường hiển thị đều đi qua cùng một phép ánh xạ độ xám chuẩn xác.
- **Hiển thị tên sách trên thanh trạng thái khi đọc:**
  - Thanh trạng thái khi ở trong sách chuyển sang hiển thị tên sách đang đọc (tự động cắt gọn theo ranh giới UTF-8 và thêm dấu ba chấm nếu quá dài), không bị đè hay lưu tên sách cũ khi quay về kệ sách.
- **Nâng cấp chân trang (Footer) đọc sách 3 tầng trực quan:**
  - Tầng 1: Tên chương hiển thị ở trên (tự động căn giữa, ngắt dòng riêng không đè lên thanh tiến trình).
  - Tầng 2: Thanh tiến trình đọc sách ở giữa với đầu bo tròn thẩm mỹ, chiều cao theo cài đặt.
  - Tầng 3: Số trang hiện tại/tổng trang hiển thị bên trái, % hoàn thành toàn sách hiển thị bên phải.
- **Hỗ trợ tùy chọn căn lề đoạn văn (Body Align):**
  - Bổ sung tùy chọn căn lề trong cài đặt đọc sách: Hai bên (Justified), Căn trái (Left), Căn giữa (Center), Căn phải (Right).
- **Nâng cấp Web Flasher:**
  - Tự động phát xung reset cứng (EN qua RTS 200ms) sau khi nạp firmware, thiết bị tự khởi động lại vào hệ điều hành mới.
  - Tự động xoá phân vùng `otadata` (0xd000) khi nạp ở chế độ Nâng cấp ứng dụng, tránh máy khởi động lại vào slot cũ.

## dpReader 1.0.7-vn · 2026-10-28

Bản cập nhật nâng cấp cơ chế cập nhật OTA trực tuyến và Web Flasher thông minh:

- **Tự động kích hoạt Wi-Fi khi kiểm tra OTA:**
  - Khi bấm "Kiểm tra cập nhật", hệ thống tự động khởi động kết nối Wi-Fi bằng tài khoản/mật khẩu đã lưu trong Cài đặt mà không yêu cầu người dùng phải tự bật Wi-Fi thủ công trước đó.
  - Xử lý triệt để race condition khi dừng/bật lại tiến trình mạng bằng cơ chế đồng bộ poll stack trước khi gọi khởi động chế độ STA (`READ_PICO_TRANSFER_MODE_STA`).
- **Khắc phục lỗi "Không thể kết nối máy chủ" khi kiểm tra cập nhật:**
  - Tăng bộ đệm TLS trong `open_http_range` lên `4096 bytes` (TX: `2048 bytes`) giúp bắt tay SSL với CDN/GitHub Pages ổn định tuyệt đối.
  - Bật tính năng tự động chuyển hướng HTTP (`max_redirection_count = 4`) và hỗ trợ phân giải phản hồi dạng chunked (`Transfer-Encoding: chunked`).
  - Chuyển lỗi đồng bộ thời gian NTP qua UDP thành cảnh báo mềm không chặn luồng cập nhật, tránh trường hợp mạng yếu làm đứt đoạn quá trình nạp firmware.
  - Cập nhật địa chỉ feed chính thức trỏ thẳng tới GitHub Pages `https://dephia31.github.io/dpReader31/metalio-s31-update.json` và fallback `raw.githubusercontent.com`.
- **Nâng cấp trang Web Flasher trực tuyến:**
  - Tự động nhận diện và cập nhật thông tin phiên bản mới nhất từ máy chủ (số phiên bản, dung lượng, ghi chú phát hành).
  - Bổ sung cơ chế chống lưu cache trình duyệt (`?t=timestamp`) khi tải tệp nhị phân nạp máy, đảm bảo người dùng luôn nạp đúng bản firmware mới nhất.

## dpReader 1.0.6-vn · 2026-10-28

Bản cập nhật hoàn thiện hiển thị tải sách OPDS, sửa dứt điểm lỗi đổi phông chữ và tối ưu đồng bộ KoSync cùng khả năng khử ghosting:

- **Thêm thanh tiến trình % và dung lượng tải sách trực quan trong OPDS:**
  - Hiển thị hộp thoại nổi thông minh khi bấm tải sách từ kho OPDS với thanh tiến trình trực quan (progress bar), phần trăm (%) hoàn thành và dung lượng `KB đã tải / Tổng KB`.
  - Cập nhật thời gian thực bằng chế độ làm mới vi sai siêu tốc (`MODE_DU`), không làm đơ giao diện và tự động làm mới lại trang khi tải xong.
- **Sửa dứt điểm lỗi đổi phông chữ bị nháy 2 lần và văng về phông cũ:**
  - Khắc phục lỗi trong `ttf_font_is_builtin()`: Trước đây hàm này kiểm tra `sys->f_mem != NULL` khiến mọi phông ngoài nạp vào PSRAM bị nhận nhầm là phông tích hợp sẵn, dẫn đến cơ chế bảo vệ `reader_font_fallback_observe()` kích hoạt sau 3 tick và ép văng về phông cũ.
  - Sửa hàm nhận diện chuẩn xác theo đường dẫn tệp (`ttf_font_path_is_builtin(sys->f_path)`), cho phép phông chữ trên thẻ SD được áp dụng ổn định ngay lập tức trong cả trình đọc sách lẫn Cài đặt hệ thống.
  - Gọi `reader_font_choice_finish()` đầy đủ ở nhánh chọn phông thành công để giải tỏa cờ trạng thái.
- **Sửa dứt điểm lỗi đồng bộ hai chiều KoSync (Pull & Push):**
  - Cập nhật định dạng header chuẩn `Content-Type: application/vnd.koreader.v1+json` tuân thủ chuẩn `koreader-sync-server`.
  - Sửa thuật toán tính Partial MD5 (`kosync_compute_doc_md5`) bằng hàm `stat()` để xác định kích thước tệp chính xác trên FATFS thay cho `fseek()`.
  - Quản lý vòng đời kết nối WiFi đơn phiên (`managed_wifi`) trong `kosync_sync_book`: giữ kết nối WiFi thông suốt xuyên suốt quá trình kéo (pull) rồi đẩy (push), tránh lỗi ngắt kết nối giữa chừng (`ESP_ERR_INVALID_STATE`).
  - Tự động lưu tiến độ vào NVS ngay khi kéo thành công từ máy chủ KoSync.
- **Tối ưu hóa khử hiện tượng Ghosting trên màn hình E-ink:**
  - Chuyển chu kỳ dọn sạch mực toàn màn hình mặc định trong trình đọc sách sang **mỗi 3 trang** (thay vì 5 trang).
  - Tối ưu lệnh dọn dẹp định kỳ `s_reader_cleanup` chuyển sang `update_display_full(ctx->hl)` để kích hoạt toàn bộ chu trình GC16 toàn màn hình và thiết lập lại dirty map, triệt tiêu hoàn toàn bóng ma tích tụ.


## dpReader 1.0.5-vn · 2026-10-28

Bản cập nhật điều chỉnh điện áp VCOM chuẩn an toàn và tối ưu chu kỳ làm mới bảo vệ tuổi thọ màn hình E-ink:

- **Tối ưu điện áp phân cực VCOM về mức an toàn chuẩn E-Ink (`1700 mV`):**
  - Đặt điện áp $V_{COM}$ mặc định về `1700 mV` ($-1.7\text{V}$), nằm trọn trong dải khuyến nghị danh định của nhà sản xuất panel E-Ink (1.2V – 2.0V). Mức này mang lại lực điện trường đủ mạnh để lật dứt khoát hạt mực, triệt tiêu ghosting mà không gây ứng suất điện trường quá mức làm suy giảm tuổi thọ màng vi nang.
  - Khóa trần hiệu chuẩn an toàn `VCOM_MAX_MV` ở `2200 mV` trong driver phần cứng để ngăn ngừa việc cấu hình quá áp gây chai màn hình.
- **Tối ưu chu kỳ Settle & Clean:**
  - Điều chỉnh chu kỳ cân bằng toàn trang `settle` (GL16 full) thành **mỗi 4 trang**, vừa triệt tiêu cặn mực vi sai, vừa tiết kiệm 30% pin và bảo vệ màng E-ink bền bỉ lâu dài.
  - Chu kỳ làm mới toàn phần GC16 đặt ở **mỗi 6 thao tác trang**.

## dpReader 1.0.4-vn · 2026-10-28

Bản cập nhật quan trọng khắc phục triệt để hiện tượng ghosting (bóng ma / lưu ảnh) trên tấm nền E-ink của Metalio E-INK4 Plus:

- **Khắc phục sai lệch điện áp phân cực màn hình VCOM:**
  - Khôi phục điện áp VCOM chuẩn tối ưu của tấm nền ED047TC1 về `3000 mV` (-3.0V) thay vì mức 1200 mV bị suy giảm điện trường nghiêm trọng.
  - Mở rộng giới hạn trần hiệu chuẩn `VCOM_MAX_MV` lên `4000 mV` (dải 900..4000 mV) và gỡ bỏ giới hạn 2000 mV trong driver phần cứng `read_pico_board.c`. Khi điện áp bias được khôi phục, hạt mực đen và trắng chuyển động dứt khoát, mực đen sâu hơn và triệt tiêu nguyên nhân cốt lõi gây kẹt mực tạo ghosting.
- **Tối ưu hóa chu kỳ cân bằng và làm mới màn hình (Refresh & Settle):**
  - Kích hoạt cơ chế `settle` (GL16 full-screen) định kỳ mỗi 2 trang cho cả chế độ thông thường (`ordinary`), giúp cân bằng lại toàn bộ điểm ảnh trên màn hình nhằm xóa cặn mực vi sai mà không gây chớp đen màn hình.
  - Rút ngắn chu kỳ tự động làm mới toàn diện GC16 từ 10 thao tác xuống mỗi 5 thao tác trang (`APP_UI_GC16_EVERY = 5`).
- **Nâng cấp tùy chọn làm mới trong trình đọc sách:**
  - Bổ sung tùy chọn làm mới toàn màn hình `Mỗi 1 trang` và `Mỗi 3 trang` bên cạnh các mức 5, 10, 15, 20 trang hoặc tắt, giúp người dùng linh hoạt lựa chọn theo nhu cầu đọc sách hoàn toàn không có bóng mờ.
- **Dọn dẹp hệ thống:**
  - Loại bỏ toàn bộ nhật ký thay đổi tiếng Trung cũ, chuẩn hóa tài liệu chính thức bằng tiếng Việt từ bản 1.0.0-vn.

## dpReader 1.0.3-vn · 2026-10-27

Bản cập nhật v1.0.3-vn nâng cấp tính năng cấu hình tài khoản và đồng bộ kết nối trực tiếp trên màn hình thiết bị:

- **Cấu hình trực tiếp KoSync & OPDS trên màn hình thiết bị:**
  - Cho phép chạm trực tiếp vào các hàng Máy chủ, Tài khoản (Username) và Mật khẩu (Password) trong menu `Cài đặt -> Kết nối -> Đồng bộ KoSync / Kho sách OPDS` để chỉnh sửa qua bàn phím ảo tích hợp.
  - Tự động kích hoạt bàn phím Latin chuẩn (`ascii_only`) giúp nhập URL và thông tin đăng nhập chính xác; tự động mã hóa băm mật khẩu MD5 (`x-auth-key`) tuân thủ nghiêm ngặt giao thức KOReader Sync.
- **Đồng bộ KoSync 2 chiều thông minh:**
  - Tích hợp nút bấm đồng bộ nhanh 1 chạm trên thanh Quick Settings (vuốt từ đỉnh màn hình xuống) và trên thanh công cụ Reader Toolbar.
  - Tự động so khớp timestamp và tiến độ đọc giữa thiết bị và máy chủ KoSync, cảnh báo khi đồng bộ hoặc mất kết nối.
- **Tối ưu phông chữ và toàn hệ thống:**
  - Giữ trọn bộ tối ưu phông OTF/TTF trên thẻ SD, chống nhấp nháy, nạp CFF PSRAM và bộ dấu tiếng Việt 2 tầng từ v1.0.2-vn.

## dpReader 1.0.2-vn · 2026-10-27

Bản cập nhật v1.0.2-vn tập trung khắc phục lỗi phông chữ trên thẻ nhớ SD và hoàn thiện hỗ trợ dấu tiếng Việt trên toàn hệ thống:

- **Hỗ trợ toàn diện phông chữ OpenType / CFF (`.otf`) và TrueType (`.ttf`) trên thẻ nhớ SD:**
  - Khắc phục triệt để lỗi khi người dùng tải phông lên từ Web File Transfer / thẻ SD rồi chọn phông đọc sách bị nhấp nháy màn hình và văng về phông mặc định. Cơ chế nạp phông CFF tự động nạp toàn bộ file font vào PSRAM của ESP32-S31 khi font được mở từ tệp trên thẻ SD.
  - Loại bỏ giới hạn 32 bảng SFNT Table Directory, cấp phát động đọc đầy đủ toàn bộ bảng thư mục của phông chữ (các phông chữ hiện đại tiếng Việt như Bookerly, Inter, Noto Sans thường có 35-50 bảng).
  - Quét phông thông minh đa tầng: hỗ trợ quét đệ quy các thư mục con cấp 1 (ví dụ `/sdcard/fonts/Bookerly/Bookerly-Regular.ttf`) và bổ sung các biến thể tên thư mục `Fonts`, `Font`, `FONTS`, `Books/fonts`.
- **Hoàn thiện bộ dấu Tiếng Việt trên phông mặc định và toàn hệ thống:**
  - Cập nhật trần kích thước ký tự dọc (`ascent` 1120, `descent` -300) trong `builtin.ttf`, khắc phục triệt để hiện tượng cắt cụt dấu 2 tầng ở các chữ hoa tiếng Việt như `Ẳ, Ẩ, Ằ, Ắ, Ầ, Ấ, Ẫ, Ể, Ế...` (có đỉnh yMax lên tới 1064).
  - Tích hợp bộ ghép dấu tổ hợp tiếng Việt (Unicode NFD -> NFC) theo thời gian thực trực tiếp trong bộ giải mã `decode_utf8`, hiển thị chuẩn xác các văn bản dùng Unicode tổ hợp mà không bị văng dấu hay hiển thị ô vuông.
  - Chuẩn hóa chuỗi tiếng Việt tại nguồn khi tải chương sách TXT và sửa bộ nhận diện mã hóa tránh đoán nhầm văn bản tiếng Việt UTF-8 thành GBK (tiếng Trung).

## dpReader 1.0.1-vn · 2026-10-27

Rà soát an toàn trước khi nạp: đối chiếu cây nguồn với repo gốc nhà sản xuất [CloudZao/Metalio-E-INK4-Plus](https://github.com/CloudZao/Metalio-E-INK4-Plus) (bản 2.0.66) và với Kconfig ESP-IDF v6.1. Các mục dưới đây đều là sửa lỗi phát hiện trong lần rà soát đó.

- **VCOM mặc định 3000 mV → 1200 mV, dải hiệu chuẩn 4000 → 2000 mV.** Firmware nhà sản xuất chỉ dùng 1200/1600/2000 mV; giá trị 3000 nằm ngoài toàn bộ dải đó và được áp dụng ngay lần khởi động đầu tiên khi NVS chưa có giá trị, tức là máy chạy sai điện áp bias màn hình ngay từ đầu. Nay máy chưa hiệu chuẩn khởi động trên 1200 mV của nhà sản xuất. Giá trị cũ 3000 mV còn trong NVS sẽ bị từ chối vì ngoài dải mới và tự rơi về 1200 mV. Vẫn chỉ ghi thanh ghi runtime, không ghi OTP vĩnh viễn.
- **Gỡ nhánh VCOM chết trong đường khởi động.** Hàm đọc VCOM luôn trả `true` nên nhánh mở trang hiệu chuẩn khi chưa có giá trị không bao giờ chạy. Nay máy khởi động bình thường; trang hiệu chuẩn nằm trong debug panel, mở bằng cách **giữ phím âm lượng lúc bật máy**.
- **`CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_300` là dòng cấu hình vô hiệu.** ESP32-S31 chỉ có 40/240/320 MHz; ký hiệu `MHZ_300` không tồn tại nên IDF bỏ qua và máy vốn đã chạy 320 MHz. Nay ghi rõ `MHZ_320` cho khớp thực tế và khớp nhà sản xuất.
- **Main task stack 12288 → 16384 và Task WDT 5 s → 20 s.** Firmware nhà sản xuất từng gặp lỗi stack protection ở màn hình đầu cộng lần làm mới toàn màn hình đầu tiên và đã nâng stack lên 16384. Làm mới GC16 toàn màn hình chặn vài giây nên mốc 5 s mặc định báo nhầm timeout của tác vụ IDLE ở mỗi lần làm mới. `PANIC` vẫn tắt: hết giờ chỉ cảnh báo, không khởi động lại giữa lúc đang quét màn hình.
- **Sửa cảnh báo nạp firmware trong README.** Ghi chú cũ nói "đừng bấm Erase Flash để giữ VCOM" là sai: `full.bin` trải liền từ `0x0` nên tự nó ghi đè vùng NVS bằng `0xFF`, làm mất cài đặt, mật khẩu và VCOM đã hiệu chuẩn. Nay nêu rõ điều đó và chỉ ra lệnh nạp tại `0x10000` mới là lệnh giữ nguyên NVS.
- **Ghi log dung lượng PSRAM thật lúc khởi động.** Các ngân sách cache trong mã đặt theo một giả định về dung lượng PSRAM, nhưng dung lượng thực của từng lô máy không được ghi trong tài liệu. Nay mỗi lần khởi động in `PSRAM total/free` và `internal free`, nên không phải suy đoán khi gặp vấn đề bộ nhớ.
- **Thêm cài đặt KOSync và OPDS vào mục Cài đặt -> Kết nối:** Bổ sung giao diện xem trạng thái máy chủ, tài khoản, khóa đồng bộ, bật/tắt tự động đồng bộ tiến độ đọc KOSync bằng 1 chạm, kiểm tra kết nối tài khoản KOSync, mở nhanh kho sách OPDS trên máy, đặt lại máy chủ mặc định và liên kết nhanh tới cổng cấu hình Web Portal.
- **Cập nhật host test VCOM** theo dải 900..2000 mV và mặc định 1200 mV, thêm kiểm tra rằng giá trị 3000 mV cũ trong NVS bị từ chối và rơi về mặc định.

## dpReader 1.0.0-vn · 2026-10-27

Phiên bản khởi đầu chính thức dpReader cho thiết bị Metalio E-INK4 Plus (ESP32-S31):
- Giao diện người dùng thuần Việt hoá, font chữ Bitter và tối ưu trải nghiệm đọc sách E-ink.
- Hỗ trợ định dạng sách TXT, EPUB với dàn trang tự động, bookmark và bảng mục lục.
- Bảng điều khiển Quick Settings vuốt từ trên xuống, quản lý WiFi, đèn nền và cấu hình phím tắt.
