# Sao lưu và khôi phục dữ liệu đọc

Tại **Cài đặt → Sao lưu & Khôi phục cấu hình**, sao lưu thủ công tạo
`/sdcard/dpReader-settings.backup`. Định dạng hiện tại chứa các trường cấu hình
hệ thống/đọc được bộ xuất hỗ trợ, thông tin Wi-Fi và các bản ghi đọc: tiến trình,
thời gian, đánh dấu, yêu thích, tên sách và trạng thái ẩn khỏi thư viện. Đây không
phải bản sao toàn bộ NVS hoặc toàn bộ thiết bị; không chứa firmware, sách, font,
ảnh, cache trang, VCOM hay thông tin hiệu chuẩn phần cứng. Không nên coi đây là
công cụ sao chép mọi thiết lập giữa hai dòng máy.

Bản sao có thông tin Wi-Fi dạng rõ; giữ riêng tư khi chép sang máy tính.

## Ghi và giữ bản trước

Bộ ghi tạo file `.backup.tmp`, hoàn tất `fflush`/`fsync` và đóng file trước khi
đổi tên. Bản chính trước đó được giữ tại `dpReader-settings.backup.previous`.
Lỗi ghi không thay bản chính; lỗi đưa file tạm thành bản chính sẽ thử trả lại tên
cũ. Nếu chỉ còn bản `.previous`, lượt ghi tiếp theo giữ bản đó cho đến khi có bản
chính mới. Việc đổi tên trên FAT không bảo đảm giao dịch nguyên tử khi mất nguồn;
chép bản sao quan trọng sang máy tính vẫn hữu ích.

Trên S31, workspace xuất/nhập cấu hình, buffer luồng đọc/ghi và vùng chứa dữ liệu
của từng bản ghi lịch sử khi xuất/nhập dùng PSRAM rõ ràng, không tự chuyển sang SRAM nội khi
thiếu PSRAM. Cấp phát thất bại dừng lượt sao lưu. Không tạo task hay tăng cấu hình
stack. NVS và hệ thống file vẫn có thể cấp phát SRAM nội tạm thời.

## Điểm tự sao lưu trên S31

Tại **Cài đặt → Cập nhật & Khôi phục → Sao lưu & Khôi phục**, công tắc
**Tự động sao lưu** mặc định bật khi chưa có lựa chọn đã lưu. Tắt/bật được lưu
trong NVS và giữ sau reboot. Tắt chỉ ngừng các điểm sao lưu tự động; hai nút sao
lưu và khôi phục thủ công vẫn hoạt động.

`CONFIG_DPREADER_AUTO_BACKUP=y` được bật trong bản S31 để công tắc thực sự điều
khiển cơ chế sao lưu. Không tạo task mới, dữ liệu/buffer mới vẫn dùng PSRAM. Việc
bật mặc định không đồng nghĩa đã chứng minh đỉnh SRAM không tăng: mức sử dụng cao
nhất lúc chạy thật vẫn cần đo trên thiết bị.

Khi bật, chỉ có hai thời điểm tự sao lưu:

- **Trước Sleep:** sau khi lưu tiến trình và hiển thị màn khóa, khi máy chuẩn bị
  ngủ thực sự; ghi trước khi tạm dừng thẻ/font/sách. Áp dụng light sleep và deep
  sleep. Bỏ qua nếu USB, công việc nền, thẻ hoặc tiến trình chưa sẵn sàng. Không
  tự khôi phục thẻ chỉ để ghi bản sao.
- **Trước cập nhật firmware:** trước cài gói TF hợp lệ hoặc bắt đầu tải/ghi OTA.
  Chỉ kiểm tra phiên bản mới không tạo bản sao.

Đóng sách, chuyển trang, rời Cài đặt, reboot và tắt máy trực tiếp không tự tạo bản
sao. Các bước lưu tiến trình vào NVS vốn có vẫn giữ nguyên. Khóa màn hình khi USB
hoặc điều kiện khác ngăn Sleep cũng không tự ghi bản sao. Các lần thức định kỳ
trong cùng lượt light sleep không gọi lại bước sao lưu.

