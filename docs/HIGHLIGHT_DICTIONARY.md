# Tô sáng, ghi chú và từ điển offline

Bản thử trên nhánh `codex/Test-highlight-dictionary` hỗ trợ TXT/EPUB có chữ ngang. Nhấn giữ một từ trong phần nội dung để mở menu, tiếp tục giữ và kéo để mở rộng vùng chọn trong trang hiện tại. Dấu gạch chân cho biết đoạn được chọn; **Tô sáng** lưu dấu này. **Ghi chú** mở bàn phím, nhập tối đa 127 byte UTF-8 rồi chạm **Lưu ghi chú**. Chữ Việt có dấu có thể chiếm nhiều byte.

**Các ghi chú** liệt kê tối đa 32 đoạn trong chương hiện tại. Chạm một mục để về trang chứa đoạn đó; chọn **Ghi chú** để sửa, hoặc **Xóa** trong danh sách. **Đóng** trở về đọc. Khi menu đang mở, phím lật trang đóng menu trước; điện thoại/điều khiển từ xa không xuyên qua vùng chọn. Nhấn nguồn vẫn đi đường khóa màn hình. Bản nháp chưa lưu được bỏ khi đóng bàn phím, khóa, rút thẻ hoặc rời sách.

Nhấn giữ chữ ngang thay cho thao tác mở mục lục cũ. Mục lục vẫn có trong công cụ đọc. Chữ dọc và trang chỉ có hình chưa hỗ trợ chọn chữ; thao tác ảnh hiện có được giữ. Chưa hỗ trợ kéo chọn xuyên trang/chương, tay nắm hai đầu, xuất sang KOReader hoặc đồng bộ ghi chú.

## Lưu trên thẻ nhớ

Từ RC6, danh sách, ghi chú, highlight và tệp trao đổi đều nằm trong **`/dpReader_note/`** ở gốc thẻ. Mỗi sách có thư mục riêng theo đường dẫn tương đối trên thẻ, gồm cả phần mở rộng:

```
Sách: /books/VanHoc/TenSach.epub
Ghi chú:
/dpReader_note/books/VanHoc/TenSach.epub/snapshots/
/dpReader_note/books/VanHoc/TenSach.epub/dpReader.annotations.json
```

`snapshots/` chứa bản lưu đọc trực tiếp; JSON phục vụ trao đổi KOReader. Hai sách cùng tên ở các thư mục khác nhau, hoặc bản TXT/EPUB cùng tên, không dùng chung dữ liệu. Không tạo thư mục `.sdr` mới để lưu ghi chú. Khi đổi tên/di chuyển sách, cần chuyển thư mục ghi chú tương ứng để giữ liên kết.

Mở lại từng chương sẽ tự sao chép ghi chú RC3–RC5 từ `.sdr/kiikoread/`, hoặc dữ liệu RC2 từ `/.kiikoread-notes`, khi chương chưa có bản lưu ở nơi mới. Tệp JSON cũ trong `.sdr` cũng được đọc/chuyển nếu chưa có JSON mới. Các tệp cũ giữ nguyên; một khi có dữ liệu mới sẽ không nhập đè bằng bản cũ. Nếu dữ liệu cũ hỏng/không đọc được, chức năng lưu dừng để tránh mất nội dung. Việc chuyển diễn ra theo chương được mở, không quét tất cả sách khi khởi động.

Vị trí highlight dựa vào chương, nội dung UTF-8 đã phân tích và mã nhận dạng văn bản, nên đổi font/cỡ chữ/căn lề vẫn giữ đúng đoạn. Sửa nội dung sách có thể tạo bộ ghi chú mới; dữ liệu cũ được giữ, không tự gắn vào đoạn khác.

Mỗi chương có hai bản lưu luân phiên kèm số thế hệ/checksum. Bản ghi dở được phục hồi từ bản trước; nếu không còn bản hợp lệ sẽ báo lỗi. Ghi lỗi giữ bản đã lưu và bản nháp để thử lại. Có thể sao lưu nguyên thư mục `dpReader_note` trên máy tính.

## Cài từ điển

