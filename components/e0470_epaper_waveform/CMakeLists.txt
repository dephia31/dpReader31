# 面板波形表。两块板编译同一份源文件：厂家 2.0.76 的完整 48 相表放在 metalio/ 下，
# 由 E0470_WAVEFORM_TRIM（按板取值，见 include/e0470_epaper_waveform.h）决定是否裁剪。
# Panel waveform tables. Both boards compile the same sources: the vendor 2.0.76 full 48-phase
# tables live under metalio/ and E0470_WAVEFORM_TRIM (per board, see the public header) decides
# whether they are trimmed.
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/board.cmake")

set(kiiko_selected_sources "e0470_epaper_waveform.c" "e0470_waveform_trim.c")
idf_component_register(
    SRCS ${kiiko_selected_sources}
    INCLUDE_DIRS "metalio" "include"
    PRIV_INCLUDE_DIRS "metalio" "waveforms"
    REQUIRES epdiy
)
