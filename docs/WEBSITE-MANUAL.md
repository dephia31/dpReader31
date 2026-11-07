# Hướng dẫn trực quan và tìm kiếm trên website dpReader

Hướng dẫn cũ được phục vụ tại `https://kiikoread.com/manual/`. Ô hỏi ở trang chủ và trang hướng dẫn dùng chung chỉ mục thao tác. Giữ các liên kết chương đã chia sẻ, đường chuyển hướng từ `manual.html` và URL PDF `manual/kiikoread-manual.pdf` để liên kết cũ tiếp tục hoạt động.

## Nguồn và cách tạo

- `docs/user-manual-detailed.zh-CN.json` là nguồn hiện tại của các chương, đường dẫn thao tác, các bước, kết quả và lưu ý cho bản tiếng Trung.
- `tools/build_detailed_manual.py` tạo trang web, chú thích hình, chỉ mục tìm kiếm và PDF tùy chọn.
- `flash/manual/assets/` chứa hình minh họa dựng từ các hàm vẽ của firmware. Số màu xanh trên ảnh là chú thích hướng dẫn.
- `flash/assets/manual-search.js` quản lý từ đồng nghĩa và quy tắc tìm kiếm.
- `flash/assets/manual-helper.js` kết nối ô hỏi, ô tìm kiếm và liên kết đến thao tác.

Sau khi cập nhật nội dung, dùng bộ hình hiện có để tạo lại trang và chạy kiểm thử tìm kiếm.

```sh
python3 tools/build_detailed_manual.py --assets flash/manual/assets --output flash/manual --website
node tools/manual_search_test.js
```

Mỗi thao tác mới cần một `id` duy nhất và ổn định để giữ liên kết đã chia sẻ. Cập nhật hình minh họa tương ứng trước khi thay ảnh. Với PDF, dùng `--pdf --font <đường-dẫn-phông>`, kiểm tra trang và ký tự rồi sao chép đến đường dẫn PDF công khai. Công cụ `tools/stage_public_flash.py` kiểm tra tài nguyên cục bộ, manifest, nguồn Release, SHA-256 và tương thích OTA.

## Tìm kiếm

Chỉ mục hỗ trợ từ khóa, cách hỏi thông thường, từ đồng nghĩa và một số lỗi đánh máy. Kết quả phù hợp nhất hiển thị ba bước đầu. Liên kết theo ID mở đầy đủ bước, lưu ý và hình minh họa. Ô hỏi và ô tìm kiếm dùng cùng chỉ mục và thứ tự kết quả.

Trình duyệt tải chỉ mục khi cần rồi dùng lại trong bộ nhớ. Tìm kiếm chạy cục bộ, dùng câu trả lời từ tài liệu và không lưu câu hỏi. Khi không tìm thấy, giao diện hiển thị trạng thái và liên kết tới toàn bộ hướng dẫn. Có thể thử lại nếu tải lỗi. Kết quả của yêu cầu cũ được bỏ qua khi người dùng đổi câu hỏi.

`tools/manual_search_test.js` kiểm tra các cách hỏi chính, lỗi đánh máy, câu hỏi không liên quan và liên kết đến thao tác. Trước khi triển khai, kiểm tra ô hỏi, tìm kiếm, chuyển trang, bố cục điện thoại và tải PDF trên trình duyệt.
