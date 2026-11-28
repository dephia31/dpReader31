# dpReader 1.7.3 · Metalio 4.7 ESP32-S31

Full và Love Book · Full and Love Book

- Nâng cấp màn hình khóa **Ghép bìa Tủ sách** (bản Full) với 3 bố cục C1, C3 và C4 (dải tối bán trong suốt, bỏ khung Polaroid), chọn từ tối đa 48 sách với 3 mật độ bìa và 3 chế độ sắp xếp (Gần đây, Ngẫu nhiên mỗi lần khóa, Ngẫu nhiên cố định), kèm nút **Xáo lại** có hiệu ứng phản hồi khi chạm và đếm `#N`.
- Thêm màn hình khóa **Bìa sách đang đọc** cho cả hai bản Full và Love Book với 2 kiểu: B1 (thẻ nổi) và B3 (dải thông tin tối bán trong suốt phía trên dải chữ ký đen đặc sát đáy).
- Bổ sung 2 lựa chọn hiệu ứng lật trang sóng nước (**Sóng nước nhanh (22 pha)** và **Sóng nước mượt (37 pha)**), thêm chế độ **`dpMetal Dark`** trong **dpMetal Core**, và nâng độ đen sâu cho chữ trên các dạng sóng GC48, GC36, GL37 và 22 pha để không bị bạc màu ở cuối lượt quét.
- Đặt dữ liệu ngắt I2C, cảm ứng, bộ mở rộng I/O và gia tốc kế vào SRAM nội bộ cùng `CONFIG_I2C_ISR_IRAM_SAFE=y`; bổ sung tự động lưu tiến độ, đồng bộ thẻ SD và tắt nguồn an toàn khi pin cạn; đồng bộ đèn nền sáng dần khi thức từ Light Sleep / Deep Sleep và tắt sạch I/O, rung, xung màn hình khi ngủ sâu.

**English**

- Upgrade the **Cover-collage lock screen** (Full) with C1, C3, and C4 layouts (translucent dark band without Polaroid frame), up to 48-book selection, 3 cover densities, Recent / Random per lock / Fixed random order, and a **Shuffle** button with tap highlight and `#N` counter.
- Add the **Current book cover** lock screen (Full & Love Book) with B1 floating card and B3 translucent info band above a solid black signature strip.
- Add dual water-ripple page-turn options (**Fast 22-phase** and **Smooth 37-phase**), **`dpMetal Dark`** mode under **dpMetal Core**, and deeper black text contrast across GC48, GC36, GL37, and 22-phase waveforms.
- Place I2C/touch/I/O-expander/accelerometer structures in internal SRAM with `CONFIG_I2C_ISR_IRAM_SAFE=y`, add critical-battery safe shutdown saving progress and syncing SD, unify soft-start frontlight wake across Light and Deep Sleep, and cleanly tear down I/O, haptics, and display clocks for Deep Sleep.

Firmware source commit: `957d9b30cd58d2d0fd2420d8d1d31906ada89e72`
