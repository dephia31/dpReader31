/* SPDX-License-Identifier: Apache-2.0
 * Nhận diện bo mạch và khả năng phần cứng Metalio E-INK4 Plus V1.5. Chọn bo trước khi build. Firmware không dò đoán phần cứng hoặc nhận gói của bo khác.
 * English: Metalio E-INK4 Plus V1.5 build identity and hardware capabilities. The board is fixed
 * before compilation; never guess hardware at runtime or accept another board's update.
 *
 * Quy tắc cố định: chỉ đặt hằng số và macro khả năng ở đây. Mã dùng chung chọn nhánh bằng các macro này hoặc API của components/dpreader_board.
 * Frozen: constants and capability macros only. Shared code branches on these macros or on the
 * components/dpreader_board interface.
 */
#pragma once

#ifndef DPREADER_BOARD_METALIO47
#define DPREADER_BOARD_METALIO47 1
#endif
#if DPREADER_BOARD_METALIO47 != 0 && DPREADER_BOARD_METALIO47 != 1
#error "DPREADER_BOARD_METALIO47 must be 0 or 1"
#endif

/* ---- Nhận diện bo mạch ---- */
#define DPREADER_BOARD_ID "metalio47"
#define DPREADER_BOARD_PROJECT "dpReader"
/// chip_id trong đầu ảnh ESP32-S31 là 32 theo esp_chip_info.h.
/// esp_image header chip id for the ESP32-S31 (CHIP_ESP32S31 = 32 in esp_chip_info.h).
#define DPREADER_BOARD_IMAGE_CHIP_ID 0x0020
/// Giữ không gian tên NVS của test31 đến test36 để cấu hình và mật khẩu còn dùng được sau khi nâng cấp.
/// The NVS namespaces keep the values used by the standalone test31-test36 firmware so settings
/// and the lock PIN survive the move to the shared tree.
#define DPREADER_BOARD_SETTINGS_NAMESPACE "metalio_s31"
#define DPREADER_BOARD_LOCK_NAMESPACE "metalio_lock"
#define DPREADER_BOARD_STARTUP_NAMESPACE "metalio_start"

/* ---- Tấm nền ---- */
/// Hai bo dùng tấm nền đơn sắc 1216×684 với đường điều khiển khác nhau.
/// Both boards carry the same 1216x684 mono panel; only the driving path differs.
#define DPREADER_BOARD_PANEL_WIDTH 1216
#define DPREADER_BOARD_PANEL_HEIGHT 684
/// Giá trị hiệu chuẩn VCOM nằm trong NVS. Mức mặc định 1200 mV khớp firmware nhà sản xuất CloudZao/Metalio-E-INK4-Plus.
/// Đây là mốc duy nhất đã được xác minh trên phần cứng xuất xưởng. Khoảng hiệu chuẩn từ 900 đến 2000 mV.
/// The calibrated VCOM lives in local NVS; the 1200 mV default matches the vendor
/// firmware and is the only value verified on shipping hardware. Calibration
/// window 900..2000 mV.
#define DPREADER_BOARD_VCOM_IN_NVS 1
#define DPREADER_BOARD_VCOM_DEFAULT_MV 1200
// User-authorized local optical trial; preserve saved calibration for rollback.
#define DPREADER_BOARD_VCOM_TRIAL_MV 2000

/* ---- Khả năng phần cứng ---- */
/// Front light (brightness + warmth) driven over I2C; haptics through a GPIO-driven motor.
/// Đèn trước có độ sáng và nhiệt màu, cùng mô-tơ rung.
#define DPREADER_BOARD_HAS_FRONTLIGHT 1
#define DPREADER_BOARD_HAS_HAPTIC 1
/// Phím âm lượng bên hông nối với FCA9555 P1.0/P1.1.
#define DPREADER_BOARD_HAS_VOLUME_KEYS 1
/// Đường nguồn TPS65185 và đọc lại VCOM.
#define DPREADER_BOARD_HAS_TPS65185 1
/// GPIO36 chuyển USJ/UTMI và bàn giao chế độ tải qua USB.
#define DPREADER_BOARD_HAS_USB_ROUTE 1
/// Làm mới định kỳ chống mờ và cache bìa trong PSRAM.
#define DPREADER_BOARD_HAS_ANTI_FADE 1
#define DPREADER_BOARD_HAS_COVER_CACHE 1
#define DPREADER_BOARD_CACHE_DIR "/sdcard/.metalio_s31"
/// No CW32L010 PMU on this board: time comes from the PCF8563, keys from the expander.
/// Bo không có PMU CW32L010. Thời gian lấy từ PCF8563 và phím đọc qua bộ mở rộng I/O.
#define DPREADER_BOARD_HAS_PMU 0
/// Lock-face standby policy (light/deep/off) is a product setting here.
/// Có cài đặt chế độ chờ ở màn hình khóa gồm ngủ nông, ngủ sâu và tắt nguồn.
#define DPREADER_BOARD_HAS_LOCK_SLEEP_POLICY 1
/// Reader turn effect adds a fast monochrome mode on this panel.
/// Tấm nền hỗ trợ chế độ lật trang đen trắng nhanh.
#define DPREADER_BOARD_READER_TURN_EFFECT_MAX 3

