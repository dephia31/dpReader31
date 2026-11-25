# dpReader 1.6.5 · Metalio 4.7 ESP32-S31

Full và Love Book · Full and Love Book


- Tối ưu ngủ và tiết kiệm pin: bật Tickless Idle, cắt xung màn hình giữa các khung hình, tắt trace và heap poisoning sản phẩm, giãn nhịp đọc pin và kiểm tra thẻ SD khi rảnh, dùng `WIFI_PS_MIN_MODEM` lúc Wi-Fi truyền tệp chờ, cho BLE nghỉ khi remote vắng mặt, và giữ `P1.4/VOUT` qua Deep Sleep để loại bỏ reset sụt áp.
- Nâng cấp bộ lọc cảm ứng `FT6336U` xác nhận đối xứng `>= 12 ms` và chống dội `45 ms` không khóa tay, hạn chế lật 2 trang ngoài ý muốn mà vẫn giữ chạm đúp (double-tap), cùng thời gian ổn định reset cảm ứng `300 ms`.
- Chỉ tính phím Nguồn khi bật "Nhấn đúp nguồn mở khóa", khôi phục thẳng sách hoặc tab đang mở sau khi tắt nguồn theo lịch (`timed_off`), giữ mặc định tắt nguồn sau Deep Sleep là tắt (`false`), và mặc định làm sạch toàn trang mỗi 10 lần lật.
- Đồng nhất tên sách và tên tệp tiếng Việt (chuẩn hóa NFC và hỗ trợ 32 tổ hợp ngược dấu trên Trang chủ, Tủ sách, Chi tiết sách, mục lục TXT/EPUB, OPDS, Quản lý tệp, truyền Wi-Fi và Tìm kiếm); mở rộng giới hạn tiêu đề chương TXT lên 96 byte và bảo vệ tệp sách cũ khi ghi đè qua OPDS.

**English**

- Optimize sleep, idle and battery life: enable Tickless Idle, gate active display clocks between frames, disable product trace/heap poisoning, space out idle battery and SD polling, use `WIFI_PS_MIN_MODEM` while HTTP transfer is idle, park absent BLE remotes cleanly, and retain `P1.4/VOUT` across Deep Sleep to prevent brownout resets.
- Add symmetric `>= 12 ms` press/release confirmation and non-starving `45 ms` bounce suppression in `FT6336U` to prevent unintended double page turns while keeping deliberate double-taps, retaining the 300 ms touch reset settle delay.
- Restrict double-press unlock to the power button, resume straight into the prior book/tab after scheduled timed power-off, keep timed Deep Sleep shutdown user-controlled (`false` default), and set the default full-refresh cadence to 10 turns.
- Fix Vietnamese text normalization (NFC + 32 reversed combining marks) and layout across Home, Library, Book details, TXT/EPUB TOC, OPDS, File Manager, Wi-Fi upload and search; raise the TXT chapter title limit to 96 bytes and protect existing books on OPDS replacement.
