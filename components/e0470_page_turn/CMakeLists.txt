include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/board.cmake")
dpreader_board_sources(dpreader_selected_sources "${CMAKE_CURRENT_LIST_DIR}" "e0470_page_turn.c")
idf_component_register(
    SRCS ${dpreader_selected_sources}
    INCLUDE_DIRS "include"
    REQUIRES epdiy e0470_epaper_waveform
    PRIV_REQUIRES log esp_timer
)
