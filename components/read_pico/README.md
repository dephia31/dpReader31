# BSP Read Pico

Component `mindreset/read_pico` hỗ trợ bo Read Pico dùng ESP32-S3 và tấm nền đơn sắc 4,7 inch. BSP quản lý I²C chung, cấu hình epdiy, nguồn màn hình qua SY7636A và FCA9555, thẻ TF, còi và chế độ flash hiệu năng cao.

## API và phần cứng

- `read_pico_init()` khởi tạo màn hình, dò thẻ, kiểm tra rồi tắt gia tốc kế, khởi động giao tiếp PMU và cảm ứng. Lỗi ngoại vi xóa cờ `*_ready` tương ứng. Lỗi màn hình làm khởi tạo thất bại.
- `read_pico_get_status()` và `read_pico_get_ioe_status()` chụp trạng thái chân mở rộng, nguồn và thanh ghi SY7636A để dùng trong trang chẩn đoán.
- `read_pico_rails_on()`, `read_pico_toggle_vcomctl()` và callback của epdiy điều khiển nguồn.
- Gia tốc kế ánh xạ trục chip sang trục thiết bị. Màn hình hướng lên là +Z.
- `read_pico_sd_*` cung cấp dò, mount, định dạng và đọc dung lượng thẻ theo luồng không chặn.
- `read_pico_buzzer_tone()` phát âm bằng LEDC và chặn trong thời lượng được truyền vào.
- `read_pico_flash_hpm.c` cấu hình thanh ghi flash Zbit (`0x5E`) để chạy 120 MHz. Bảng IDF gốc chỉ chứa các hãng `0xC8` và `0x20`.

Trong `idf.py menuconfig` → **Read Pico board configuration**, có thể chọn cổng, chân và tốc độ I²C cùng chân ngắt FCA9555/CST836U. Chân màn hình, thẻ và còi nằm trong `read_pico_board.c`, `read_pico_sd.c` và `read_pico_buzzer.c`.

Phụ thuộc gồm `epdiy`, `sy7636a`, `fca9555`, `cst836u`, `sc7a20h`, `read_pico_pmu` và `e0470_epaper_waveform`.

Giấy phép Apache-2.0. Xem [LICENSE](LICENSE).