Không chạy theo từng lần lật trang, không tự sao lưu ở vòng lặp nền. Khi tắt công
tắc, checkpoint trả về trước khi kiểm tra thẻ hoặc cấp phát buffer. Trong cùng
phiên khởi động, bỏ ghi file nếu số lần commit thiết lập, phiên bản tiến trình và
thông tin Wi-Fi không đổi. Các bản ghi lịch sử khác được chụp cùng lượt sao lưu;
thay đổi riêng các bản ghi đó chưa có bộ theo dõi thay đổi độc lập. Sau khởi động lại, nếu công tắc đang bật, điểm kiểm tra đủ điều kiện đầu tiên tạo
lại bản sao. Thiếu thẻ/PSRAM hoặc lỗi
ghi không đánh dấu dữ liệu đã được sao lưu; điểm kiểm tra sau có thể thử lại.

NVS khởi tạo/mở/ghi lỗi sẽ chặn tự sao lưu. Nếu phát hiện namespace thiết lập
không tồn tại trước bước tạo giá trị mặc định, firmware ghi cờ cách ly; cờ còn
hiệu lực sau khởi động lại để mặc định không ghi đè bản sao cũ trên thẻ. Khi chỉ bật tự sao lưu, một lượt **sao lưu hoặc khôi phục thủ công thành công**
mới cho phép tự sao lưu trở lại. Khi bật tự phục hồi, trạng thái chờ/gián đoạn
chặn cả thao tác ghi bản sao thủ công để bảo vệ bản cũ; cần khôi phục thành công
trước khi sao lưu tiếp.
Công tắc vẫn có thể hiển thị bật trong trạng thái cách ly, nhưng chú thích sẽ
báo tạm dừng và yêu cầu sao lưu/khôi phục thủ công một lần. Bật công tắc không bỏ
qua chốt bảo vệ. Cờ này không phát hiện được mọi trường hợp NVS bị xóa một phần
hoặc dữ liệu hỏng nhưng vẫn đọc được.

Cơ chế hiện tại không tự báo hoàn thành trên giao diện và không bảo đảm sao lưu
xong trước một lần nạp USB từ máy tính. Đóng sách/khóa máy trước khi nạp; với dữ
liệu quan trọng, dùng sao lưu thủ công và giữ một bản trên máy tính.

## Khôi phục tự động sau reboot

`CONFIG_DPREADER_AUTO_RESTORE` vẫn mặc định **tắt**. Công tắc Tự động sao lưu
không bật tự khôi phục sau reboot; khôi phục đang dùng thao tác thủ công. Nhánh
tự khôi phục bên dưới chỉ áp dụng khi được bật riêng trong cấu hình.

Bản sao mới dùng định dạng V30 (`PICOSETU`), chứa nhận diện S31, MAC nhà máy và
lựa chọn công tắc tự sao lưu, giao diện đơn giản và hai shortcut riêng cùng lựa chọn viền/nhãn kèm checksum. V29 (`PICOSETT`), V28 (`PICOSETS`), V27 (`PICOSETR`), V26 (`PICOSETQ`) và các định dạng cũ vẫn
nhập được; bản trước V27 giữ nguyên lựa chọn công tắc hiện tại. MAC chỉ được đọc, không ghi eFuse. Firmware hiện tại vẫn nhập thủ công được các bản sao cũ; firmware cũ chưa hỗ trợ
V30 sẽ không đọc được bản sao mới. Lựa chọn giao diện đã có trên máy luôn được giữ, kể cả khi bản sao ghi tắt hoặc là định dạng cũ. Chỉ máy chưa có lựa chọn mới lấy cờ giao diện hợp lệ từ bản sao; cờ thử nghiệm trước V28 không được dùng.
Bản `.previous` được giữ khi ghi thành công; nên giữ bản cũ riêng nếu cần hạ ROM.
Nhận diện MAC giúp tránh nhập nhầm thiết bị, không phải chữ ký chống sửa đổi.

