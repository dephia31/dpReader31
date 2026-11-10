# NVS chung 256 KiB — bản thử 1.4.2RC2

Worktree `codex/nvs256-rc`, nền `588f798` (1.4.1). Full và Love Book cùng dùng bố cục này. Đây là bản thử cục bộ, chưa phát hành và chưa nạp lên thiết bị.

| Vùng | Địa chỉ | Kích thước |
|---|---|---|
| `nvs_old` | `0x9000` | `0x4000` — 16 KiB, nguồn cũ giữ nguyên |
| `otadata` | `0xD000` | `0x2000`, giữ nguyên |
| `phy_init` | `0xF000` | `0x1000`, giữ nguyên |
| `ota_0` | `0x10000` | `0x500000`, giữ nguyên |
| `ota_1` | `0x510000` | `0x500000`, giữ nguyên |
| `storage` | `0xA10000` | `0x5A0000` — 5.625 MiB trước overhead FAT/WL |
| `nvs` | `0xFB0000` | `0x40000` — 256 KiB |
| `coredump` | `0xFF0000` | `0x10000`, giữ nguyên |

## Cài thử

1. Dùng gói đúng edition trong `Test Build/<version>/`. Chạy `FLASH_USB.command` trên macOS hoặc `FLASH_USB.bat` trên Windows; có thể thêm `--port /dev/cu.usbmodem...` hoặc `--port COM5`.
2. Python cần `esptool >= 5.6`, `pyserial`, `construct`. macOS ưu tiên môi trường ESP-IDF có sẵn; không tự tải firmware hay dependency. Gói kèm các module FAT/WL của SDK và giấy phép.
3. Bộ cài kiểm tra chip S31, MAC, SHA-256, version và bảng phân vùng. Máy dùng layout mới chỉ nhận app và chọn boot OTA; máy layout cũ đi qua chuyển đổi bên dưới. Không có thao tác xóa toàn flash, ghi bootloader, eFuse, TPS hoặc SD.
4. Sau khi máy khởi động, kiểm tra phiên bản, font, PIN, Wi-Fi, kết nối lại BLE, các phím đã học và sách/font trong `/flash`. Giữ nguyên thẻ SD cũ.

Lần chuyển đổi đọc và đối chiếu nhiều MiB bằng stub chạy trong RAM. Các bước ghi vẫn chạy qua ROM với `--no-stub`. Quá trình này lâu hơn cài app thông thường. Không ngắt cáp. Bộ cài không xác nhận thiết bị đã boot đúng chỉ từ kết quả ghi USB.

## Chuyển dữ liệu

Bộ cài dừng CPU trong download mode, đọc NVS/storage hai lần và đối chiếu; lưu bản gốc NVS, storage, bảng phân vùng và otadata vào `Migration Backups/<MAC>/`, kèm checksum. Backup có thông tin Wi-Fi/khóa BLE riêng của thiết bị; không chia sẻ công khai.

NVS được sao chép **nguyên 16 KiB byte**, nối thêm các trang đã xóa tới 256 KiB. Cách này giữ mọi namespace, kiểu giá trị, khóa Wi-Fi, NimBLE và cấu hình không cần biết trước tên khóa. Kiểm thử dùng chính lớp Storage của ESP-IDF xác nhận việc mở lại, đọc blob nhiều trang và thu hồi trang sau mở rộng. Không xóa hay khôi phục đè từ `nvs_old` ở mỗi lần boot.

Storage được giải wear-leveling 4096 byte, kiểm tra CRC và hai bản trạng thái, phân tích FAT12/FAT16, dựng lại FAT12/WL nhỏ hơn. Tên dài UTF-16, nội dung, thư mục rỗng, thuộc tính và timestamp được đối chiếu trước khi ghi. Tên ngắn 8.3 có thể được tạo lại. Dữ liệu trống toàn `FF` được tạo thành filesystem rỗng. Không hỗ trợ ảnh mã hóa, sector khác 4096, trạng thái WL không nhất quán, FAT hỏng/cross-link/orphan hoặc dữ liệu không vừa vùng mới: dừng trước khi ghi. Không tự bỏ tệp để làm vừa.

Ghi storage, NVS mới và app rồi đọc lại so từng byte; kiểm tra NVS cũ còn nguyên. Sau đó ghi otadata, cuối cùng ghi bảng phân vùng và đọc lại. Chỉ reset chạy firmware khi tất cả bước thành công. Firmware kiểm tra layout trước khi khởi tạo NVS và không format `/flash` khi mount thất bại.

## Khi gián đoạn

Giữ nguyên gói và backup. Đưa máy vào download mode, chạy cùng launcher với `--resume "đường dẫn Migration Backups/<MAC>"`. Bộ cài xác minh MAC, firmware, checksum backup và NVS cũ rồi ghi lại từ dữ liệu đã chuẩn bị; cho phép tiếp tục nếu bảng phân vùng bị ghi dở. Không khởi động firmware cũ trong lúc chuyển đổi dở, vì nó có thể sửa dữ liệu gốc. Backup chưa có `migration.json` là bước chuẩn bị chưa hoàn tất, chưa có ghi dữ liệu thiết bị; chạy lại launcher bình thường để sao lưu vào thư mục mới. Bộ cài giữ lại bản dở.

Bản `nvs_old` chỉ là snapshot thời điểm chuyển, không chứa cấu hình mới phát sinh sau chuyển đổi. Không hạ về layout cũ bằng app-only/TF/OTA. Phục hồi layout cũ cần khôi phục ảnh storage và NVS cũ cùng firmware/bảng phân vùng cũ tương ứng; bộ cài này không tự thực hiện rollback.

## BLE, thống kê và kiểm tra

Hồ sơ bốn thiết bị/phím đã học trên `/sdcard/DPRBLE` giữ nguyên định dạng và journal của 1.4.1. Khóa NimBLE vẫn dùng `nvs_open()` mặc định, nay trỏ tới `nvs` 256 KiB. Các namespace BLE legacy cũng đi theo ảnh NVS nguyên byte; luồng nhập legacy trên SD được giữ nguyên. Không đồng nhất phím đã học trên SD với khóa bonding trong NVS.

Log boot có tổng used/free/total và số entry theo namespace, không in giá trị bí mật. Không có tác vụ thống kê trong luồng lật trang. SDK thu hồi bản ghi thay thế/xóa; không tự xóa namespace người dùng hoặc tiến độ legacy để giải phóng chỗ.

Đã kiểm tra trên host: chuyển filesystem/Unicode/ẩn/rỗng, từ chối quá dung lượng và ảnh hỏng, USB gián đoạn/resume, kiểm tra package, journal BLE; stress bằng NVS SDK thật đạt 7.787 khóa số bổ sung tới đầy, xóa/ghi lại nhiều vòng, GC và mở lại vẫn giữ dữ liệu gốc. Số này là fixture host, **không phải dung lượng sử dụng thực trên thiết bị**. Cần kiểm tra mất nguồn thực, mount `/flash`, RAM/boot và reconnect Wi-Fi/BLE trên máy.
