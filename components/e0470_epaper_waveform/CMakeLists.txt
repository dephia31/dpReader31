include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/board.cmake")
kiiko_board_sources(kiiko_selected_sources "${CMAKE_CURRENT_LIST_DIR}" "e0470_epaper_waveform.c" "e0470_waveform_trim.c")
idf_component_register(
    SRCS ${kiiko_selected_sources}
    INCLUDE_DIRS "metalio" "include"
    PRIV_INCLUDE_DIRS "metalio" "waveforms"
    REQUIRES epdiy
)
