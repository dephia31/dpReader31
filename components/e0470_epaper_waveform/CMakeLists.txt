include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/board.cmake")
dpreader_board_sources(dpreader_selected_sources "${CMAKE_CURRENT_LIST_DIR}" "e0470_epaper_waveform.c" "e0470_waveform_trim.c")
idf_component_register(
    SRCS ${dpreader_selected_sources}
    INCLUDE_DIRS "metalio" "include"
    PRIV_INCLUDE_DIRS "metalio" "waveforms"
    REQUIRES epdiy
)
