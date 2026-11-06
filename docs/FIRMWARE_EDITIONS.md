# Full và Love Book

Cùng mã nguồn, hai cấu hình biên dịch. Full giữ các chức năng hiện tại. Love Book cố định I Love Book, mở TXT/EPUB trực tiếp bằng trình duyệt Books, giữ tiến độ, USB, WiFi, OPDS và KoSync; bỏ thư viện/kệ sách, thiết lập kiểu kệ và màn hình khóa ghép bìa. Mã nguồn hiện tại không tích hợp TTS ở cả hai bản.

## Build và giao gói thử

Dùng `python3 tools/build_firmware.py --board metalio47` với SDK/Python/tools như hướng dẫn BOARDS. Mặc định `--edition all` tạo cả hai; `--edition full` hoặc `--edition love-book` dành cho xác minh riêng. `--output <root>` vẫn tách `<root>/full` và `<root>/love-book`; không chia sẻ sdkconfig/build cache giữa hai bản.

Phiên bản gốc nằm ở `boards/metalio47/board.json`. Full dùng `1.3.3RC4-vn`, Love Book được suy ra `1.3.3RC4-love-vn`; bản chính thức tương ứng `1.3.3-vn` / `1.3.3-love-vn`. Trước giao gói mới, chọn RC tiếp theo chưa tồn tại ở cả hai bản. Phiên bản nhúng truyền từ công cụ build, không đổi tên Bin để giả phiên bản. Cả hai gói giữ nhận diện chip/project tương thích với bộ nạp S31 và phân vùng gốc.

Gói RC xuất vào `Test Build/<version>/`, mỗi gói có Bin, SHA256SUMS, metadata `edition` và FLASH_USB.command/.bat. Việc chuyển Full/Love Book qua USB không xóa NVS/thẻ sách. Thiết lập giao diện cũ vẫn được giữ để quay lại Full; Love Book bỏ qua lựa chọn đó và không ghi đè.

## Web Flash và cập nhật

Web Flash có hai bộ chọn độc lập: phiên bản firmware Full/Love Book và cách nạp toàn phần/chỉ ứng dụng. Full giữ nguyên `releases/`; Love Book dùng `releases/love-book/`. Mỗi nhánh có `metalio-s31-update.json`, ứng dụng `dpReader_metalio47_firmware.bin`, ảnh toàn phần `dpReader_metalio47_full.bin` và thư mục lịch sử `<version>/`. Metadata Love Book phải có phiên bản kết thúc `-love-vn`. Thiếu/sai metadata thì khóa nạp và ẩn link tải của lựa chọn đó; không dùng gói Full thay thế.

**Web Flash chỉ nhận bản chính thức, không nhận RC, kể cả xem thử cục bộ.** Build RC chỉ xuất hai gói thử USB/TF trong `Test Build/`, không tạo Web Flash preview. Sau dual build bản chính thức thành công mới tạo `build/editions/web-preview/`. Công cụ stage và mã Web Flash đều từ chối RC; không đưa RC lên website, Pages hoặc OTA.


Lựa chọn OTA lần đầu được lưu trong NVS của thiết bị. Phiên bản trước 1.3.3 phải cập nhật lên 1.3.3 trước khi có màn hình chọn nhánh.

Firmware Love Book dùng feed HTTPS riêng dưới `releases/love-book/` trên cùng các nguồn được phép; Full giữ feed cũ. Kiểm tra cập nhật và Bin từ thẻ đều từ chối chuyển nhánh. Từ 1.3.3, lần kiểm tra OTA đầu tiên cho chọn Full hoặc Love Book. Lựa chọn lưu trong NVS, dùng cho các lần OTA sau. Thiết bị xác nhận trước khi tải nếu gói chuyển nhánh, giữ kiểm tra chip, project, phân vùng, phiên bản và SHA. Chuyển bằng USB vẫn được hỗ trợ. Chưa phát hành nhánh Love Book lên máy chủ thì OTA của nhánh này chưa có bản tải được.

Khi được yêu cầu phát hành chính thức, build cả hai từ cùng mã nguồn, xác minh checksum/phiên bản/chip/phân vùng, lưu từng bộ ứng dụng/toàn phần/USB/SHA riêng. Giữ nguyên URL và bytes chính thức Full hiện có. Lưu lịch sử và `previous_version` riêng từng nhánh trước khi thay metadata; kiểm tra Web Flash lựa chọn đúng ứng dụng/toàn phần và thử cập nhật trên thiết bị. Quy trình này không tự cấp quyền xuất bản hoặc ghi đè bản chính thức bằng RC.

Từ bản chính thức 1.3.3, công cụ kiểm tra/phát hành yêu cầu đủ cả Full và `releases/love-book/`, cùng phiên bản gốc; Release gắn thêm tài sản Love Book có tên phân biệt. Bản Full 1.3.2 trở về trước giữ tương thích quy trình cũ. Lần đầu phát hành Love Book có thể chưa có `previous_version`; các lần sau phải lưu bản Love Book trước đó. Pages xác minh cả hai trước khi chép tài sản.

## Nơi sửa mã nguồn

Mã nguồn thật nằm ở `firmware/shared`, `firmware/full`, `firmware/love_book`; xem [quy ước thư mục](../firmware/README.md). Các đường dẫn `main/*.c`, `main/apps`, `main/book`… giữ liên kết tương thích, không phải bản sao độc lập. Lõi đọc và phần chung sửa một lần cho hai bản; thư viện/giao diện riêng sửa trong thư mục chủ sở hữu. Không sửa nguồn được sinh trong `build/`.

Xem [cây tính năng Love Book](LOVE_BOOK_FEATURES.md) để tra vị trí các chức năng.

Love Book không dùng PIN khi mở máy hoặc đánh thức. Mã PIN đã lưu chỉ được bản Full sử dụng.
