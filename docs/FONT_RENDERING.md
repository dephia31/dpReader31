# Hiển thị phông chữ TTF

Đường mức xám và fallback của firmware raster hóa đường viền bằng `stb_truetype` thành độ phủ 8 bit, lưu glyph trong cache PSRAM, áp dụng điều chỉnh nét nếu đã chọn, rồi đổi sang 16 mức xám của framebuffer. Gamma độ phủ hiện giữ ở `TTF_COVER_GAMMA = 0.5`; đây là tăng độ phủ viền, không phải hinting hoặc hiệu chuẩn độ sáng quang học của tấm nền.

Phép đổi màu dùng tổng có trọng số và làm tròn về mức xám gần nhất:

```
(bg * (255 - coverage) + fg * coverage + 127) / 255
```

`fg` và `bg` ở miền 0–15. Không chia phần chênh lệch màu có dấu trước khi cộng nền: cách đó làm tròn về nền, khiến nét đen nhạt hơn, nét trắng trên nền đen cũng giảm tương phản. Các đường vẽ ngang, có giãn cách, căn đều và dọc dùng chung `mix_ink`. Chế độ không khử răng cưa giữ ngưỡng độ phủ gốc 128; không áp dụng gamma trước ngưỡng này.

Bộ raster `stb_truetype` không có TrueType hinting; đường mono thử nghiệm dùng FreeType được mô tả bên dưới. Nét rất mảnh ở cỡ nhỏ vẫn có thể trải ra nhiều pixel xám; sửa lượng tử hóa không thể khắc phục mọi khác biệt giữa các phông. Trong cài đặt đọc, có thể so sánh khử răng cưa bật/tắt và điều chỉnh độ đậm; chế độ nhị phân có thể mất nét/dấu nhỏ dù trông tương phản hơn. So sánh cùng phông, cùng cỡ và cùng trang sau một lần làm mới toàn màn hình để phân biệt chất lượng raster với bóng mờ. Kết quả trên framebuffer không thay thế kiểm tra mắt trên thiết bị.

Kiểm tra tái lập:

```
python3 tools/font_ink_host_test.py
```

Bài kiểm tra chạy hàm đổi màu thực tế với mọi tổ hợp màu/độ phủ, kiểm tra sai số tối đa nửa mức xám, đối xứng đảo màu và tính đơn điệu; đồng thời raster các ký tự tiếng Việt thật từ `main/assets/builtin.ttf` ở cỡ 18–90 px. Không đo độ tương phản vật lý của panel.

Trên S31, các trang hệ thống giữ mức xám ở chế độ Thường/Sóng nước. Chỉ chế độ Nhanh bật ép đen trắng cho trang có `binary_ui`; chế độ này chủ động bỏ viền xám để ưu tiên tốc độ. Trình đọc vẫn theo lựa chọn khử răng cưa riêng. Bộ vẽ font giao diện (`ui_font.c`) cũng làm tròn tổng màu về mức gần nhất. Kiểm tra đường vẽ giao diện và lựa chọn đầu ra bằng `python3 tools/font_output_host_test.py`.

Kiểm tra đóng gói outline của phông TTF tĩnh: `python3 tools/font_outline_pack_host_test.py /path/to/font.ttf`. So sánh mọi glyph tại 73 px với raster trực tiếp từ file gốc, gồm kích thước, vị trí và mọi byte độ phủ. Không kiểm tra gvar/biến thể trong bài này. Có thể truyền đường dẫn font cho `font_ink_host_test.py`; không kèm hay tái phân phối font của người dùng trong kho mã hoặc gói firmware.

Khử răng cưa của nội dung sách có ưu tiên cao hơn hiệu ứng Đen trắng nhanh trên S31. Khi bật, chuẩn bị Fast BW từ chối chuyển mục tiêu sang nhị phân; bộ xuất tiếp tục dùng waveform mức xám gốc, nhất quán với lần mở sách đầu tiên. Khi tắt, Fast BW dùng nguyên đường DU20. Giữ nguyên lựa chọn hiệu ứng đã lưu; bật khử răng cưa có thể làm lật trang chậm hơn Fast BW. Kiểm tra hồi quy bằng `python3 tools/reader_turn_antialias_host_test.py` (nhiều trang liên tiếp, bật/tắt/bật, bốn hướng xoay, bảo toàn lịch sử và đủ 16 mức xám).

