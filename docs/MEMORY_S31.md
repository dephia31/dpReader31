# Bộ nhớ Metalio E-INK4 Plus

## 1. Cấu hình đúng board

Dự án sử dụng ESP-IDF qua `tools/build_firmware.py`, không dùng PlatformIO. Board Metalio hiện chọn **ESP32-S31**, chip ID32, dự án `Kiikoread_Metalio_S31`; không áp cấu hình ESP32-S3 lên S31. Firmware và kiểm tra chip trên USB trước đây cùng xác nhận điều này. PSRAM16MB đã hoạt động; SRAM ít không chứng minh PSRAM chưa được bật.

Khối cấu hình hiệu lực trong `boards/metalio47/sdkconfig.defaults`:

```ini
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_200M=y
CONFIG_SPIRAM_USE_MALLOC=y
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16
CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=65536
CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y
CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y
CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL=y
CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE=8192
CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC=y
CONFIG_NVS_ALLOCATE_CACHE_IN_SPIRAM=y
CONFIG_FREERTOS_TASK_CREATE_ALLOW_EXT_MEM=y
CONFIG_FREERTOS_USE_TRACE_FACILITY=y
CONFIG_ESP_MAIN_TASK_STACK_SIZE=16384
```

`CONFIG_SPIRAM_USE_MEMALLOC` không phải tên Kconfig của SDK này: tên đúng là `CONFIG_SPIRAM_USE_MALLOC`. S31 dùng `CONFIG_SPIRAM_MODE_OCT` và200MHz; các tên `ESP32S3_SPIRAM_MODE_OCT`/80MHz trong đề bài không áp dụng. Giữ timing đã dùng trên thiết bị.

Ngưỡng16byte hiện tại đã ưu tiên PSRAM mạnh hơn1024/2048byte. Tăng ngưỡng sẽ đẩy thêm cấp phát nhỏ vào SRAM. `malloc` chỉ **ưu tiên**, có thể fallback; dữ liệu lớn cần ràng buộc `MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT` và xử lý NULL. Pool reserve64KiB được SDK cấp rồi đăng ký lại thành heap có ưu tiên DMA/internal: không phải64KiB biến mất. Giữ64KiB vì riêng bounce DMA màn hình đã cần39040byte; giảm reserve không tương đương thu hồi32KiB SRAM vật lý.

## 2. Phân bổ và ranh giới an toàn

| File / thành phần | Dữ liệu | Chính sách |
| --- | --- | --- |
| `main/app/log_export.c` | `s_log_buf`,32768byte | `EXT_RAM_BSS_ATTR` trên S31; khi cache ngừng không truy cập ring, console vẫn forward; SD I/O ngoài critical section |
| `main/font/ttf_font.c` | `s_slots`,9024byte | metadata slot đặt external BSS; font engine là stb_truetype, không phải FreeType |
| `boards/metalio47/src/main/display_main_waveform.c` | `s_gray`, `s_bw`, `s_page_gray`,7424byte | external BSS cho bảng CPU tạo lúc chạy |
| `boards/metalio47/src/components/e0470_epaper_waveform/e0470_epaper_waveform.c` |4 bảng mutable,12288byte | external BSS; giữ nguyên nội dung, waveform gốc, phase timing và VCOM |
| `components/epdiy/src/highlevel.c` | front/back415872byte mỗi buffer, diff831744byte | đã PSRAM từ trước; bổ sung rõ `MALLOC_CAP_8BIT`; tổng1663488byte |
| `main/font/ttf_font.c` | TTF, glyph bitmap/cache, layout work, optional table maps | đã cấp PSRAM; giữ giới hạn optional table maps2MiB và thu hồi khi áp lực bộ nhớ |
| `main/book/zip_reader.c` | ZIP handle, central directory, names, input/inflate/prefetch | handle chuyển từ calloc thường sang heap_caps_calloc PSRAM; các buffer lớn đã PSRAM |
| `boards/metalio47/src/components/epdiy/src/output_lcd/lcd_driver.c` |2 bounce19520byte + DMA descriptors160byte | giữ `MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA` |
| TinyUSB / ISR | endpoint buffer16384byte; ISR stack/queue/LUT phục vụ ngắt | giữ internal; không chuyển hàng loạt chỉ để làm đẹp số free |

Mẫu cấp phát bắt buộc, không fallback nội:

```c
void *buffer = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
if (!buffer) return ESP_ERR_NO_MEM;
/* ... */
free(buffer);
```

Với BSS tồn tại suốt ứng dụng:

```c
#include "esp_attr.h"
static EXT_RAM_BSS_ATTR uint8_t cpu_only_buffer[32768];
```

Không sử dụng mẫu này cho DMA/ISR hoặc dữ liệu cần truy cập khi flash cache ngừng. Callback LCD ISR chỉ tiêu thụ line queue và trạng thái internal; bảng nguồn waveform được đọc trong task dựng frame. Không thay đổi waveform dữ liệu gốc.

### Task stack

