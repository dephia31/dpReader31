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

### Hinted gray trial (1.2.0RC1)

On S31 supported TrueType faces, AA on now uses FT_LOAD_TARGET_NORMAL and the upstream smooth raster. Signed stroke offsets precede rasterization, as in mono. Gray cache entries carry raw 8-bit coverage and a hinted-gray flag; drawing maps coverage to four tones (0/85/170/255) with thresholds32/104/176, bypassing the legacy gamma lift for this path. The mask and horizontal/rotated/spaced/fitted drawing paths use the same mapping. Existing native layout metadata/advances and AA/mono cache separation remain. Unsupported or unavailable faces use the existing coverage path.

This is a local optical trial, not a measured ghosting fix. Native GL48/GC48/DU20 waveforms, refresh cadence and power sequencing remain unchanged. Per the subsequent user request, 1.2.0RC1 applies1200mV at boot while preserving saved NVS calibration for rollback. Faint coverage is removed and the black core strengthened; screen-history residue still requires a correct refresh path and actual-device comparison. Roll back using the official1.1.9 USB application package.

### S31 VCOM reference limits

The vendor [Plus config](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/2f55cb1234baae4968982feb23e7a9001dac16f0/main/boards/metalio-e-ink4-plus/config.h#L22) sets EPD_VCOM_MV to1200; [metalio_epd.c](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/2f55cb1234baae4968982feb23e7a9001dac16f0/main/boards/metalio-e-ink4-plus/display/metalio_epd.c#L38) applies it, and the board API takes positive magnitude for a negative voltage. The inspected barkure2970d347,Marspacecraftb400605e andAndluo123d9fe9ab8 forks retain1200 too. These inherited defaults are not independent optical calibration results. Current dpReader S31 default is1700mV; a captured RC9 device log also reported1700mV register readback. Do not infer that increasing magnitude reduces ghosting, or overwrite a device calibration from a repository default. The3.97-inch Metalio E-Ink4 uses a different S3/SPI controller and its parameters are not applicable. The subsequent user-authorized1.2.0RC1 trial applies1200mV at runtime; it does not rewrite saved NVS calibration or TPS OTP.

### Selective ink cleanup trial (1.2.0RC2)

The user rejected shortening the global-cleanup interval. For S31 AA text turns, the reader now derives a3KiB table: all255 source/target pairs involving ink reuse the exact native GC48 sequence; unchanged white (15→15) retains the existing text GL48 white-maintenance sequence. Native tables are unmodified. No framebuffer history is falsified, no phases are trimmed, and no second display pass is added. The normal white background receives no black pulse from this derived table; old/new glyph footprints can visibly pulse during reconstruction. The existing batch crop/masks keep updates inside the reader region. AA off retains the original path. Unsupported source tables fall back to the original reader waveform.

This is an optical test of local reconstruction, not a guarantee against uniform background darkening or a speed improvement.48phases remain; measured RC1 turns were approximately848ms with AA versus421ms without AA. Keep the user-selected full-refresh interval and1200mV trial unchanged. Begin an optical comparison from a clean screen once, then compare several turns under identical light; assess both background and glyph flicker. The host regression verifies every transition and absence of a black pulse for unchanged white; only device testing can establish visible benefit.

### Tone-mode handoff (1.2.0RC3)

Changing the persistent AA setting invalidates prepared pages and, after a successful save, requests one S31 full redraw/optical cleanup and resets the page-cleanup counter. Failed saves keep the active mode and counter unchanged. This reset occurs only when the user changes tone mode; it does not shorten the saved periodic-refresh interval. A binary target bitmap does not prove optical black saturation when switching from a gray optical state. If black still appears gray immediately after this cleanup, the remaining cause needs separate waveform/panel calibration; do not claim a mode-handoff fix addresses all ghosting or voltage effects.

### VCOM comparison (1.2.0RC4)

Per the user request, RC4 initializes and applies a1400mV runtime magnitude (approximately−1.40V) instead of RC3's1200mV. Existing saved calibration and the vendor-reference fallback remain unchanged; only the trial override and pre-resolution board initial value change. Rendering, selective ink cleanup, mode-switch reset and periodic-cleanup settings remain the same as RC3. No TPS OTP write is added. Optical darkening/saturation still require device comparison; this voltage is a trial, not a validated calibration.

### VCOM comparison (1.2.0RC5)

Per the user request after RC4 feedback, RC5 initializes and applies an1800mV runtime magnitude (approximately−1.80V) instead of1400mV. Saved calibration, vendor-reference fallback1200mV, rendering, waveforms and refresh cadence remain unchanged. This remains a local optical trial, not a validated calibration; no TPS OTP is programmed.

### VCOM comparison (1.2.0RC6)

After the user reports darkening at1800mV and requests2000mV, RC6 initializes and applies a2000mV runtime magnitude (approximately−2.00V). Saved calibration and vendor fallback1200mV are preserved. All rendering, waveform and refresh-cadence behavior stays as RC5. This is a local comparison, not a validated calibration; no TPS OTP is programmed.

### Auto-hinter stack repair (1.2.0RC7)

A real RC6 panic identified `af_latin_metrics_init_widths` during glyph loading, with the stack pointer below the main task's hardware stack bounds. The nested autofit loader and metrics functions used approximately14.7KiB before callers, exceeding the16KiB task stack. Raster-pool limiting alone did not cover these separate inline hint buffers.

The build now generates a reduced-inline-capacity header outside the original vendor tree. Existing FreeType dynamic growth handles larger outlines using the bounded PSRAM allocator. Original algorithms, source font streams and the512KiB engine budget remain. Native target stack frames reduce to about1.4KiB(loader) and2.3KiB(Latin metrics). Host regressions force auto-hinting for built-in and supplied fonts and compare original/fixed mono+gray pixels; native cache tests retain coverage of AA mode separation and cleanup. Bluetooth and reader exit still require device acceptance; the captured panic itself was in glyph loading, not a BLE callback.

### S31 task headroom (1.2.0RC10)

The S31 main task now reserves24KiB and book prefetch16KiB (previously16/12KiB); Pico prefetch remains12KiB. This adds12KiB of task-stack allocation while prefetch is alive, independently of the512KiB FreeType PSRAM budget. Cached S31 build configurations migrate to the same24KiB main stack. Existing chapter-load logs report main-task minimum remaining stack; prefetch teardown logs its minimum remaining stack. These runtime figures require device acceptance with Bluetooth and representative books; the reduced-inline-buffer autofit fix remains necessary.