Khi bật, trước lần nạp font/trang đầu:

1. Nếu thiết lập NVS còn tốt, bỏ qua nhập bản sao. Update/reboot bình thường không
   đưa tiến trình về bản cũ.
2. Chỉ cho tự phục hồi khi đã ghi nhận namespace thiết lập bị mất và toàn bộ các
   namespace lịch sử đọc không còn bản ghi. Dữ liệu thiếu một phần hoặc lịch sử
   còn sống được giữ để người dùng xử lý thủ công.
3. Chờ khởi tạo thẻ tối đa khoảng 3 giây. Thiếu/chưa sẵn sàng thẻ thì mở Cài đặt
   với thông báo; không lặp thử trong cùng phiên. Có thể lắp thẻ rồi reboot lại.
4. Kiểm tra toàn bộ định dạng/checksum và MAC. Bản chính không phù hợp thì thử
   `.previous`. Bản cũ chưa có MAC hoặc thuộc máy khác chỉ nhập được thủ công.
5. Ghi dấu bắt đầu vào NVS trước khi nhập dữ liệu. Chỉ đánh dấu hoàn tất sau khi
   thiết lập, lịch sử và Wi-Fi đều báo thành công; đọc lại cỡ chữ, đường dẫn font
   đọc/hệ thống và thông tin Wi-Fi trước khi chốt. Nạp lại thiết lập cho phiên
   chạy hiện tại, không tự reboot.
6. Dấu đang nhập hoặc lỗi còn lại sau reboot sẽ chặn tự thử tiếp, chặn sao lưu và
   mở Cài đặt với thông báo yêu cầu khôi phục thủ công. Không xóa NVS để cứu lỗi.

Nếu file font không còn trên thẻ, tự phục hồi vẫn giữ đường dẫn đã chọn; bộ nạp
font dùng cơ chế dự phòng hiện có. Không lấy tình trạng thiếu font tạm thời làm
lý do xóa lựa chọn trong bản sao.

## Khôi phục thủ công và giới hạn

Người dùng vẫn chọn **Khôi phục cấu hình** trong Cài đặt. Thao tác này kiểm tra
bản sao trước khi ghi và cho phép nhập bản cũ/khác thiết bị một cách chủ động.
Nếu bản chính không mở được, bộ đọc thử `.previous`; nếu bản chính mở được nhưng
hỏng, thao tác thủ công không tự thay bằng bản cũ. Giữ các file và chọn/chép bản
dự phòng phù hợp thành bản chính trước khi thử lại.

Font, ảnh nền và sách không nằm trong bản sao. Tiến trình hiện đối chiếu cả
đường dẫn và kích thước sách; đổi tên/chuyển thư mục/thay nội dung có thể khiến
bản ghi không được nhận lại.

Các namespace được ghi qua nhiều commit. Dấu tiến trình ngăn phục hồi lặp và
ngăn báo hoàn tất khi có lỗi; **chưa có rollback giao dịch toàn bộ NVS**. Mất
nguồn giữa chừng có thể để lại dữ liệu đã phục hồi một phần. Không rút thẻ/tắt
máy trong quá trình; khi bị chặn, dùng khôi phục thủ công để nhập lại bản đã kiểm
tra. NVS không khởi tạo/đọc được cần xử lý riêng, không tự erase.

## Kiểm tra trên máy tính

Chạy tại thư mục gốc:

```sh
python3 tools/auto_backup_host_test.py
```

Lệnh tạo source view S31 riêng trong `build/`, chạy kiểm tra checkpoint, giữ bản
trước, lỗi cấp phát/ghi/đổi tên, thử lại, phát hiện thay đổi Wi-Fi, cách ly qua
khởi động lại và vòng sao lưu/khôi phục lịch sử với UndefinedBehaviorSanitizer. Test tự phục
hồi kiểm tra đúng/sai MAC, bản cũ, bản dự phòng, lịch sử còn sống, thiếu thẻ,
thiếu PSRAM, lỗi nhập/đọc lại, lỗi chốt journal và gián đoạn qua reboot.
Không đọc/ghi thiết bị thật.