/* ---- Sao lưu và tên thiết bị ---- */
/// Values kept byte-identical to the standalone test31-test36 firmware so existing backups
/// and the default device name keep matching.
/// Giữ định dạng từng byte của test31 đến test36 để dùng được bản sao lưu và tên mặc định cũ.
#define DPREADER_BOARD_BACKUP_PREFIX "dpReader-settings"
#define DPREADER_BOARD_PRODUCT_NAME "dpReader"

/* ---- Chính sách hiển thị ---- */
/// One epd_clear_checked baseline instead of the Pico blind-repair cycles.
/// Tạo nền trắng bằng một lần epd_clear_checked, theo đúng quy trình dành cho bo này.
#define DPREADER_BOARD_HAS_PANEL_BLIND_REPAIR 0
/// Power the rails down right after every output instead of an idle timeout
/// (the 2e1e4e2 fix: white turns grey under strong light if the rails stay up).
/// Ngắt nguồn tấm nền ngay sau mỗi lần xuất hình. Giữ nguồn khi rảnh làm nền trắng ngả xám dưới ánh sáng mạnh.
#define DPREADER_BOARD_RAILS_IDLE_TIMEOUT_MS 0
/// Vendor 2.0.76 full GL48 waveform, untrimmed.
/// Giữ nguyên waveform GL48 đầy đủ của firmware nhà sản xuất 2.0.76.
#define DPREADER_BOARD_WAVEFORM_TRIM_GL48 0
/// No extra phase reordering for reader text; equal-value hold comes from the derived tables.
/// Lật trang chữ giữ nguyên thứ tự pha. Bảng dẫn xuất xử lý các giá trị bằng nhau.
#define DPREADER_BOARD_READER_TEXT_PHASE_TUNE 0
/// Giữ đích cập nhật vi sai đã xác minh và bỏ qua vùng trắng không đổi.
#define DPREADER_BOARD_PANEL_WHITE_TICK 0

/* ---- Cache và ngân sách bộ nhớ ---- */
#define DPREADER_BOARD_COVER_CACHE_DIR DPREADER_BOARD_CACHE_DIR "/covers"
/// RAM cover cache lives in the S31's PSRAM; the standalone firmware put it in the 4 MiB
/// budget with 96 slots, and trims 512 KiB before opening a book.
/// Cache bìa nằm trong PSRAM, dùng ngân sách 4 MiB và 96 khe. Thu hồi 512 KiB trước khi mở sách.
#define DPREADER_BOARD_RAM_COVER_CACHE 1
#define DPREADER_BOARD_COVER_RAM_SLOTS 96
#define DPREADER_BOARD_COVER_RAM_BUDGET_KB 4096
#define DPREADER_BOARD_CACHE_PSRAM_RESERVE_KB 2560
#define DPREADER_BOARD_CACHE_BLOCK_RESERVE_KB 1024
/// Cache phông chữ dùng 1 MiB. Ngân sách dựa trên PSRAM còn trống và dung lượng thực được ghi khi khởi động.
/// Cache được cấp phát theo nhu cầu sau khi kiểm tra bộ nhớ trống. Tổng ngân sách khác mức sử dụng thường trực.
/// TTF cache 1 MiB: these budgets follow the PSRAM headroom, and boot logs the
/// actual free size. Every cache allocates lazily after checking free space, so
/// the budgets do not sum to resident usage.
#define DPREADER_BOARD_TTF_CACHE_LIMIT_KB 1024
#define DPREADER_BOARD_TTF_CACHE_MID_KB 768
#define DPREADER_BOARD_TTF_CACHE_PRESSURE_KB 320
/// Four page-cache slots instead of two, so the reader can pre-render both neighbours.
/// Bốn khe cache trang cho phép dựng trước cả trang trước lẫn trang sau.
#define DPREADER_BOARD_PAGE_CACHE_SLOTS 4
#define DPREADER_BOARD_PAGE_CACHE_MAX_KB 1024
/// ZIP prefetch on the S31 only; it needs the PSRAM headroom.
/// Chỉ bật đọc trước ZIP trên S31 vì còn đủ bộ nhớ.
#define DPREADER_BOARD_ZIP_PREFETCH_KB 512

/* ---- Không gian tên NVS của thư viện ---- */
/// Kept byte-identical to the standalone test31-test36 firmware; renaming would clear the
/// user's shelf, favorites and bookmarks.
/// Giữ từng byte như test31 đến test36. Đổi tên sẽ làm mất truy cập tới kệ sách, mục yêu thích và dấu trang cũ.
#define DPREADER_BOARD_FAVORITES_NS "mt_favs"
#define DPREADER_BOARD_SHELF_NS "mt_shelf"
#define DPREADER_BOARD_MARKS_NS "mt_marks"
