# Bàn giao công việc dpReader

Tài liệu này quy định quy trình và mẫu bàn giao. Sổ công việc đang chạy nằm tại `docs/HANDOFF.local.md`. Tệp này bị Git bỏ qua và chỉ lưu trên máy hiện tại.

## Tiếp quản

1. Nếu chưa có sổ cục bộ, bắt đầu từ [hướng dẫn cho người mới](ONBOARDING.md).
2. Nếu có, đọc mục mới nhất. Đối chiếu danh sách tệp với `git status` và `git diff --stat`, rồi đọc các khối `Frozen` trong mã liên quan. Ghi một mục tiếp quản ở đầu sổ.
3. Nếu mục mới nhất đã cũ so với trạng thái Git, thư mục build hoặc lịch sử Terminal, dựng lại tình trạng từ bằng chứng hiện có. Ghi rõ ai dựng lại và đánh dấu các phần chưa xác định.

## Bàn giao

Khi đổi người phụ trách, kết thúc phiên hoặc hoàn thành một mốc, chép mẫu dưới đây vào đầu `docs/HANDOFF.local.md`. Nếu chưa có tệp, tạo tệp với tiêu đề “Sổ công việc cục bộ dpReader”.

```markdown
## YYYY-MM-DD HH:MM · Người phụ trách · Tên công việc

- **Nhánh và commit nền** — <branch> @ <short-sha> từ git rev-parse --short HEAD
- **Mục tiêu** — …
- **Đã hoàn thành** — …
- **Đang làm và phần còn lại** — …
- **Mức xác minh** — Chưa build, build CI, build cấu hình thiết bị, đã nạp và chưa xác minh boot, hoặc đã xác minh trên thiết bị. Ghi rõ bo, SDK, cổng USB, ca đã thử và ca còn thiếu.
- **Nhật ký nạp** — Nếu có, ghi câu lệnh cùng địa chỉ hoặc @flash_args. Ghi các dòng boot như Loaded app from partition at offset 0x10000 và UI ready on … nếu đã quan sát qua cổng nối tiếp.
- **Tệp liên quan** — Ghi N cho tệp mới, M cho tệp sửa.
- **Quyết định Frozen** — Ghi tệp, dòng và lý do nếu có thay đổi.
- **Ràng buộc đã kiểm tra** — Ghi kết quả và ngoại lệ.
- **Trạng thái thiết bị** — Bản đang chạy, tình trạng cổng và nhu cầu nạp lại.
- **Bước tiếp theo** — Liệt kê việc có thể thực hiện trực tiếp.
- **Câu hỏi còn mở** — Nêu lựa chọn và lý do.
- **Môi trường** — SDK, cổng, container và công việc đang chạy song song.
```

Giữ mục lịch sử trong sổ. Khi việc hoàn toàn kết thúc, chuyển mục xuống phần lưu trữ. Giữ quy tắc bỏ qua `docs/HANDOFF.local.md` và tránh thêm cưỡng bức vào Git. Tệp hướng dẫn cho agent khác có thể liên kết tới tài liệu này để dùng cùng mẫu.

Kế hoạch, checkpoint, rà soát và kết quả nghiệm thu lưu trong `docs/local/` hoặc thư mục build bị Git bỏ qua. Tài liệu sản phẩm phải tự đầy đủ. Tóm tắt thay đổi theo phiên bản trong [changelog](CHANGELOG.md).

Khi chuyển trách nhiệm qua máy khác, tóm tắt phạm vi, tình trạng, việc còn lại, kết quả xác minh và bước tiếp theo trong mô tả PR hoặc kênh bàn giao đã thống nhất. Giữ nội dung cá nhân, nhật ký gỡ lỗi đầy đủ và thông tin thiết bị riêng trong sổ cục bộ.
