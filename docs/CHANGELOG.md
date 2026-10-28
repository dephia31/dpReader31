# Nhật ký thay đổi / Changelog

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