Tất cả stack dưới đây được giữ internal. Dung lượng là byte yêu cầu, không gồm TCB/overhead. Task ngắn hạn không tồn tại đồng thời; không cộng toàn bảng thành mức tiêu thụ thường trực.

| Task | Stack | Nguồn / lý do |
| --- | ---: | --- |
| main |16384| SDK, board defaults; boot/font sâu, đã từng gặp guard lỗi khi nhỏ hơn |
| epd_prep ×2 |4096 mỗi task| board `epdiy/src/render.c`; render worker, giữ tương thích cache/driver |
| metalio_keys |2048| `components/read_pico_pmu/metalio_power_adapter.c` |
| metalio_haptic |2048| `components/metalio_hw/metalio_hw.c` |
| ft6336_feed |3072| `components/cst836u/metalio_ft6336.c` |
| nimble_host |8192| `components/ble_page_turner/src/ble_page_turner.c`; BLE và lưu bond NVS |
| sd_probe |4096| board `read_pico_sd.c`; ngắn hạn |
| sd_space |4096| cùng file; ngắn hạn |
| sd_health |3072| cùng file |
| sd_recover / sd_manual |4096| cùng file; hai đường tạo cùng worker |
| s31_sd_write |4096| `components/read_pico_transfer/transfer_stream_writer.h`; payload PSRAM, control internal |
| rtc_sync |6144| `main/time_service.c`; Wi-Fi/config |
| avatar |12288| `main/ui/ui_avatar.c`; decode ảnh/SD, cần watermark đường ảnh nặng trước khi giảm |
| covers |12288| `main/book/book_cover_jobs.c`; decode ZIP/ảnh |
| book_prep |12288| `main/apps/app_book.c`; EPUB/layout/font, không giảm xuống4KiB khi chưa đo peak |
| font_online |8192| `main/font/online_font.c`; tải mạng và cấu hình |
| kosync_auto |12288| `main/book/kosync_client.c`; TLS và NVS |
| kiikoread_ota |12288| board `main/ota_online.c`; flash, bắt buộc internal |
| TinyUSB |4096| `components/espressif__esp_tinyusb/tinyusb_task.c`; driver quản lý |
| HTTP server |12288| `components/read_pico_transfer/read_pico_transfer.c`; IDF tạo, task_caps internal |
| pie_check |4096| benchmark board; tắt trong firmware giao người dùng |

Task Wi-Fi/lwIP/timer/idle do SDK tạo ngoài các lời gọi ứng dụng; snapshot runtime liệt kê cả chúng. Không coi test_apps của vendor là task firmware. Không tìm thấy triển khai Bluetooth audio/A2DP/I2S audio trong đường build này; BLE hiện phục vụ phím lật trang.

Chưa giảm stack vì chưa có watermark đo trên thiết bị qua các đường EPUB lớn, glyph khó, TLS, ảnh và lỗi SD. SDK có `xTaskCreateWithCaps`, nhưng việc bật external stack không đảm bảo mọi task an toàn khi flash cache ngừng. Chỉ chuyển sau khi kiểm tra toàn bộ đường gọi và dùng `vTaskDeleteWithCaps` tương ứng. Giữ stack trong lượt sửa này tránh tái phát lỗi cache khi boot/OTA/NVS.

## 3. Theo dõi và nghiệm thu

Helper hoàn chỉnh nằm trong `components/metalio_hw/metalio_memory_profile.c`, API ở `include/metalio_memory_profile.h`:

```c
void print_memory_profile(const char *stage);
void print_task_stack_profile(void);
```

Helper in byte free/largest/min/total internal, free/largest/total PSRAM, free/largest DMA, watermark stack hiện tại. DMA là một phần internal, không cộng hai số này. Task snapshot cấp phát PSRAM; nếu hết PSRAM thì bỏ snapshot, không chiếm thêm SRAM.

Các điểm gọi:

- `boards/metalio47/src/main/app_main.c`: sau khởi tạo log, stage `runtime-after-bootloader`; PSRAM đã được IDF khởi tạo trước app_main. Đây là checkpoint ứng dụng sau bootloader, không sửa bootloader để gọi allocator sớm.
- Cùng file: `display-and-board-ready` sau init BSP/màn hình.
- `main/apps/app_book.c`: `epub-opened`/`txt-opened` sau mở sách thành công, kèm snapshot tất cả task.

So sánh cùng thiết bị, cùng sách/font/trang, cùng Wi-Fi/BLE và cùng thời điểm. Ghi cả free và largest khi mở sách, lật liên tục, về Home, bật Wi-Fi/KoSync, BLE và sau khi đóng các chức năng. Nếu free đủ nhưng largest nhỏ, cần điều tra phân mảnh; minimum-ever thấp không đồng nghĩa hiện tại đang thấp.

Đối chiếu ELF baseline và build mới xác nhận **61504byte (60.06KiB)** của9 symbol rời vùng internal0x2f... sang external0x500...; đây là mức chuyển dữ liệu tĩnh, không phải số free đo trên máy. Mục tiêu160–220KiB chưa được chứng minh. Nếu baseline88KiB đo cùng trạng thái, riêng mức chuyển này chỉ gợi ý khoảng148KiB trước các thay đổi runtime/overhead; không hứa160KiB bằng cách cắt stack/DMA thiếu bằng chứng.

