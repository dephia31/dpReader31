# Hiển thị E-ink và bóng mờ

## Cơ chế và nguyên nhân trong firmware

Bóng mờ là vết vật lý còn lại của hình trước sau một lần đổi hình. DU/GL16 vi sai và waveform giữ các cặp mức xám không đổi giúp nhanh và ít nháy, nhưng không thay thế GC16 đầy đủ. RAM trống nhiều hơn không tự xóa trạng thái mực trên panel. [Microchip: E-paper fundamentals](https://developerhelp.microchip.com/xwiki/bin/view/software-tools/mgs/dev-kits/epd-ug/fundamentals/) mô tả ghosting khi dùng partial refresh.

Các phát hiện cụ thể của đường Metalio:

- `boards/metalio47/src/main/display.c`, `main_begin()`: điều kiện cũ `!monochrome && ui_page_cleanup_due(changed)` bỏ qua cả việc đếm lẫn GC cho frame gắn cờ monochrome. Không được hiểu rằng mọi chế độ đã có GC định kỳ.
- `hl_update()`: điều kiện cũ `!s_system_binary && whole_page ...` cũng loại các trang hệ thống đen trắng khỏi GC tự động.
- Nhánh dọn Home cũ nằm trong renderer phân vùng. Có thể dùng soft waveform cho phần ngoài tranh, giữ nền trắng/thanh điều hướng; do đó không tương đương đường làm mới toàn màn hình thủ công.
- Trình đọc có bộ đếm riêng `app_settings_reader_full_pages()`: 0 là tắt, các giá trị1/3/5/8/10/15/20/30 được giữ trong NVS. Mặc định3 không chứng minh thiết bị đang dùng3. Càng nhiều lượt vi sai giữa các lần GC càng có thể thấy chữ/tranh cũ.
- Các lần mở menu, đổi đồng hồ/thông báo và phản hồi cục bộ không được tính như một trang mới. Giữ chính sách này để tránh chớp toàn màn hình sau vài thao tác nhỏ; nếu bóng tập trung ở vùng đó cần đo riêng thay vì tăng chu kỳ lật trang.
- Bảng Metalio hiện có một dải nhiệt0–50°C và các lời gọi phổ biến truyền25°C. Không có LUT nhiều dải nhiệt được hiệu chuẩn cho thiết bị trong mã hiện tại. Đây là giới hạn cần kiểm tra khi so sánh ở nhiệt độ khác nhau, không phải bằng chứng nhiệt độ là nguyên nhân ở máy cụ thể.

## Chính sách từ1.1.8RC4

Home và trang đen trắng cùng tính các frame nội dung thay đổi. Đến6 frame, và tối thiểu3giây từ GC trước, dùng đường `update_display_full_unlocked()` với `E0470_FULL_WAVEFORM`, `MODE_GC16`, toàn bộ panel. Gọi trước khi cấp cache/phân vùng/soft output; không ghép thêm một vòng soft lên sau GC. Nền trắng và thanh điều hướng cũng tham gia.

Các trang hệ thống full-frame đen trắng không còn bị loại khỏi GC định kỳ. Không thay raw waveform GC/GL/DU, pulse timing, scan clock, VCOM, nguồn hoặc OTP. Không tự đổi lựa chọn full-refresh của trình đọc. Khi GC báo lỗi, bộ đếm không bị xóa, đường phục hồi/GC tiếp theo vẫn được giữ.

Đổi lại, GC định kỳ sẽ có một lần nháy và lâu hơn lượt fast thông thường. Đây là đổi hành vi có chủ đích để giảm ghosting, không phải lỗi treo. Thao tác cục bộ và frame không đổi vẫn không ép GC.

## Kiểm tra trên thiết bị

1. Làm mới toàn màn hình thủ công để có mốc so sánh. Nếu vẫn còn vết rất rõ ngay sau GC, không kết luận chỉ do tần suất; cần log lỗi scan/underrun, ảnh panel và kiểm tra nguồn/panel tại cùng nhiệt độ.
2. Đổi nội dung qua ít nhất6 trang Home/hệ thống có thay đổi, cách GC trước ít nhất3giây; xác nhận có một lần GC toàn panel, gồm thanh điều hướng. Lặp cả mode thường và đen trắng.
3. Trong trình đọc chọn full-refresh mỗi3trang, lật qua trang nhiều chữ/ít chữ/tranh. Có thể thử mỗi1trang để phân biệt phần bóng do vi sai. Giữ nguyên font, ánh sáng và sách khi so sánh.
4. Kiểm tra vùng từng có menu/KoSync/đồng hồ riêng; ghi nhận GC thủ công có xóa được vết không. Chu kỳ tính theo lượt đổi trang, không theo số phút đứng yên.
5. Nếu có log scan thiếu hàng/EPD_DRAW_* thì xử lý phục hồi trước khi đánh giá waveform. Không tự thử VCOM/OTP hoặc mượn waveform của panel khác.

Host test `tools/metalio_ghost_cleanup_host_test.py` thực thi bộ đếm và đường GC thật với mock phần cứng: đen trắng/thường, frame không đổi, thời gian tối thiểu và lỗi/retry. Build/host test chỉ chứng minh cách chọn đường cập nhật; mức ghosting quang học phải xác nhận trên panel.
