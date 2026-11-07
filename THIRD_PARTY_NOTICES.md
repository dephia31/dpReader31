# Thông báo giấy phép bên thứ ba của dpReader

Giải mã JPEG kết hợp trên S31 dùng `esp_driver_jpeg` của Espressif (Apache-2.0) và `espressif/libjpeg-turbo 3.2.0~1` (IJG/BSD-3-Clause cho mã gốc, Apache-2.0 cho phần đóng gói Espressif).
This software is based in part on the work of the Independent JPEG Group.
S31 hybrid JPEG decoding links the Espressif hardware driver and the pinned
libjpeg-turbo component. Upstream and packaging licenses remain unchanged;
the distributed component's LICENSE and upstream LICENSE.md govern those files.

[Chính sách giấy phép học tập cá nhân](LICENSE) áp dụng cho phần sáng tạo mới có đánh dấu rõ. Giấy phép bên thứ ba, quyền đã cấp và các bản lịch sử tiếp tục áp dụng.
The [personal-study license policy](LICENSE) covers only explicitly marked new original material. It does not change the following third-party licenses, existing grants or historical releases, or revoke rights granted by third parties.

This is an independent reader firmware for MindReset's Read Pico board. It is
based on the [MindReset demo firmware](https://github.com/MindReset/read_pico_firmware),
which is licensed under Apache-2.0. The original copyright and license notices
remain in the source. This project is not an official MindReset release.

dpReader là firmware độc lập phát triển từ firmware mẫu MindReset Read Pico. Thông báo bản quyền và giấy phép Apache-2.0 gốc được giữ trong mã nguồn.

| Thành phần | Giấy phép | Thông báo |
| --- | --- | --- |
| MindReset Read Pico firmware and board components | Apache-2.0 | [Original Apache-2.0 license](licenses/Apache-2.0.txt); source file headers |
| [MokuMMk/MokuReader](https://github.com/MokuMMk/MokuReader/commit/1d7951ec303c320a154282f4e6a579f687d0dc4f) — power and PIN liquid-glass rendering adapted into `main/ui/ui_glass.c` and `ui_pinpad.c` | Apache-2.0 (upstream file notices retained) | Gaussian blur, bezel refraction and highlights; local adaptations bound/release PSRAM workspaces and pack control snapshots |
| [Lucide](https://lucide.dev/icons/) — UI icon paths, vendored as SVG sources in `tools/icons/lucide/` and compiled into `main/assets/ui_icons.h` by `tools/gen_ui_icons.py` | ISC | [Lucide notice](licenses/LUCIDE-ISC.txt) |
| [AnimateIcons](https://github.com/Avijit07x/animateicons) — pagination and circular-back paths; pinned source in `tools/icons/animateicons/` | MIT and underlying Lucide ISC | [MIT notice](licenses/AnimateIcons-MIT.txt), [upstream notices](tools/icons/animateicons/UPSTREAM_NOTICES.md), Lucide notice above |
| [JPEGDEC](https://github.com/bitbank2/JPEGDEC) — progressive JPEG decoding, vendored and patched in `components/jpegdec/` | Apache-2.0 | [Component license](components/jpegdec/LICENSE), [Fork notes](components/jpegdec/FORK.md) |
| [epdiy](https://github.com/vroland/epdiy), including the locally modified driver | LGPL-3.0-or-later | [Modification notes](components/epdiy/LICENSE), [LGPLv3](licenses/LGPL-3.0.txt), [GPLv3](licenses/GPL-3.0.txt) |
| [pypinyin](https://github.com/mozillazg/python-pinyin) dictionary data used by offline book search | MIT | [pypinyin notice](components/read_pico_search/LICENSE.pypinyin) |
| [CrossMux](https://github.com/0x1abin/crossmux) BLE report mapping and text-turn adaptations | MIT | [CrossMux notice](licenses/CrossMux-MIT.txt) |
| FreeInk SDK text-turn waveform assembly, commit `96de1be6ce08eb732909e6e8149af8f892b9a2c5` | MIT | [FreeInk SDK notice](licenses/FreeInk-SDK-MIT.txt) |
| [Metalio E-Ink4-Plus](https://github.com/CloudZao/Metalio-E-INK4-Plus) sample firmware — ESP32-S31 LCD/IOMUX output path, TPS65185 and 9555 board reference, C equivalents of the Xtensa SIMD pixel paths, and the `s31_sleep` SDK compatibility module (source ad8edf4150a688737c0d7c00cae4413079b262ef) | MIT | [Metalio notice](licenses/Metalio-MIT.txt) |
| Embedded Noto Sans SC Medium font subset and compressed common-Han bitmaps | SIL OFL-1.1 | [Font license](main/assets/OFL-Noto.txt) |
| [ESP Web Tools](https://github.com/esphome/esp-web-tools) browser flasher | Apache-2.0 | [Bundled web tool license](flash/vendor/LICENSE) |
| Espressif TinyUSB component | Apache-2.0 | [Component license](components/espressif__esp_tinyusb/LICENSE) |

License terms apply to their respective material. The repository's
[license-scope statement](LICENSE) and new personal-study license do not
replace these component licenses or restrict their independently granted rights.

Giấy phép áp dụng riêng cho từng thành phần.

Phần lật trang chữ tham khảo CrossMux `1512e8e049f2b1a4efc7c004a5b1a93cbc5e3f9f` và FreeInk SDK `96de1be6ce08eb732909e6e8149af8f892b9a2c5`. Cách ghép bảng trong `components/e0470_epaper_waveform/e0470_epaper_waveform.c` chuyển từ `libs/display/EpdiyLcd/src/e0470/e0470_epaper_waveform.c`. Giữ đầu tệp `Copyright 2026 mindreset` / Apache-2.0 và toàn văn thông báo MIT của FreeInk và CrossMux trong các tệp dẫn ở bảng trên. Chỉ lật trang chữ thông thường dùng bảng này. Waveform gốc và thời gian quét giữ riêng theo bo.
The text-turn assembly is adapted from the pinned FreeInk source above,
retaining its mindreset Apache-2.0 file notice and the complete FreeInk/CrossMux
MIT notices linked above. Only ordinary text turns select it; vendor LUTs,
scan timing and other screens are unchanged. Panel validation remains required.

The firmware bundles only the embedded Noto Sans SC subset and common-Han bitmap supplement; users may supply
their own fonts on a TF card. The repository and flashing site do not provide
additional font packages.

Phân tích báo cáo và ánh xạ phím cho bộ lật trang BLE tham khảo BleKeyboardHost của CrossMux. Đầu tệp component giữ nguồn và giấy phép MIT của CrossMux. JPEGDEC dùng giấy phép Apache-2.0 của BitBank. Xem các sửa đổi trong `components/jpegdec/FORK.md`.

## FreeType monochrome TrueType engine

This software uses the FreeType library, copyright © the FreeType Project (www.freetype.org). FreeType2.14.3 is included under the FreeType License in `components/dpreader_freetype/vendor/docs/FTL.TXT`; upstream sources remain unmodified. Source archive identity and selected modules are recorded in `components/dpreader_freetype/UPSTREAM.md`. The embedded Bitter Regular reader font is licensed under SIL OFL-1.1; see `main/assets/OFL-Bitter.txt`. It is instantiated at weight 400 from Google Fonts `ofl/bitter/Bitter[wght].ttf` (https://github.com/google/fonts/tree/main/ofl/bitter).
