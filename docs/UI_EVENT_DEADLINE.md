# Vòng lặp UI chờ sự kiện và deadline trên S31

Tài liệu mô tả cơ chế hiện được triển khai từ bản thử `1.2.9RC19-vn`, làm căn
cứ bảo trì và mở rộng. Mục tiêu là giảm lượt chạy UI khi đọc tĩnh, đồng thời
ưu tiên nhận input và giữ cache lật trang. Đây là thiết kế đã build và kiểm tra
host; chưa có số đo pin, độ trễ quang học hoặc nghiệm thu trên thiết bị.

## 1. Nguyên tắc bảo vệ tốc độ lật trang

- Input mới đánh thức UI qua semaphore; không phải đợi hết timeout đọc tĩnh.
- Chốt đang có sự kiện hoặc hàng đợi AI/âm lượng còn dữ liệu thì thời gian chờ bằng 0.
- Đang thao tác, mở sách, dàn trang, cache thiếu hoặc có trạng thái chưa được
  hỗ trợ thì giữ nhịp dự phòng cũ: 8 ms khi hoạt động, 40 ms khi rảnh.
- Chỉ kéo dài lượt chờ sau ít nhất 400 ms yên kể từ input gần nhất.
- Kiểm tra điều kiện chờ không dựng trang, không cấp phát, không đọc file và
  không chờ worker. Khóa cache chỉ được thử với timeout 0.
- Giữ cache trang/font, cách raster, đường quét màn hình và thứ tự ưu tiên
  mở sách → hiển thị → tác vụ radio. Không bật PM/DFS hoặc thay waveform/VCOM.

Trần 200 ms là thời gian UI **có thể chờ khi không có input**, không phải
khoảng nghỉ bắt buộc giữa hai lần lật trang. Độ trễ tổng vẫn gồm lấy mẫu,
chống dội, điều phối task, dựng nội dung và quét màn hình; cần đo trên máy thật.

## 2. Thành phần và mã nguồn

| Thành phần | Vai trò |
| --- | --- |
| [metalio_ui_wake.h](../components/metalio_hw/include/metalio_ui_wake.h), [metalio_ui_wake.c](../components/metalio_hw/metalio_ui_wake.c) | Chốt đánh thức và bộ đếm, dùng semaphore nhị phân cấp phát tĩnh |
| [app.h](../main/app/app.h) | Hợp đồng tùy chọn `app_desc_t.next_tick_ms` |
| [app_wait.h](../main/app/app_wait.h) | Hàm thuần chọn thời gian chờ theo deadline |
| [app_loop.c](../main/app/app_loop.c) | Điều kiện cho phép chờ dài, tổng hợp deadline và chờ sự kiện |
| [app_book.c](../main/apps/app_book.c) | Điều kiện trang đọc/cache sẵn sàng, deadline đồng hồ và thống kê |
| [anti_fade.c](../main/anti_fade.c) | Hạn làm mới chống nhạt theo chính sách hiện có |

## 3. Chốt đánh thức: thông báo không thay cho dữ liệu

Một UI consumer chờ semaphore riêng. Các producer từ **task context** ghi dữ
liệu vào hàng đợi/trạng thái của mình trước, sau đó gọi
`metalio_ui_wake_post(reasons)`. Không gọi API này trực tiếp từ ISR.

| Bit lý do | Nguồn đã nối |
| --- | --- |
| `METALIO_UI_WAKE_TOUCH` | Task lấy mẫu FT6336 ở chế độ hoạt động |
| `METALIO_UI_WAKE_KEY` | Sự kiện nguồn đã chống dội, sự kiện AI/âm lượng được xếp hàng |
| `METALIO_UI_WAKE_BLE` | Báo cáo key/raw đã được chấp nhận vào hàng đợi BLE |
| `METALIO_UI_WAKE_PREP` | Worker chuẩn bị trang hoàn tất lượt làm việc |

Các bit và semaphore chỉ gộp **yêu cầu thức dậy**. Dữ liệu input vẫn nằm trong
hàng đợi gốc, với giới hạn và chính sách tràn vốn có; chốt không mở rộng dung
lượng hoặc bảo đảm giữ input vượt quá dung lượng hàng đợi.

Sự kiện trước lúc UI khởi tạo được giữ bằng bit pending. Sự kiện đến trong
cửa sổ tính deadline/chuẩn bị block vẫn làm lượt chờ trả về. Chốt không dùng
chung task notification với sampler hoặc renderer, tránh tiêu thụ nhầm thông báo.
Không tạo task, timer hoặc cấp phát heap mới cho chốt.

Do producer có thể bị chuyển lịch giữa cập nhật bit và give semaphore, có thể
có lượt thức mà dữ liệu đã được xử lý ở vòng trước. Đây là lượt thức rỗng,
được đếm riêng, không được báo thành timeout thật.