Tạo thư mục `/dict` ở gốc thẻ. Từ RC7, chép trực tiếp các tệp cùng tên gốc vào `/dict`, không cần chạy Python hoặc giải nén `.dict.dz`/`.idx.gz` trước. Ví dụ:

```
/dict/anh-viet/anh-viet.ifo
/dict/anh-viet/anh-viet.idx      (hoặc anh-viet.idx.gz)
/dict/anh-viet/anh-viet.dict.dz  (hoặc anh-viet.dict)
/dict/anh-viet/anh-viet.syn    (nếu có)
```

Có thể đặt trực tiếp trong `/dict` hoặc trong một lớp thư mục con. Bộ đọc không tự tải từ điển hoặc gửi văn bản ra mạng.

Hỗ trợ StarDict 2.4.2/3.0.0, offset 32 bit và `sametypesequence` gồm tối đa tám trường `m`, `h`, `g`, `x`, `t`. Trường markup được bỏ thẻ và giải mã một số entity thông dụng để hiển thị văn bản. Có tra `.syn`; giữ dấu tiếng Việt, chỉ bỏ khác biệt hoa/thường ASCII. Chọn cụm nhiều âm tiết bằng kéo. Chưa có fuzzy search, phân từ CJK, biến thể hình thái hoặc chuẩn hóa NFC/NFD.

Mỗi đợt tra quét tối đa 4096 bản ghi, trên thiết bị nhường lại giao diện sau khoảng 8 ms giữa các nhóm 16 bản ghi; giao diện có thể hủy bằng **Đóng**. Bộ lớn có thể chậm do quét tuần tự, chưa có chỉ mục tìm nhanh. Định nghĩa giới hạn 8191 byte, dài hơn sẽ báo rút gọn. **Trang trước/Trang sau** đọc phần kết quả tiếp theo, **Từ điển kế** tìm trong bộ tiếp theo. Thứ tự các bộ hiện theo thứ tự thư mục trên thẻ; chưa có mục ưu tiên riêng.

Firmware đọc `.idx.gz` theo luồng; `.dict.dz` dictzip có bảng RA được đọc ngẫu nhiên theo khối. Chỉ giải nén khối chứa phần định nghĩa cần xem, không tạo tệp giải nén/cache trên thẻ. Nếu `.dict.dz` là gzip thông thường không có RA, firmware đọc tuần tự từng đợt tới vị trí cần tra (có thể chậm hơn). `.idx` và `.dict` không nén vẫn dùng được. Nếu có đồng thời bản không nén và bản nén cùng tên, ưu tiên bản không nén; nên chép một bộ nhất quán để tránh dùng bản cũ.

Với bộ bạn đang dùng, cần đủ các tệp **cùng tên gốc**:

```
/dict/en-vi/tudien-stardict-en-vi-20260411.ifo
/dict/en-vi/tudien-stardict-en-vi-20260411.idx     (hoặc .idx.gz)
/dict/en-vi/tudien-stardict-en-vi-20260411.dict.dz
```

Không đổi đuôi `.dict.dz` thành `.dict`. Nếu tải về là ZIP/TAR, vẫn cần mở gói ngoài để lấy bộ tệp; firmware chưa tự mở gói phân phối. `.syn` tùy chọn phải cùng tên gốc. Chép xong mở lại menu Tra từ để quét bộ mới. **Đóng** hủy và đóng các tệp đang đọc.

Giới hạn hiện tại: index 32 bit, StarDict 2.4.2/3.0.0, trường `sametypesequence` gồm `m/h/g/x/t`; gzip một member, dictzip RA phiên bản 1. Phần sau giải nén tối đa 512 MiB cho mỗi tệp nén. Giữ nguyên giới hạn định nghĩa 8191 byte; vượt giới hạn sẽ báo rút gọn. Chưa hỗ trợ MDX/MDD, DSL, index 64 bit hoặc tài nguyên ảnh/âm thanh nhị phân. `prepare_stardict.py` vẫn có thể dùng tùy chọn trên máy tính để kiểm tra/chuyển bộ cũ, nhưng không cần cho việc tra trực tiếp.

