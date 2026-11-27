# dpReader 1.7.0 · Metalio 4.7 ESP32-S31

Full và Love Book · Full and Love Book

- Sửa lỗi nháy trắng toàn màn hình khi bấm **Làm mới (Refresh)** trong chế độ chữ `dpMetal Core` (tắt `dpMetal Bold`), đưa về lượt quét sạch GC48 trực tiếp không ảnh hưởng mục Sửa màn hình trong Cài đặt.
- Sửa lỗi đọc/ghi tràn chuỗi trên tiêu đề danh sách sách, chân trang trình đọc, ghi chú cập nhật và `cache_read_text` EPUB; xóa tệp đích trước khi đổi tên ảnh bìa và thông báo giấy phép font trên thẻ SD; dọn báo cáo crash 0 byte để tránh lặp thử lại.
- Tối ưu tiết kiệm pin và ổn định nền: dừng đánh thức CPU 750 ms khi không cần tải lại font, không giữ xung CPU / chặn BLE khi Wi-Fi truyền tệp lỗi và tự tắt Wi-Fi lỗi khi rời màn Truyền tệp; khôi phục rung nếu tắt nguồn sớm bị hủy; đặt lại trạng thái BLE khi ngắt kết nối; chuyển hướng OPDS cùng nguồn an toàn (chặn hạ cấp HTTPS→HTTP và rò rỉ xác thực).
- Đồng nhất vùng chạm nút Quay lại trên các trang con của Cài đặt Metalio 4.7 và hoàn thiện bản dịch tiếng Anh cho các màn hình Cài đặt, bàn phím, font trực tuyến và trạng thái đọc xong.

**English**

- Fix full-screen white flash on manual Refresh in `dpMetal Core` mode with `dpMetal Bold` off by routing directly to single-pass GC48 refresh without affecting Settings Panel Repair.
- Fix string/UTF-8 bounds on book list titles, reader footer, update notes, and EPUB `cache_read_text`; unlink FATFS targets before renaming covers and font license notices; clean 0-byte crash reports.
- Save battery on idle font/failed Wi-Fi transfer retry, restore haptics if early deep poweroff aborts, reset BLE state on disconnect, and harden same-origin OPDS redirects (block HTTPS→HTTP downgrade and auth leakage).
- Standardize Metalio 4.7 Settings Back hitboxes and complete English translations for Settings, keyboard, online fonts, and finished reading status.

Firmware source commit: `cbbe3dfb70adec8ef5376a51c10a9cb3aa7b9a55`
