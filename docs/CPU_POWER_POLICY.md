# Chính sách xung CPU S31: 320 MHz hoạt động, cho phép 160 MHz khi rảnh

Từ bản thử `1.2.9RC22-vn`, S31 bật `CONFIG_PM_ENABLE` và cấu hình SDK PM với
`max_freq_mhz=320`, `min_freq_mhz=160`, `light_sleep_enable=false`.
`CONFIG_PM_DFS_INIT_AUTO` tắt để SDK không tự xuống XTAL trước khi app lấy
khóa UI; build driver cũng loại cấu hình auto-DFS cũ khỏi cache.
Đây là chính sách thử cục bộ đã build và kiểm tra host, chưa nghiệm thu phần
cứng và chưa có số đo pin/độ trễ để kết luận 160 MHz là mức tối ưu.

## Phạm vi và nguyên tắc

CPU giữ yêu cầu mức tối đa trong toàn bộ xử lý UI, mở sách, layout, raster,
quét màn hình và các ngữ cảnh không đủ điều kiện đọc tĩnh. Chỉ gỡ yêu cầu này
ngay trước một lượt chờ của trang đọc đã sẵn sàng theo
[UI_EVENT_DEADLINE.md](UI_EVENT_DEADLINE.md).

Điều kiện đọc tĩnh vẫn gồm: WiFi dừng, Bluetooth tắt, KoSync tự động tắt,
cache liền kề đủ, không có thao tác/công việc chờ, trang đã hiển thị thành công
và ít nhất 400 ms yên sau input. Lượt chờ phải lớn hơn 40 ms; trần deadline
hiện là 200 ms. Không hạ xung trong lúc đang lật trang để thử tiết kiệm điện.

Gỡ khóa chỉ **cho phép SDK chọn mức thấp**, không chứng minh CPU đã chạy
160 MHz suốt lượt chờ. Scheduler và các khóa PM khác vẫn có thể giữ 320 MHz.
SDK cũng có khóa CPU_MAX của RTOS khi task hoạt động. Tickless idle và tự
ngủ nhẹ chưa được bật; tick RTOS vẫn 1000 Hz nên chi phí thức/chuyển xung còn
cần đo, không suy ra phần trăm tiết kiệm từ tỷ lệ 160/320.

## Thành phần

- [metalio_cpu.h](../components/metalio_hw/include/metalio_cpu.h) và
  [metalio_cpu.c](../components/metalio_hw/metalio_cpu.c): quản lý khóa, điều kiện
  cho phép idle, guard giao dịch, lỗi và bộ đếm.
- [app_loop.c](../main/app/app_loop.c): khởi tạo trước vòng UI, chỉ mở cửa sổ
  idle sau planner, lấy lại khóa ngay khi lượt chờ kết thúc.
- [metalio_ui_wake.c](../components/metalio_hw/metalio_ui_wake.c): giữ bit pending,
  lấy lại khóa CPU_MAX trước khi give semaphore đánh thức UI.
- [display.c](../boards/metalio47/src/main/display.c): guard toàn bộ các cửa
  ra quét màn hình, kể cả phục hồi lỗi và các đường gọi lồng nhau.
- [metalio_hw.c](../components/metalio_hw/metalio_hw.c): guard trọn giao dịch
  đọc/ghi I²C và IO expander, bao gồm thời gian chờ driver.
- [read_pico_sd.c](../boards/metalio47/src/components/read_pico/read_pico_sd.c):
  guard mount/unmount, tính dung lượng, sync/format và CMD13 nền.
- [app_book.c](../main/apps/app_book.c): guard lượt làm việc của worker chuẩn
  bị trang; kết thúc guard trước thông báo hoàn tất.

Guard không thay đổi chính sách format/sync, không tự gọi format, không thay
waveform/VCOM, chân, phân vùng, cấu hình flash hoặc cấu hình PSRAM.

## Khóa và thứ tự thực thi

Một semaphore mutex cấp phát tĩnh tuần tự hóa thao tác trên handle SDK PM.
SDK cấp phát một handle `ESP_PM_CPU_FREQ_MAX` lúc khởi tạo. Không tạo task,
timer hoặc cấp phát handle theo từng trang/giao dịch.

Khóa UI được lấy **trước** khi cấu hình min 160 MHz, giữ máy ở mức tối đa
trong lúc khởi tạo. Những giao dịch nền dùng guard lồng nhau trên cùng handle
PM; bỏ khóa UI không bỏ các khóa giao dịch đang còn sống.

