# Bộ phông chữ đọc sách

Từ 1.4.2RC6, mục **Bộ phông chữ đọc sách** nhóm các file cùng bộ trong cùng thư mục. Ví dụ:

- `MyFont-Regular.ttf`
- `MyFont-Bold.ttf`
- `MyFont-Italic.ttf`
- `MyFont-BoldItalic.ttf`

Chọn bộ một lần; trình đọc dùng biến thể tương ứng với chữ đậm/nghiêng trong EPUB. Nhận diện bộ phông trên SD hiện dựa vào tên file, không dựa vào metadata family bên trong font. Các hậu tố Book/Roman, SemiBold và Oblique cũng được nhận diện.

Khi bật phông nhúng EPUB, các khai báo `@font-face` cùng family được phân biệt theo `font-weight` và `font-style`. Khi chọn phông người dùng thay phông nhúng, kiểu đậm/nghiêng của nội dung vẫn được giữ. Với TTF thiếu biến thể, trình đọc tạo kiểu thay thế từ phông gốc; chất lượng đẹp nhất khi đủ bốn file thật. Kiểu thay thế chưa hỗ trợ outline CFF.

Các biến thể được nạp khi chuẩn bị chương hoặc dàn lại trang; cache glyph dùng ngân sách chung. Nếu thiếu bộ nhớ hoặc hết slot phông, trình đọc dùng phông gốc. Không quét thư mục font ở mỗi lần lật trang.

Đã kiểm thử host: semantic/CSS đậm-nghiêng lồng nhau, bốn biến thể phông nhúng qua EPUB thật, thay bộ phông và gán lại run, raster FreeType, phân nhóm danh mục, dàn trang và khôi phục lựa chọn phông. Build Full/Love Book đã được thực hiện; chất lượng hiển thị, bộ nhớ và tốc độ trên thiết bị cần thử thực tế.
