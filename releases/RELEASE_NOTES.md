# dpReader 1.6.4 · Metalio 4.7 ESP32-S31

Full và Love Book · Full and Love Book


- Sửa trường hợp CPU giữ xung cao sau khi máy khởi động lại bất thường. Trang đọc và Trang chủ đã ổn định chờ thao tác hoặc đến hạn cần xử lý.
- Cho CPU hạ xung giữa các lần Bluetooth thử kết nối lại với nút lật trang. Tính thời gian đọc khi có thao tác và khi thoát hoặc khóa máy.
- Giữ thẻ đã tháo ở trạng thái nghỉ cho đến khi mở khóa. Thử lại một lần nếu phần cứng tạm thời từ chối Deep Sleep.
- Giảm số lần CPU thức ở Cài đặt, Tập tin và Tủ Sách khi màn hình đứng yên.
- Giảm công việc sao lưu lúc khóa nếu dữ liệu chưa đổi. Giãn thử lại khi màn khóa gặp lỗi và ghi thời gian máy thực sự ngủ.
- Rút ngắn khôi phục cài đặt bằng cách bỏ lượt đọc lịch sử trùng và chỉ nạp phông hệ thống cần dùng.
- Trang tĩnh chờ đúng hạn thử lại khi thẻ SD bị rút hoặc liên tục không gắn được.
- Gốc Tập tin cập nhật dung lượng khi vào màn hoặc chạm Làm mới, rồi chờ kết quả từ thẻ.
- Khởi động với trần CPU 320 MHz kể cả khi đã lưu chế độ 160 hoặc 240 MHz ở bản thử trước.

**English**

- Restore normal CPU scaling after abnormal resets. Settled reader and Home screens wait for input or pending deadlines.
- Allow lower CPU clock between Bluetooth reconnect attempts. Account for reading time at input, exit, and lock boundaries.
- Keep parked SD media suspended until unlock. Retry once when hardware temporarily rejects Deep Sleep.
- Reduce CPU wakeups on settled Settings, Files and Bookshelf screens.
- Skip redundant backup preparation at lock. Space out lock recovery attempts and record actual sleep time.
- Restore settings faster by avoiding a duplicate history pass and loading only the selected system font.
- Let static screens wait until the next SD retry after removal or repeated mount failure.
- Refresh storage usage when entering Files or tapping Refresh, then wait for the SD result.
- Boot with a 320 MHz CPU ceiling even if a previous test build saved a lower limit.
