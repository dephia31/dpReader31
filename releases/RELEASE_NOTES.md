## dpReader 1.5.3-vn / 1.5.3-love-vn

- Bổ sung bản dịch tiếng Anh cho Cài đặt và thông báo. Tách mục **Nguồn & Khóa Màn Hình**.
- Dùng hiệu ứng Quick Settings khi mở và đóng pop menu. Đồng bộ kiểu thông báo mở/đóng sách, KoSync, OPDS và đánh thức. Căn lại New Home trên Love Book.
- Cho chọn Deep Sleep ngay hoặc sau 5/10 phút Light Sleep. Cải thiện phản hồi khi thức và khôi phục phông đọc.
- Bộ cài USB ưu tiên kết nối với máy đã ở BOOT, có thông báo lỗi kết nối chi tiết.

Bản chính thức giữ P1.4 trong lúc ngủ và giữ tùy chọn dùng nút âm lượng để đánh thức. Tùy chọn tắt nguồn sau 1 phút vẫn là chế độ thử nghiệm. Sau khi tắt nguồn hoàn toàn, cách bật máy phụ thuộc phần cứng.

English highlights: more translated settings and notices, a faster pop menu, wake feedback and font recovery improvements, and improved USB installer diagnostics.

Mã nguồn: `639ce1c9cbabbd4678fb24d16bb1f52885c64547`.

## dpReader 1.5.2-vn / 1.5.2-love-vn

- Thêm New Home và làm mới thanh điều hướng, Tủ sách bìa lớn và trang thống kê đọc.
- Thêm giao diện Tiếng Việt / English và màn Welcome có logo dpReader khi cài mới.
- Bổ sung README tiếng Anh và Web Flash song ngữ, có ảnh minh họa ba kiểu Home.

Có hai gói Full và Love Book cho Metalio E‑Ink 4 Plus (ESP32‑S31). Nếu cần quay về 1.5.1, hãy dùng bản sao lưu tạo trước khi nâng cấp. Sao lưu có lựa chọn ngôn ngữ dùng định dạng V33.

English highlights: New Home, a refreshed dock and large-cover library, English firmware UI, first-run language selection, and bilingual Web Flash.

Mã nguồn: `5664b1714be63ed8f59d6ab66ea870fe46db676a`.

## dpReader 1.5.1-vn / 1.5.1-love-vn

- Tối ưu hiệu năng máy, mở và đóng sách nhanh hơn.
- Tải ảnh bìa nhanh hơn trên Trang chủ và Tủ sách.
- Tối ưu pin với Deep Sleep và quản lý Wi-Fi, Bluetooth khi ngủ.

Có hai bản Full và Love Book cho Metalio E‑Ink 4 Plus. Hãy chọn cùng bản đang dùng để cập nhật.

Bản sao lưu có mục Yêu thích hoặc Ẩn sách theo định dạng mới cần dpReader 1.5.1 trở lên để khôi phục.

Mã nguồn: `b7046ca8a2b71dfc45c83026e697ac38bd8ab447`.

## dpReader 1.4.8-vn / 1.4.8-love-vn · 2026-11-15

- **Tối ưu giao diện trình đọc sách:** theo kết quả tác giả tự đo, phản hồi nhanh hơn **30%**, thao tác nhẹ nhàng hơn và mức tiêu thụ pin giảm **60%**. Menu dùng cập nhật chênh lệch, giảm lượt vẽ và làm mới dư thừa khi mở/đóng thanh công cụ, cài đặt đọc, phông chữ và mục lục.
- Thanh công cụ, Cài đặt đọc và Phông chữ mở bằng DU20. Quick Setting mở nhanh, đóng bằng khôi phục vùng bị che để hạn chế nháy toàn màn hình; bảng phủ thanh trạng thái, trải hết chiều ngang và chỉ bo hai góc dưới.
- Đồng bộ style và nút Quay lại trong các menu đọc sách. Độ đậm chữ có khung xem trước và nút giảm/tăng dạng thẻ; sửa hàng Chụp ảnh màn hình bị nút Quay lại che trong gán phím hệ thống.
- Thêm shortcut **Chụp ảnh màn hình**, lưu PNG trong thư mục screenshots trên thẻ SD.
- Kiểu nét chữ giữ Đơn sắc gốc, tách Khử răng cưa với hai công tắc Khử răng cưa/Tương phản chữ và Đen trắng đậm với các ngưỡng **35%, 40%, 45%, 50%, 55%, 60%, 65%**. Tương phản chữ khi bật Khử răng cưa dùng lõi đen từ độ phủ 80%, giữ viền xám; không đổi cách chia trang.
- Tăng cỡ chữ Xin Chào, Đã đọc …% và Đọc tiếp trên Home của I Love Book và Love Book; giới hạn độ rộng để tránh chồng chữ khi dùng phông rộng.
- Bỏ đường đánh thức trực tiếp từ Light Sleep bằng nút AI và tùy chọn tương ứng; giữ nút nguồn và các nút âm lượng theo thiết lập. Dọn mã menu cũ và giảm công sao chép bộ đệm Quick Setting.

Mã nguồn: `5e4b0e7a776f222de14aae4c5c8b9449f0ef3160`.