## Dữ liệu theo cuốn trên S31

Từ `1.4.0`, firmware tạo `/sdcard/.dpreader`, đặt thuộc tính Hidden trên FAT,
và lưu tiến độ theo đường dẫn sách trong `books/` (chia 16
thư mục con để giới hạn số tệp mỗi thư mục). Tên tệp dùng hash 64-bit; bản ghi
vẫn chứa đường dẫn đầy đủ để từ chối ghi đè khi xảy ra va chạm. Sách trong
`/flash/books/` dùng `/flash/.dpreader` trên vùng lưu trữ nội bộ đã có.

Mỗi bản ghi có hai slot luân phiên, số thứ tự và checksum; ghi slot kế tiếp,
flush và đồng bộ trước khi báo thành công. Bản hợp lệ mới nhất được đọc, bản
hỏng bị loại. Cách này giữ bản trước khi một tệp ghi dở; không bảo đảm khôi phục
nếu toàn bộ hệ thống file hoặc cả hai slot hỏng. Thẻ lỗi/đầy trả lỗi lưu, không
xóa NVS hay tự định dạng. Các bộ đệm kiểm tra dùng PSRAM, cấp phát có giới hạn.

Từ `1.4.4`, font, cỡ chữ, độ đậm, khử răng cưa, khoảng cách, căn lề,
hình nền, hiển thị ảnh, font nhúng, thanh trạng thái và chân trang dùng chung.
Khi nâng cấp, bộ đọc dùng cài đặt chung đang lưu trên máy. Các tệp trình bày
cũ trên thẻ được giữ để tương thích với bản sao lưu và firmware cũ.
Đổi đường dẫn hoặc đổi kích thước tệp không tự mang theo tiến độ cũ.

Lật trang tiếp tục cập nhật tiến độ trong RAM; cơ chế lưu trì hoãn lúc rảnh,
đóng sách và khóa máy được giữ nguyên. Chỉnh trình bày ghi khi thay đổi lựa chọn.
Dấu vết khởi động/mở sách nằm trong bản ghi phiên riêng, được đọc sau khi thẻ
sẵn sàng, không ghi từng trang. Khi không có thẻ lúc khởi động, phiên dùng bộ
nhớ trong; nếu nơi lưu phiên mất kết nối thì không cho mở sách trước khi ghi
được checkpoint bảo vệ. Không đổi bảng phân vùng hoặc kích thước NVS.

Tiến độ NVS và snapshot `.dpreader-progress/progress.bin` cũ được đọc khi chưa
có bản ghi mới; các lượt lưu tiếp theo ghi theo cuốn, không xóa dữ liệu nguồn.
Sao lưu xuất tiến độ có hiệu lực và các bản ghi trình bày cũ còn trên thẻ, dùng phần lịch sử
`RPHIST2`; firmware mới tiếp tục nhập `RPHIST1`. Firmware cũ không nhập được bản
sao chứa phần lịch sử mới. Khôi phục là thay thế thư viện được lưu, không gộp;
dữ liệu cũ ngoài bản sao được che bằng bản ghi xóa. Thao tác khôi phục nhiều tệp
không phải một giao dịch nguyên tử: nếu gián đoạn cần chạy khôi phục lại.
Reset cấu hình hệ thống xóa checkpoint phiên nhưng giữ thư viện theo cuốn.

Kiểm thử host: materialize bằng `tools/board_source_view.py --board metalio47
--tests --output build/host/metalio47`, rồi chạy trong cây đó các script
`tools/book_state_files_host_test.py`, `tools/book_profile_host_test.py`,
`tools/boot_file_state_host_test.py` và `tools/book_history_files_host_test.py`.
Tốc độ và độ trễ ghi thực tế phụ thuộc thẻ, cần đo trên thiết bị; các kiểm thử
host không thay thế kiểm tra mất nguồn hoặc lật trang trên máy.