Tham khảo chính thức: [ESP-IDF6.1, external RAM ESP32-S31](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s31/api-guides/external-ram.html). Build và phép đo runtime phải dùng cùng SDK/board; tài liệu S3 không thay thế cấu hình S31.

## KoSync và Bluetooth: phần có thể chuyển thêm

NimBLE host đã bật `CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL=y`: hàm SDK `nimble_mem_malloc()` dùng `MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT`. TLS của KoSync đã bật `CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC=y`, Wi-Fi/lwIP đã ưu tiên PSRAM. Không thể thu hồi lần thứ hai những phần vốn đã ở PSRAM.

Bản1.1.8RC2 chuyển thêm các dữ liệu CPU tĩnh sang external BSS, chỉ trên S31 khi build ESP-IDF:

- `main/book/kosync_client.c`: hàng đợi push/pull/running và kết quả đồng bộ; lock/owner/task control vẫn internal. Buffer MD5 và HTTP response không fallback sang malloc khi hết PSRAM, trả lỗi và dọn tài nguyên.
- `components/ble_page_turner/src/ble_page_turner.c`: danh sách quét `s_devices` và danh sách bond `s_bonds`; giữ vòng sự kiện phím và khóa trong SRAM, không chuyển ISR/controller.
- `main/apps/app_book.c`: lịch sử OPDS1536byte.
- `main/book/book_layout.c`: scratch font runs512byte.
- `main/book/book_source.c`: đường dẫn I/O512byte.

Bật tùy chọn SDK `CONFIG_NVS_ALLOCATE_CACHE_IN_SPIRAM=y` cho cache page/key; stack thực hiện NVS vẫn internal. Tùy chọn này có fallback do SDK quản lý và có thể làm thao tác NVS chậm hơn; không hứa mức tiết kiệm heap cố định vì phụ thuộc số page/key và nhiều cấp phát NVS trước đây đã ưu tiên PSRAM qua malloc.

Radio/controller dùng allocator internal/DMA của SDK (`bt_osi_mem_malloc_internal`); không sửa thư viện hoặc ép pointer PSRAM vào đường radio. NimBLE host stack8KiB và KoSync worker12KiB vẫn internal vì có đường lưu bond/cấu hình/tiến độ NVS; chỉ cân nhắc giảm/chuyển sau khi đo watermark và chứng minh an toàn cache-off. Không cắt số buffer radio hoặc giảm MTU/số thiết bị để đổi lấy con số SRAM đẹp hơn.

Kiểm tra runtime cần bao gồm: Bluetooth quét/kết nối/lật trang, pair/forget rồi khởi động lại để kiểm tra bond; KoSync cold Wi-Fi, STA đã kết nối, đồng bộ tự động/thủ công, sai tài khoản và lỗi mạng. ELF chỉ chứng minh vị trí dữ liệu; không thay thế kiểm tra radio/ghi NVS trên máy.

RC2 ELF xác nhận chuyển thêm5644byte so RC1: KoSync1560byte, Bluetooth1524byte, OPDS1536byte, layout512byte, I/O path512byte. Tổng18 symbol đã chuyển67148byte so bản chính thức1.1.7. Đây là tổng kích thước dữ liệu, không phải số free runtime; cache NVS không được cộng vào mức chuyển tĩnh.


### Phản hồi KoSync và thu hồi tài nguyên

Từ1.1.8RC3, thao tác KoSync trong trình đọc (nút, shortcut và nút đẩy tiến độ trong hộp chuyển chương) gọi `reader_show_sync_notice()` trước `reader_prepare_network()`/Wi-Fi/HTTP. Helper vẽ và đẩy ngay vùng248×48pixel với chữ chính xác “Đang đồng bộ”, không cấp thêm framebuffer hoặc worker; thông báo kết quả thay thế sau khi yêu cầu trả về.

`kosync_request()` cleanup HTTP client (bao gồm buffer/TLS thuộc client) trước khi nhả mạng tạm, cả khi perform lỗi; init thất bại vẫn nhả mạng tạm. Buffer MD5, response và JSON/body được free ở các đường kết thúc. Auto worker kết thúc bằng `vTaskDelete(NULL)`, stack được idle task thu hồi. Mạng đã kết nối trước không thuộc phiên KoSync nên được giữ; BLE được trả lease theo lớp mạng. Không giải phóng cache/font/controller dùng chung chỉ để làm số free tăng. State KoSync external BSS nhỏ có vòng đời toàn ứng dụng, không phải heap rò rỉ theo từng lần đồng bộ.

Host test `tools/kosync_feedback_host_test.py` thực thi helper thật và HTTP request thật với lỗi init/perform, kiểm tra thứ tự present trước network và cleanup trước release. Cần kiểm tra hiển thị E-ink và free/largest lặp nhiều phiên trên thiết bị để xác nhận runtime.
