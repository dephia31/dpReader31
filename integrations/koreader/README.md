# dpReader ↔ KOReader

Chép `dpreader-sync.koplugin` vào `koreader/plugins/`, thay plugin đồng bộ cũ rồi khởi động lại KOReader. Không cài đồng thời hai bản plugin.

1. Mở sách trên firmware RC3 để tạo `/dpReader_note/<đường dẫn tương đối của sách>/`.
2. Chép sách theo cùng đường dẫn tương đối và thư mục `dpReader_note` ở gốc bộ nhớ sách sang thiết bị KOReader.
3. Mở sách, chọn **dpReader: đồng bộ ghi chú** trong menu KOReader.
4. Chép `dpReader_note` trở lại gốc thẻ dpReader; mở lại chương hoặc chọn **Các ghi chú → Đồng bộ**.

Tệp chung: `dpReader_note/<đường dẫn sách>/dpReader.annotations.json`; các bản cũ có đuôi `.old`. KOReader lưu các mục đã nhập bằng API annotation và metadata của chính nó. Không chép đè `metadata.*.lua` thủ công. Firmware giữ snapshot riêng trong `dpReader_note/<đường dẫn sách>/snapshots/`. RC2–RC5 được chuyển theo chương khi mở lại; tệp cũ giữ nguyên. Plugin vẫn đọc `.sdr/kiikoread.annotations.json` cũ khi chưa thấy tệp trung tâm. Chỉ chép sau khi đóng sách, tránh chỉnh đồng thời.

Chỉ hỗ trợ sách EPUB/TXT dùng CRE. Vị trí được đối chiếu nguyên đoạn trích hoặc XPointer đã được xác minh; đoạn xuất hiện nhiều lần chưa liên kết sẽ không tự gắn. Annotation mới từ KOReader cần tên chương duy nhất trong danh mục firmware; sách một chương có thể không có tên chương. Không hỗ trợ PDF, ảnh hoặc tự đổi định vị khi nội dung sách thay đổi.

Tạo/sửa/xóa hai chiều được kiểm tra bằng module Lua thật và mã C thật với API CRE giả lập. Chưa nghiệm thu KOReader/S31 thật. Những mục không khớp, ghi chú quá 127 byte hoặc xung đột được giữ trong JSON và báo số lượng, không tự ghi đè lựa chọn của người dùng. Xung đột hiện cần chọn một nội dung trên hai phía và đồng bộ lại; có thể sao lưu JSON và xử lý bản ghi `conflict` thủ công, không xóa bản cũ trước khi đối chiếu.

Kiểm thử: `python3 tools/run_book_notes_tests.py`; cài `lupa` và thư viện phát triển cJSON hoặc đặt `CJSON_DIR`, rồi chạy `python3 tools/koreader_notes_roundtrip_test.py`.

Mã cầu nối độc lập Apache-2.0; không chứa mã KOReader. API đối chiếu: `frontend/document/credocument.lua`, `frontend/apps/reader/modules/readerannotation.lua`, `frontend/docsettings.lua` trong kho [KOReader](https://github.com/koreader/koreader).

Ví dụ: sách `/books/demo.epub` đi cùng `/dpReader_note/books/demo.epub/`. Giữ phần mở rộng để tách TXT/EPUB, và giữ đường dẫn thư mục để phân biệt sách cùng tên. Chép cả thư mục trung tâm, không chỉ tệp JSON, khi sao lưu/đổi thiết bị.
