# dpReader 1.6.8 · Metalio 4.7 ESP32-S31

Full và Love Book · Full and Love Book

- Mở rộng Cài đặt nhanh lên 10 nút (Xoay màn hình, Khóa ngay, Kiểu lật trang, Rung, Khóa cảm ứng, Đèn nhanh), làm nổi bật 3 mức đèn nhanh và cho phép đổi tên 2 nhãn Tùy chỉnh (hỗ trợ UTF-8).
- Dùng thuật toán dựng chữ 4 mức 22 pha + Core 1 PIE SIMD cho cả sách và giao diện, kèm tùy chọn `dpMetal White` trong trình đọc.
- Sửa đánh số mục lục cha-con, tăng tốc mở/đóng sách và lật trang, đồng thời tối ưu pin (tự tắt BLE khi rảnh, giữ idle khi bật Khóa cảm ứng).

**English**

- Expand Quick Settings to 10 buttons (Rotate screen, Instant lock, Page-turn mode, Haptics, Touch lock, Light presets), highlight Quick Light Presets, and support editable UTF-8 custom labels.
- Use 4-level 22-phase + Core 1 PIE SIMD text and UI rendering, with an optional `dpMetal White` mode in the reader.
- Fix nested TOC chapter numbering, speed up book open/close and page turns, and optimize battery life (BLE idle auto-off and Touch Lock idle preservation).

Firmware source commit: `04564c6ba75481d7f55637a753b9a51f0ee5367c`