## Thử nghiệm monochrome có hinting (S31)

Từ 1.1.9RC7-vn, **tắt Khử răng cưa** dùng FreeType2.14.3 cho các face TrueType được hỗ trợ. Bộ vẽ đọc trực tiếp nguồn font gốc qua stream, không dùng cửa sổ glyf đã đóng gói cho stb và không nạp thêm toàn bộ font. Nét có hint chương trình dùng native hinter; font thiếu các chương trình prep/fpgm dùng auto-hinter. Mục tiêu là nét thuần đen/trắng, không dither viền xám.

Để so sánh Bookerly73, chọn **Đen trắng nhanh**, tắt **Khử răng cưa**, đưa điều chỉnh nét về mức thường/0, rồi xem cùng trang sau một lần toàn refresh. Hinting có thể đổi vị trí/biên glyph theo pixel; độ rộng tiến chữ và cách phân trang vẫn dùng metrics cũ. AA bật dùng stb và mức xám như trước. CFF/OTF-CFF, nguồn quá16MiB, font không được engine hỗ trợ, lỗi đọc hoặc không đủ bộ nhớ giữ fallback native; không đổi font đã lưu. Log một lần lúc mở face cho biết hinting sẵn sàng hay fallback.

Glyph mono bình thường được chép thẳng dạng1bit vào LRU hiện có; AA và mono có khóa cache riêng. Điều chỉnh nét vẫn giữ thuật toán/giá trị đang lưu, sau đó lượng tử hóa về hai màu. Cache hit không raster lại hoặc đọc thêm font. Mọi cấp phát động qua FreeType memory manager dùng PSRAM, tổng trần512KiB, giữ dự trữPSRAM2.5MiB; không chuyển engine vào SRAM khi thiếu chỗ. Đóng font/thu hồi cache tùy chọn đóng cả face/engine để trả bộ nhớ trước các tác vụ cần RAM lớn.

Hinting không xóa giới hạn pixel, không bảo đảm hết ghosting hoặc tốc độ trên máy thật. Giữ nguyên waveform, điện áp và nhịp quét. Bản thử không tự bật/tắt AA hoặc đổi lựa chọn độ đậm đã lưu.

Kiểm tra engine và cache thực tế, không cần FreeType cài sẵn trên máy:

```
python3 tools/font_mono_host_test.py /path/to/Bookerly.ttf
python3 tools/font_mono_cache_host_test.py /path/to/Bookerly.ttf
```

Các bài kiểm tra biên dịch chính subsetFreeType của firmware, thử ký tự tiếng Việt18–90px, nguồn đọc lỗi/cắt cụt, thu hồi đầy đủ, packedmono/khóaAA riêng,65mức điều chỉnh nét, fallback và lỗi cấp phát. Thời gian benchmark trên host không phải thời gian lật trangESP32-S31. Bookerly chỉ là đầu vào kiểm tra cục bộ, không kèm trong firmware hay kho mã.

RC7 có lỗi: bộ raster mono dùng mảng pool16KiB cục bộ trên stack, ngoài số liệu cấp phát động ở trên. TừRC8 pool này giới hạn2KiB; thư viện chia dải khi đầy. Không tăng stack tác vụ hay chuyển các buffer DMA. Kiểm tra `python3 tools/font_mono_stack_host_test.py /path/to/Bookerly.ttf` so sánh toàn bộ pixel/bounds với pool16KiB và đo stack frame của compiler. Stack `.su` từ bản biên dịch S31 cũng được kiểm tra trước khi bàn giao.

### Monochrome stroke adjustment (RC9)

Hinted TrueType glyphs apply the signed stroke setting to the loaded outline before mono rasterization, with centered FreeType outline emboldening/erosion. Applying the coverage filter to an already binary bitmap discarded small adjustments at the final threshold. One setting unit is a 1/16-pixel contour offset; the monochrome pixel grid can still make adjacent units look identical on some glyphs. Advances remain unchanged. Gray and unsupported-font fallback paths retain the coverage filter. The actual cache host test checks lighter/default/heavier ink counts at fine and coarse levels, binary pixels and stable advances.
