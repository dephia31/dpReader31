include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/board.cmake")
kiiko_board_sources(kiiko_selected_sources "${CMAKE_CURRENT_LIST_DIR}" "e0470_page_turn.c")
idf_component_register(
    SRCS ${kiiko_selected_sources}
    INCLUDE_DIRS "include"
    REQUIRES epdiy e0470_epaper_waveform
    PRIV_REQUIRES log esp_timer
)