```text
UI đang xử lý / dựng hoặc hiển thị trang
    → khóa CPU_MAX giữ nguyên
    → planner xác nhận đọc tĩnh và lượt chờ > 40 ms
    → kiểm tra lại bit pending dưới mutex chính sách
    → bỏ riêng khóa UI, cho SDK hạ xung nếu không còn ràng buộc khác
    → chờ sự kiện/deadline
    → lấy lại khóa UI trước khi chạy vòng xử lý tiếp
```

Khi input đến:

```text
Producer ghi payload
    → giữ bit pending
    → yêu cầu CPU_MAX, hoàn tất thao tác acquire
    → give semaphore cho UI
```

Nếu input đến đúng lúc UI chuẩn bị bỏ khóa, kiểm tra pending và mutex chính
sách phối hợp để không mất sự kiện. Producer báo pending trước khi lấy mutex
CPU; nếu phải đợi, nó lấy lại khóa CPU_MAX trước khi đánh thức consumer.

PM API ở đây chỉ được gọi từ task context, **ngoài critical section**.
Sampler AI trả cờ "đã xếp sự kiện" cho task phím; task đó thoát khóa bảo vệ
epoch rồi mới báo wake. Vì vậy vẫn giữ cơ chế hủy input khi pause/Sleep mà
không gọi mutex/PM trong vùng khóa spinlock.

## Bảo vệ giao dịch

`metalio_cpu_guard_begin()` lấy một lease mức tối đa. Helper
`METALIO_CPU_GUARD_OR_RETURN(...)` dùng cleanup attribute để trả lease kể cả
khi hàm return sớm. Guard phải bao trùm đến lúc giao dịch/DMA đã hoàn tất,
không chỉ lúc gửi yêu cầu bắt đầu.

Với task tự gọi `vTaskDelete(NULL)`, không dựa vào C cleanup để kết thúc guard:
self-delete không quay lại scope. CMD13 nền trả lease rõ ràng trước khi tự xóa;
worker chuẩn bị trang lấy lease sau khi đã giữ khóa vẽ và trả lease trước
khi nhả khóa vẽ/give semaphore hoàn tất, để việc hủy worker dưới khóa vẽ
không bỏ sót lease. Khóa vẽ,
khóa bus và giao dịch giữ nguyên thứ tự vốn có; mutex CPU không được giữ xuyên
suốt I/O, chỉ giữ trong thao tác acquire/release handle PM.

## Đường lỗi và khởi động phục hồi

- Lỗi tạo/acquire handle hoặc cấu hình ban đầu không cho phép cửa sổ DFS.
  Cấu hình khởi động S31 của bản này được kiểm tra là 320 MHz.
- Nếu đã acquire khóa UI nhưng cấu hình thất bại, giữ khóa đó.
- Lỗi PM trong lúc chạy yêu cầu cấu hình cố định 320/320 MHz, tắt DFS cho
  phần còn lại của lần chạy và ghi nhận lỗi.
- Nếu ngay cả cấu hình cố định cũng thất bại, trạng thái `unsafe` chặn giao
  dịch được guard bằng lỗi; không tiếp tục quét/I/O với clock chưa xác nhận.
- Boot có lý do panic, interrupt/task watchdog, watchdog hoặc brownout bỏ qua
  bật DFS trong lần boot phục hồi. Đây không phải cờ tắt vĩnh viễn trong NVS.

Không ghi OTP/eFuse, không đổi boot slot hoặc xóa dữ liệu để xử lý lỗi clock.

## Căn cứ chọn 160 MHz cho lần thử đầu

SDK S31 hiện dùng cùng nguồn CPLL 320 MHz cho CPU 320 và 160 MHz; hai nhánh
đều chọn MEM_CLK 160 MHz. SYS/APB có thay đổi, vì vậy giao dịch cần giữ clock
được bảo vệ bằng khóa mức tối đa. Không sửa thanh ghi clock trực tiếp.

LEDC của S31 có các nguồn XTAL/PLL_F80M/RC_FAST độc lập với xung CPU; cấu hình
đèn 20 kHz và giới hạn công suất hiện có được giữ nguyên. Việc ổn định PWM,
USB, flash/PSRAM và Sleep/wake vẫn thuộc nghiệm thu phần cứng.