## 4. Hợp đồng deadline của trang

```c
int64_t (*next_tick_ms)(app_ctx_t *ctx);
```

- `NULL`: trang chưa opt-in, giữ nhịp cũ.
- Trả `0`: chưa đủ điều kiện kéo dài lượt chờ, giữ nhịp cũ.
- Trả số dương: hạn tuyệt đối theo đồng hồ monotonic, đơn vị ms.
- Callback không thực hiện I/O, cấp phát hoặc đợi khóa/worker.

Không trả timestamp UTC trực tiếp. Khi cần mốc theo giờ thực, chẳng hạn phút
đồng hồ kế tiếp, chuyển khoảng thời gian còn lại sang deadline monotonic.

`reader_next_tick_ms()` chỉ cho phép chờ dài khi:

1. Đang ở nội dung sách, có text và trang đã hiển thị thành công.
2. Không mở toolbar/panel/choice, không có tap chờ, hold, zoom, ghi chú, thông
   báo hoặc nút hoàn tác chuyển chương còn hiệu lực.
3. Không có yêu cầu mở sách, quét thẻ, đổi font, chuẩn bị nền, phục hồi font
   hoặc lỗi lưu tiến độ; font đang dùng khớp với font của layout.
4. Không dùng ảnh trên các trang cần kiểm tra hoặc nền đọc tùy chỉnh.
5. Công việc bìa khóa và làm ấm chương gần mép chương đã hoàn tất khi cần.
6. Cache các trang kế tiếp thứ nhất, thứ hai và trang trước đã sẵn sàng nếu
   các trang đó tồn tại. Kiểm tra đúng generation và kích thước framebuffer.
7. KoSync tự động đang tắt.

Callback thử khóa vẽ bằng `xSemaphoreTake(..., 0)`. Khóa bận, cache thiếu hoặc
chưa có khóa đều trả 0; không đợi để quyết định có được chờ dài hay không.

Deadline trang là hạn sớm nhất giữa mốc kiểm tra tối đa 1 giây, phút đồng hồ
kế tiếp, ngưỡng ghi thống kê 30 giây tích lũy và kết thúc cửa sổ tính hoạt động
5 phút khi còn trong cửa sổ đó. Trần 200 ms của vòng UI vẫn áp dụng bên ngoài.

## 5. Điều kiện và cách chọn lượt chờ

Ở triển khai hiện tại, chỉ trang đọc tĩnh **offline, Bluetooth tắt** mới được
kéo dài lượt chờ. Menu, gesture, pinch, phím đang giữ, feedback, yêu cầu chuyển
trang, phục hồi SD, USB, tải dữ liệu, OTA và tác vụ mạng đều chặn điều kiện này.
WiFi phải ở trạng thái `STOPPED` và không có chủ tác vụ mạng đang bận.

UI chọn hạn gần nhất trong bảng sau:

| Công việc | Hạn được đưa vào planner |
| --- | --- |
| Trang đọc | Kết quả `next_tick_ms()` |
| Kiểm tra phím nguồn | Lần kiểm tra trước + 200 ms khi PMU sẵn sàng |
| Media/SD | Lần kiểm tra trước + 500 ms |
| Dịch vụ mạng, lịch đèn, lịch NTP | Lần kiểm tra nhóm trước + 500 ms |
| Thử font SD | Lần kiểm tra trước + 750 ms; không đồng nghĩa stat font mỗi 750 ms |
| Tự khóa | Input gần nhất + thời gian tự khóa đã chọn |
| Anti-fade | Hạn theo cấu hình và lần refresh/input gần nhất |
| Giới hạn chung | Thời điểm hiện tại + 200 ms |

Nếu không đủ điều kiện thì dùng 8/40 ms cũ. Nếu có input pending thì dùng 0.
Nếu deadline đã hết thì dùng 0 để xử lý việc đến hạn ở vòng kế tiếp.

Sau xử lý/render, UI đọc lại thời gian monotonic trước khi tính timeout.
Không lấy deadline cũ rồi cộng thêm thời gian đã tiêu tốn để quét màn hình.

```text
Xử lý input và công việc đến hạn
        ↓
Hiển thị khi cần
        ↓
Kiểm tra input pending và điều kiện trang/cache
        ↓
Tính deadline gần nhất, trần 200 ms
        ↓
Chờ semaphore hoặc hết hạn
        ↓
Lặp lại
```

Phím nguồn có bit sự kiện được kiểm tra ngay, không phải chờ đủ kỳ 200 ms.
Nhịp lấy mẫu/chống dội và ngưỡng nhấn giữ của các phím không bị kéo dài.

## 6. Giới hạn và các bẫy khi mở rộng

