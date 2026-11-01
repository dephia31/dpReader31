# Đồng bộ vị trí KoSync

KoSync EPUB của dpReader dùng định vị nội dung trong chương, không dùng số trang hoặc phần trăm nguyên làm vị trí chính. Cần cùng nội dung EPUB trên hai thiết bị và cùng cách nhận diện tài liệu bằng partial MD5. Sách đã chỉnh sửa hoặc tối ưu lại có thể mang mã tài liệu khác.

## Vị trí chính xác và vị trí dự phòng

- Khi cấu trúc XHTML và cách chuẩn hóa văn bản xác định được, dpReader chuyển vị trí UTF-8 của trình đọc thành XPointer theo phần tử, nút văn bản và vị trí ký tự Unicode (đơn vị `lString32` của CRE, không phải byte UTF-8 hay surrogate UTF-16). Chiều kéo thực hiện phép đổi ngược; không mặc định quay về đầu chương.
- Tỷ lệ gửi lên máy chủ giữ phần lẻ. Số phần trăm nguyên trên giao diện chỉ là cách hiển thị; phần trăm và số trang giữa các bộ máy dàn trang có thể khác dù cùng một đoạn văn.
- Khi không xác nhận được vị trí ký tự do cấu trúc/khoảng trắng đặc biệt, chiều đẩy có thể dùng phần tử chứa đoạn đó và báo **ước lượng theo đoạn**. Nếu ngay cả phần tử cũng không xác định an toàn, firmware báo lỗi và không ghi đè tiến độ trên máy chủ.
- Vị trí kéo không giải được thành XPointer được ước lượng bằng tỷ lệ và độ dài các chương, không nhân tỷ lệ với số chương. Đồng bộ thủ công báo vị trí ước lượng; tự động kéo khi mở sách chỉ áp dụng vị trí đã giải được.
- Vị trí cũ giữa các máy dpReader vẫn đọc được khi có dấu nhận diện thiết bị và chuỗi byte offset hợp lệ. TXT giữ cơ chế vị trí riêng của dpReader; không coi đó là định vị chính xác qua CRE trên thiết bị khác.

Định vị nội dung không đảm bảo hai màn hình có cùng số trang: kích thước màn hình, font và dàn trang khác nhau vẫn có thể làm vị trí đó nằm ở những trang khác nhau. Khả năng tương thích cuối cùng còn phụ thuộc cấu trúc DOM và phiên bản bộ đọc ở thiết bị nhận.

## Chọn bản tiến độ mới hơn

Firmware ghi thời điểm thay đổi vị trí đọc thật, tách khỏi số thứ tự lịch sử đọc `last_open_s`. Khi hai thời điểm UTC hợp lệ và khác nhau, bản mới hơn được ưu tiên, kể cả khi người dùng vừa đọc lùi. Nếu đồng hồ không đáng tin, thời điểm bằng nhau hoặc không có thời điểm, firmware so sánh vị trí nội dung; không suy đoán đọc lùi bằng phần trăm làm tròn.

Lật trang chỉ cập nhật trạng thái thời gian trong RAM. Trạng thái được lưu có kiểm tra CRC vào tệp phụ trên SD tại `/.readpico/kosync/` ở các điểm đồng bộ/đóng sách/khóa màn hình. Không thêm bản ghi vào NVS thiết lập và không phát sinh lần ghi phụ cho mỗi trang. Mở lại sách không tự đổi thời điểm đọc thành thời điểm mở ứng dụng. Kết quả tự đồng bộ bị bỏ nếu người dùng đã chuyển sang vị trí khác trong lúc chờ mạng.

Phản hồi sai tài liệu, tỷ lệ ngoài 0–1, chuỗi quá dài hoặc lỗi máy chủ không được coi là “chưa có tiến độ” để đẩy đè dữ liệu đang lưu. Đồng hồ chưa đồng bộ không tạo dấu thời gian UTC giả.

## Bộ nhớ và phạm vi xử lý

Chuyển đổi sử dụng một phiên đọc EPUB độc lập, chỉ mục sẵn có và nội dung chương cần định vị; không gọi lại bộ font đang được trình đọc sử dụng. Không giải nén toàn bộ các chương mỗi lần đồng bộ. Bộ ánh xạ có giới hạn độ sâu 48, 24 loại phần tử cùng cấp và chuỗi XPointer tối đa 767 byte; vùng làm việc và chương nằm trong PSRAM. Trường hợp vượt giới hạn hoặc cấu trúc chưa hỗ trợ được từ chối hoặc báo ước lượng, không âm thầm gắn vị trí vào chương đầu.

Bộ kiểm thử có thể chạy lại:

```sh
python3 tools/book_sync_position_host_test.py
python3 tools/kosync_wire_host_test.py
python3 tools/kosync_state_host_test.py
python3 tools/kosync_progress_host_test.py
python3 tools/kosync_auto_host_test.py
```

Kiểm thử JSON cần cJSON: dùng `CJSON_DIR` trỏ tới thư mục chứa `cJSON.c`/`cJSON.h`, phụ thuộc đã tải của bản build S31, hoặc gói phát triển `libcjson` của hệ thống. Các bài kiểm thử không gửi dữ liệu lên tài khoản KoSync thật. Việc nghiệm thu trao đổi giữa hai thiết bị vẫn cần dùng đúng cặp bộ đọc và cùng tệp sách.