Bộ nén dùng bộ đệm vào/ra 4 KiB, bộ nhớ zlib có hạn mức 64 KiB cho mỗi luồng; dictzip giữ tối đa một khối dưới 64 KiB và bảng offset theo header (tối đa khoảng 128 KiB). Cấp phát mới ở PSRAM trên S31; không tải toàn bộ từ điển vào RAM. Mỗi bước đọc định nghĩa xử lý tối đa một khối RA hoặc 32 KiB dữ liệu gzip theo luồng. Chỉ mục thường được đọc theo khối 4 KiB thay vì gọi đọc tệp từng byte. Chỉ mục từ vẫn quét tuần tự từng đợt, nên bộ lớn có thể tra chậm; chưa có chỉ mục tìm nhanh/fuzzy.

Header/độ dài/chỉ mục/ranh giới chunk và giải nén được kiểm tra; gzip theo luồng kiểm CRC khi đi đến cuối luồng. Tra ngẫu nhiên dictzip không quét toàn bộ tệp để xác minh CRC tổng trước mỗi từ; nên dùng bộ dữ liệu có checksum từ nguồn tải để kiểm tra bản chép nếu cần.

Nguồn có thể đánh giá: [FreeDict](https://freedict.org/downloads/), [WikDict](https://www.wikdict.com/) hoặc các bản StarDict từ Wiktionary có nguồn và giấy phép rõ ràng. Khả năng đọc định dạng không đồng nghĩa đã kiểm chứng mọi bộ cụ thể. Trên host đã thử bộ FreeDict Anh–Pháp 0.1.6, 8799 mục, trường `h`, giữ nguyên `.idx.gz`/`.dict.dz` (RC7), cùng các kiểm thử chéo khối và lỗi dữ liệu; chưa kiểm chứng đúng bộ Anh–Việt của người dùng hoặc nghiệm thu bộ Anh–Việt lớn hoặc thời gian tra trên S31. Không đóng gói các bộ Oxford/Longman/Cambridge/Lạc Việt không rõ quyền sử dụng.

## Kiểm tra trên máy tính

```sh
python3 tools/run_book_notes_tests.py
python3 tools/board_source_view.py --board metalio47 --tests --output build/notes-host
```

Suite đầu kiểm tra vị trí glyph thực trong các hàm vẽ font, luồng chọn/kéo/sửa/xóa/hủy, phục hồi bản ghi dở, StarDict/synonym, trường markup, UTF-8 sai và giới hạn kích thước. Các bài kiểm tra gesture/layout hiện có chạy trong thiết bị view theo quy trình của kho. Chưa có tuyên bố nghiệm thu chạm, tốc độ, mất điện thật hoặc độ rõ màn hình trên thiết bị.

## Trao đổi hai chiều với KOReader

Menu có icon; trong khi giữ/kéo chỉ hiện vùng chọn. Thả tay mới hiện menu tránh đoạn đang chọn. **Tô sáng** lưu thành công rồi đóng menu; **Xong** trên bàn phím tự lưu ghi chú và đóng khi thành công. Dấu highlight được vẽ lại cả khi lật trang dùng bộ đệm. Chạm ngoài bảng để đóng danh sách. Giữ chữ/giữ vùng trống không mở mục lục; mục lục nằm trong công cụ đọc.

Cài `integrations/koreader/dpreader-sync.koplugin` vào `koreader/plugins/`, rồi khởi động lại KOReader. **Thay plugin cũ nếu đang cài**, không giữ hai plugin đồng bộ cùng lúc. Mở sách trên dpReader trước để tạo thư mục ghi chú và danh mục chương. Chép **sách theo cùng đường dẫn tương đối và nguyên thư mục `dpReader_note` ở gốc thẻ/bộ nhớ sách** sang thiết bị KOReader. Ví dụ cả hai phía đều có `/books/VanHoc/TenSach.epub` và `/dpReader_note/books/VanHoc/TenSach.epub/`.

Mở sách trong KOReader, chọn **dpReader: đồng bộ ghi chú**. Plugin tìm `dpReader_note` từ các thư mục cha của sách, nhập vào annotation gốc và xuất chỉnh sửa về `dpReader.annotations.json`. Chép thư mục `dpReader_note` trở lại gốc thẻ dpReader; mở lại chương hoặc chọn **Các ghi chú → Đồng bộ**. Đây là đồng bộ tệp thủ công; đóng sách trước khi chép, không chỉnh đồng thời hai bản. Plugin vẫn đọc tệp trao đổi `.sdr` cũ khi chưa thấy tệp ở nơi mới.

Tệp JSON giữ `text`, `note`, `drawer`, `datetime`, `datetime_updated`, định vị riêng `kiikoread` (tên trường cũ để tương thích dữ liệu) và `koreader`. Plugin nhờ KOReader tạo XPointer thật qua đối chiếu nguyên đoạn trích; không giả vị trí từ offset byte và không ghi đè thủ công `metadata.*.lua` của KOReader.

Giới hạn: chỉ EPUB/TXT dùng CRE; đoạn trích chưa liên kết phải khớp duy nhất. Annotation mới từ KOReader cần tên chương duy nhất khớp danh mục dpReader (sách một chương có thể không có tên). Đoạn trùng/chương không khớp/nội dung thay đổi/ghi chú quá 127 byte và xung đột được giữ trong JSON, không tự chọn vị trí hoặc cắt nội dung. Xóa dùng dấu `deleted`, chỉ tác động các mục đã liên kết. Mỗi tệp tối đa 128 mục/128 KiB; firmware tối đa 32 mục/chương, chọn trong một trang, chữ ngang.

Đã kiểm thử mã C và module Lua thật qua API CRE giả lập cho tạo/sửa/xóa hai chiều, xung đột, đoạn trùng, phục hồi, đường dẫn trung tâm và chuyển dữ liệu cũ. Chưa nghiệm thu ứng dụng KOReader/S31 thật.

Chữ menu 23 px, danh sách 29/27 px, xem trước tối đa sáu từ; nội dung lưu và trình sửa vẫn đầy đủ. **Thống kê → Danh sách ghi chú** mở chương hiện tại mà không cần chọn chữ trước.

Bảng từ điển và danh sách ghi chú có viền đen. Chạm ngoài bảng để đóng; thanh công cụ ở phía trên vẫn nhận thao tác. Đóng bảng từ điển khi đang tra sẽ hủy tra và đóng các tệp/thư mục đang mở.

Nội dung định nghĩa dùng chữ 32 px, tối đa 10 dòng/trang. Danh sách ghi chú giữ chữ 29/27 px, thu gọn mỗi mục xuống 80 px và hiển thị tối đa 7 mục/trang; chỉ xem trước sáu từ đầu, không rút gọn dữ liệu đã lưu.

### Thêm từ điển bằng trang chuyển tệp local

Khi gắn thẻ thành công, firmware tạo sẵn `/dict` nếu chưa có; không cài sẵn bộ từ điển và không ghi đè dữ liệu cũ. Mở trang chuyển tệp theo địa chỉ hiển thị trên thiết bị, chọn **Từ điển**. Chọn folder chứa trọn bộ `.ifo`, `.idx`/`.idx.gz`, `.dict`/`.dict.dz` cùng tên, có thể kèm `.syn`, rồi **Tải từ điển vào thẻ nhớ**. Trên điện thoại không chọn được folder, nhập tên thư mục và chọn đủ các tệp thay thế. Không cần giải nén `.dict.dz` hay chạy Python.

Tệp được lưu vào `/dict/<tên folder>/`. Chỉ dùng một cấp thư mục con; có thể chọn folder `dict` chứa nhiều bộ ở cấp đó. Tệp phụ như README được bỏ qua. Trang kiểm tra đủ bộ, tên/đường dẫn và tổng dung lượng trước khi gửi; `.ifo` được gửi cuối cùng. Mỗi tệp vẫn được lưu bằng luồng truyền hiện có và hỏi trước khi ghi đè. Nếu mất mạng hoặc lỗi thẻ giữa chừng, một phần bộ có thể đã lưu: chọn lại đủ bộ để thử lại; đây không phải giao dịch nguyên tử cho cả folder. **Mở thư mục dict** chuyển sang trình quản lý tệp để xem/tạo/xóa thư mục theo nhu cầu.
