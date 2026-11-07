# continuous_du

Component `mindreset/continuous_du` hỗ trợ DU liên tục theo từng pixel cho đường xuất LCD của epdiy. Mỗi lần quét chỉ xuất một pha. Từng pixel lưu số pha còn lại và phép đánh dấu cộng dồn vào bộ đếm. Khi kéo ngón tay, vị trí mới đậm dần qua nhiều lượt quét trong khi vị trí cũ đang được xóa.

## API

- `continuous_du_mark_circle()` và `continuous_du_mark_rect()` cộng pha dương để làm đen hoặc pha âm để làm trắng.
- `continuous_du_scan()` xuất một pha cho toàn vùng đánh dấu. Trên tấm nền 1216 × 684, mỗi lượt khoảng 50 ms bất kể số pixel được đánh dấu.
- `continuous_du_busy()` cho biết còn pixel chưa xuất hết pha.
- `continuous_du_reset()` xóa bộ đếm. Trước lượt cập nhật vi sai kế tiếp, bên gọi cần đồng bộ framebuffer tham chiếu của epdiy với ảnh đang hiển thị.
- Các hàm đổi tọa độ hỗ trợ framebuffer xoay.

Tách waveform thành các lượt quét giúp độ đậm của điểm ảnh thay đổi độc lập với tần suất lấy mẫu cảm ứng. Ví dụ trong `main/apps/app_touch.c` đánh dấu điểm mới, xóa điểm cũ, gọi `continuous_du_scan()` khi còn bận và đồng bộ `hl.back_fb` khi nhấc tay trước khi dùng lại GC16.

Giấy phép Apache-2.0. Xem [LICENSE](LICENSE).