- Trang khác, BLE bật, WiFi/tác vụ mạng hoạt động và các trạng thái phức tạp
  vẫn giữ nhịp cũ. Đây chưa phải chuyển toàn bộ hệ thống sang chờ vô hạn.
- Không dùng mọi hàng đợi BLE raw đang có dữ liệu làm lý do chờ 0 toàn cục:
  một trang không có consumer tương ứng có thể khiến UI chạy liên tục.
  BLE hiện đánh thức bằng chốt chung; các đường đọc dữ liệu và nhịp dự phòng
  vẫn giữ chính sách hiện có.
- Deadline đã hết nhưng công việc không cập nhật lại hạn có thể tạo busy loop.
  Trạng thái đang bị chặn phải giữ nhịp dự phòng hoặc cung cấp hạn thử lại rõ ràng.
- Chỉ nối wake source chưa đủ để kéo dài chờ: phải kiểm tra pending payload,
  công việc chạy ngay và tất cả hạn thời gian liên quan của ngữ cảnh đó.
- Chờ trên semaphore không đồng nghĩa CPU đã ngủ sâu. CPU vẫn 320 MHz và PM
  chưa bật; giảm lượt UI chạy không phải số đo mức tiết kiệm pin.
- Không dùng cơ chế này để đánh thức máy trong khóa/Sleep. Vòng khóa/PIN có
  quy trình riêng; WiFi, lịch đèn và bảo vệ OTA vẫn theo chính sách của chúng.

## 7. Bộ đếm phục vụ đo đạc

Lấy hai snapshot `metalio_ui_wake_stats()` rồi tính chênh lệch để đo một phiên:

| Trường | Ý nghĩa |
| --- | --- |
| `iterations` | Số vòng UI bắt đầu |
| `posts` | Số lần post có lý do khác 0 |
| `waits` | Số lượt gọi chờ |
| `event_wakes` | Lượt chờ trả về có bit pending |
| `timeouts` | Lượt chờ không nhận semaphore và không có bit pending |
| `empty_wakes` | Lượt nhận semaphore nhưng không còn bit pending |
| `reasons` | OR tích lũy các loại sự kiện đã nhận, không phải số lượng từng loại |

Bộ đếm dùng `uint32_t`, có thể quay vòng. Dùng phép trừ unsigned cho khoảng đo
phù hợp. Chúng không đo thời gian raster/scan, stack, điện năng hoặc số input
bị hàng đợi từ chối. Không có màn hình hoặc lệnh USB chẩn đoán mới được thêm.

## 8. Kiểm tra hồi quy và nghiệm thu

Tạo source view đúng S31 trước khi chạy host tests:

```sh
python3 tools/board_source_view.py --board metalio47 --tests --output build/ui-deadline-tests
cd build/ui-deadline-tests
python3 tools/reader_wait_deadline_host_test.py
python3 tools/reader_wait_loop_host_test.py
python3 tools/ui_wake_host_test.py
python3 tools/anti_fade_deadline_host_test.py
python3 tools/ui_wake_ble_queue_host_test.py
python3 tools/reader_idle_prefetch_host_test.py
bash tools/run_app_loop_host_tests.sh
```

Các suite cũ có dùng AddressSanitizer. Máy Mac phát triển hiện gặp lỗi runtime
ASan; các lượt tương ứng đã kiểm tra với UBSan qua compiler wrapper cục bộ.
Không xem lỗi runtime sanitizer là kết quả kiểm tra firmware, và không ghi
nhận một lượt ASan không chạy được là PASS.

Phạm vi host đã kiểm tra gồm điều kiện cache/frame/công việc chờ, deadline
đã hết sau render, input pending chờ 0, chuyển lại nhịp active 8 ms, fallback
BLE/mạng, 8.000 payload từ bốn luồng gửi, anti-fade, hàng đợi BLE, 40 lượt
lật cache liên tục và bộ tình huống vòng UI/Sleep.

Trước khi kết luận không tăng độ trễ trên thiết bị, cần so cùng sách/font,
độ sáng, cache và cấu hình với bản nền:

- Input đến ngay trước, trong và sau lúc UI bắt đầu chờ; chạm, AI, âm lượng,
  nút nguồn và BLE; nhấn nhanh liên tiếp, giữ và double-tap.
- Lật tới/lùi, đổi chương, thiếu cache, ảnh/nền, thẻ chậm hoặc tháo thẻ.
- Mở sách/khung đầu, Sleep/wake, đồng hồ đổi phút, lịch đèn, tự khóa và OTA bận.
- Đo median/p95/p99 từ input đến hiện trang; tách dispatch, dựng trang và scan.
- Đo chênh lệch bộ đếm UI, thời gian radio bật, dòng pin và peak SRAM riêng.

Host tests và build thành công không thay thế nghiệm thu phần cứng.