Các mức 80 MHz hoặc CPU hoạt động cố định 240 MHz chưa được bật trong chính
sách này. 160 MHz ở đây là mức idle cho phép, không phải mức đã xác nhận đủ
để raster/quét sách nặng.

## Quan sát và ý nghĩa bộ đếm

`metalio_cpu_stats()` trả snapshot trong RAM:

| Trường | Ý nghĩa |
| --- | --- |
| `enabled` | Chính sách cho phép DFS đã cấu hình thành công và chưa bị tắt do lỗi |
| `ui_pinned` | Theo dõi khóa CPU_MAX của UI |
| `unsafe` | Chưa xác nhận được cấu hình clock an toàn sau lỗi PM |
| `idle_windows` | Số cửa sổ đã bỏ khóa UI thành công, không phải số lần thực tế đạt 160 MHz |
| `rejected` | Số yêu cầu idle không được chấp nhận |
| `wakes` | Số lần lấy lại khóa UI thành công từ trạng thái đã bỏ khóa |
| `leases` | Số guard đang được theo dõi; khi có lỗi SDK phải xem kèm `faults` |
| `faults`, `last_error` | Lỗi và nguyên nhân gần nhất được ghi nhận |

Không thêm lệnh USB chẩn đoán hoặc màn hình cấu hình CPU. Snapshot không tự
đo thời gian thực ở mỗi mức xung, năng lượng, p95/p99 hoặc peak SRAM. Có thêm
bộ nhớ cho mutex/handle/bộ đếm; không khẳng định peak SRAM không đổi.

## Kiểm tra host và nghiệm thu tốc độ/pin

Các kiểm tra đã chạy trên mã thực với biên SDK/hardware giả lập:

- Khóa 320 MHz khi hoạt động; chỉ bỏ khóa cho lượt chờ hợp lệ trên 40 ms.
- Guard lồng nhau, giao dịch còn sống khi UI bỏ khóa, và cleanup khi return sớm.
- Input lấy lại khóa trước semaphore; deadline hết cũng lấy lại khóa trước UI.
- Lỗi create/acquire/release/configure, cố định lại 320 MHz, chặn giao dịch nếu
  không xác nhận được clock; boot phục hồi không bật DFS.
- AI báo wake ngoài critical section kể cả khi đọc I²C sau đó lỗi.
- Vòng UI, lật cache/prefetch, màn hình/underrun recovery, SD và lịch đèn.

Tạo source view S31 rồi chạy, ví dụ:

```sh
python3 tools/board_source_view.py --board metalio47 --tests --output build/cpu-tests
cd build/cpu-tests
python3 tools/cpu_policy_host_test.py
python3 tools/cpu_policy_concurrency_host_test.py
python3 tools/ui_wake_host_test.py
python3 tools/metalio_ai_release_rearm_host_test.py
python3 tools/metalio_lock_key_host_test.py
python3 tools/reader_idle_prefetch_host_test.py
python3 tools/reader_wait_loop_host_test.py
python3 tools/test_sd_media_guard.py
bash tools/run_display_host_test.sh
bash tools/run_app_loop_host_tests.sh
```

Các suite cũ dùng ASan có thể gặp lỗi runtime trên máy Mac phát triển; các
lượt tương ứng dùng UBSan qua wrapper compiler cục bộ. Xem lưu ý trong
[UI_EVENT_DEADLINE.md](UI_EVENT_DEADLINE.md#8-kiểm-tra-hồi-quy-và-nghiệm-thu).

Nghiệm thu thiết bị phải so cùng sách/font/độ sáng và trạng thái cache giữa
bản nền RC19 với RC22. Dùng touch/âm lượng/nguồn để kiểm tra input đến ngay
trong cửa sổ idle; BLE bật giữ nhịp/xung hoạt động và cần kiểm tra hồi quy riêng.
Đo lật có cache, thiếu cache, EPUB đổi chương, font SD, ảnh, lật tới/lùi nhanh,
Sleep/wake nhiều lần, USB/WiFi/OTA và lịch đèn. Đo dòng pin, năng lượng mỗi
trang và median/p95/p99 input→dispatch→render→scan; không dùng dòng USB đang
sạc làm dòng pin đọc.

Build và host tests không xác nhận thời gian tăng xung thực tế, không chứng
minh lật trang nhanh bằng bản nền, và không chứng minh tiết kiệm năng lượng.
Chưa có số đo phần cứng để chọn một mức CPU hoạt động thấp hơn 320 MHz.
