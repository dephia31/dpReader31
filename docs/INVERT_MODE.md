# Đảo màu S31 — thử nghiệm RC8

Trong **Cài đặt → Hiển thị → Đảo màu**, bật để đảo toàn bộ giao diện, chữ và hình ảnh. Mặc định tắt. Lựa chọn được lưu NVS; không thay đổi độ sáng đèn. Áp dụng cho cả Full và Love Book.

Renderer, cache trang/ảnh và framebuffer mà ứng dụng sử dụng vẫn ở màu bình thường. Khi xuất màn hình, adapter S31 tạo bản đích đảo 4 bit (`15 - gray`), chuyển lịch sử sang màu vật lý, refresh đồng bộ rồi trả buffer lịch sử về màu logic. Dùng thêm một framebuffer PSRAM, chỉ cấp phát khi cần. Không đảo vùng nhớ file sách hoặc cache phông.

Chế độ đảo dùng GL48/GC48 chuẩn của S31, bỏ các đường refresh/animation chuyên cho nền trắng. Bật/tắt và yêu cầu full refresh dùng GC48 toàn màn hình; các lần vẽ còn lại dùng GL48 vi sai, có GC định kỳ. Có thể chậm hơn chế độ sáng và có chớp sáng khi làm mới kỹ. Ảnh cũng đảo màu; chưa có cơ chế giữ nguyên màu ảnh. Waveform, VCOM và trình tự nguồn không sửa.

Kiểm thử host mô phỏng giao diện scan: đủ 16 mức xám, buffer logic không bị sửa, màu của ảnh lịch sử trước/sau bật/tắt, lỗi cấp phát, refresh thất bại và cờ phục hồi; setter NVS giữ lựa chọn khi mở/ghi/commit thất bại. Đây không phải kiểm thử quang học.

Cần thử trên máy: bật/tắt nhiều lần; lật ít nhất 50 trang với AA bật/tắt; trang có ảnh; mở/đóng menu, điều chỉnh đèn và slider; xoay màn hình; ngủ/thức và khởi động lại; refresh thủ công. Đánh giá bóng mờ trên nền đen, độ rõ nét chữ trắng, chớp sáng và độ trễ. Chưa có kết luận đạt chất lượng phần cứng.

## RC8 update (supersedes the RC7 refresh description above)

Dark output now inspects actual changed physical pixels: DU20 for black/white transitions, GL48 if either the old or new changed pixel is gray. Unchanged gray pixels do not promote an otherwise binary update. Existing derived waveforms hold all equal-value pairs, including black backgrounds; all changed pairs retain native vendor pulses.

Local menus/sliders preserve the requested logical crop through all four rotations. Pending pixels outside the crop remain untouched, even in a shared byte or aligned 32-pixel scan column. No-change submissions do not scan. Only changed, successful page submissions count toward the adapter's periodic GC; local controls do not. Explicit full refresh, polarity transitions, reader cleaning cycles and recovery still use full GC.

Host checks cover real DU20/GL48 LUTs, DU/GL/GC routing, unchanged gray, exact crop and rotation, clipping, no-op/control/page cycle, allocation failure, failed refresh and toggle history. Optical quality and page latency require hardware testing. Reading with AA normally uses GL; turn AA off to exercise DU text pages. Existing configured reader full-refresh intervals still apply.
