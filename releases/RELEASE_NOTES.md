# dpReader 1.6.9 · Metalio 4.7 ESP32-S31

Full và Love Book · Full and Love Book

- Ẩn thanh trạng thái trên Tủ sách, Tệp và Cài đặt; mở rộng vùng hiển thị, cân đối tab Tủ sách, hạ hàng phân trang (`Tủ sách`, `Tệp`, `OPDS`), chống tràn chữ và làm đậm viền đen khung thẻ.
- Căn trái lời chào trên New Home và thêm mục **Tùy chỉnh giao diện** (Full & Love Book) cho phép ẩn/hiện tên sách & tác giả, thanh tiến độ, nút Đọc tiếp và tự phóng to bìa sách theo phần trống.
- Đèn nền sáng sớm an toàn khi thức từ Light Sleep và tăng sáng mềm khi thức từ Deep Sleep; nâng sóng chữ 22 pha lên 16 mức xám, tăng độ đậm GC36 Flash, sửa cắt dấu dòng đầu khi lật trang dọc và không lưu hộp hoàn tác nhảy chương vào cache.
- Mở rộng 3 mức đèn tùy chỉnh, sửa cỡ chữ Cài đặt nhanh, giữ xung APB USB khi Light Sleep, đọc trực tiếp VBUS bộ sạc khi tắt nguồn hẹn giờ, cho phép hạ xung CPU khi giữ kết nối BLE/mở Quick Setting, và tối ưu BLE/OPDS/KOSync.

**English**

- Hide the status bar on Library, Files, and Settings; expand usable space, balance Library tabs, lower the pager bar (`Library`, `Files`, `OPDS`), bound menu text, and use crisp black card borders.
- Left-align the New Home greeting and add **Home customization** (Full & Love Book) toggles for book title/author, reading progress, and Continue button with automatic reflow and proportional cover scaling.
- Safe early wake frontlight for Light Sleep and soft-start ramp on Deep Sleep wake; upgrade 22-phase text rendering to 16 gray levels, boost GC36 Flash black pulses, fix 4 px top overhang on portrait Normal turns, and keep chapter-jump undo overlays out of page cache.
- Add a 3rd custom light preset, fix Quick Settings font sizing, retain USJ APB clock during Light Sleep, read charger VBUS directly for timed Deep power-off, allow CPU downclock during idle BLE / Quick Settings, and fix BLE/OPDS/KOSync handling.

Firmware source commit: `622ee2a8f1b7bb0168905e11179c648294172eb0`
