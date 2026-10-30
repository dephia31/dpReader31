# Hiển thị phông chữ — dpReader 1.2.1-vn

Trên S31, phông TrueType được hỗ trợ dùng FreeType2.14.3: auto-hinting cho cả hai chế độ; AA dùng FT_LOAD_TARGET_LIGHT/FT_RENDER_MODE_LIGHT, đen trắng dùng FT_LOAD_TARGET_MONO/FT_RENDER_MODE_MONO. Glyph AA giữ độ phủ8bit với gamma1.0 đến lúc trộn vào framebuffer4bpp; không lượng tử bốn mức trước khi trộn. Chữ giao diện từ builtin.ttf dùng cùng bộ raster; các ngưỡng đen trắng/tựa sách giữ nguyên. Font không được engine hỗ trợ hoặc thiếu bộ nhớ dùng fallback stb. Pico giữ chính sách riêng.

Tham khảo chính sách raster của [CREngine f335f7a](https://github.com/koreader/crengine/blob/f335f7a97e240f505bb8a0dfb721fcb7912a7824/crengine/src/lvfntman.cpp) và [KOReader-base 007ac6a](https://github.com/koreader/koreader-base/blob/007ac6a81e2de4822c8df7ce57b8dbb06aa3b8ea/cre.cpp). Phần tích hợp viết riêng bằng API FreeType, không chép mã hoặc bảng CREngine; không port KOReader/HarfBuzz.

## Cache, bố cục và bộ nhớ

Advance/phân trang giữ metadata hiện có; hinting có thể thay vị trí pixel của nét. Cache phân biệt AA/mono, codepoint, cỡ, trọng lượng và điều chỉnh nét. Mono được đóng gói1bit; cache hit không raster hoặc đọc lại font. Phông được đọc qua stream gốc, không nạp thêm toàn bộ font cho FreeType. Engine giới hạn512KiB PSRAM và giữ dự trữ2.5MiB, không chuyển sang SRAM khi thiếu chỗ.

Điều chỉnh nét có dấu thực hiện trên outline trước raster với bước1/16pixel; advance không đổi. Fallback giữ bộ lọc độ phủ hiện có. Main taskS31 có24KiB stack, prefetch16KiB; FreeType dùng poolmono2KiB và các bufferautofitinline nhỏ với khả năng tăng động qua allocator có giới hạn. Các sửa stack này vẫn cần thiết dù tổng cấp phát PSRAM thấp.

## Chuyển mức xám và màn hình

Trộn màu làm tròn tổng có trọng số: `(bg*(255-coverage)+fg*coverage+127)/255`, màu0–15. Độ phủ0 không làm tối nền. Mọi đường vẽ ngang, xoay, giãn cách và mask dùng cùng chính sách độ phủ; mono không áp gamma trước ngưỡng.

AA lật trang đọc dùng GL37 dẫn xuất từ bảng GL48 gốc S31 với erase_max11/sat_cut5/white_sat_cut0/hold3. Giữ đuôi mức xám; trắng→đen có13xung và đen→trắng18xung. Các cặp mực không đổi giữ nguyên; trắng→trắng giữ số xung bảo trì trước ba pha giữ cuối. Full refresh/chu kỳ làm sạch/phục hồi dùng GC48; tạo bảng không hợp lệ quay về selectiveGC48. Không sửa bảng gốc hoặc giả lịch sử framebuffer.

AA bật được ưu tiên hơn lựa chọn FastBW, giữ mức xám cả khi mở sách và lật tiếp; AA tắt cho phép DU20. VCOM runtime2000mV giữ từ1.2.0, không ghi đè hiệu chuẩn lưu hoặc OTP; nguồn và nhịp quét giữ nguyên.12ms×37pha=444ms chỉ là ngân sách pha, chưa gồm render/cấp nguồn và không phải thời gian ổn định quang học. Độ đen và bóng mờ phụ thuộc panel/nhiệt độ/nội dung; kiểm tra framebuffer không thay quan sát thiết bị.

## Kiểm tra tái lập

```
python3 tools/font_koreader_ui_host_test.py
python3 tools/font_aa_host_test.py
python3 tools/font_mono_host_test.py
python3 tools/font_mono_cache_host_test.py
python3 tools/font_autofit_stack_host_test.py
python3 tools/reader_aa_waveform_host_test.py
python3 tools/reader_selective_cleanup_host_test.py
python3 tools/reader_turn_antialias_host_test.py
```

Các bài font nhận đường dẫn font riêng nếu được hỗ trợ; không tái phân phối font người dùng. So cùng font/cỡ/trang/ánh sáng sau một lần full refresh rồi theo dõi nhiều trang để phân biệt raster với bóng mờ.
