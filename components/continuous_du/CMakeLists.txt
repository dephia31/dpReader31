include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/board.cmake")
kiiko_board_sources(kiiko_selected_sources "${CMAKE_CURRENT_LIST_DIR}" "continuous_du.c")
idf_component_register(
    SRCS ${kiiko_selected_sources}
    INCLUDE_DIRS "include"
    REQUIRES epdiy e0470_epaper_waveform read_pico
)
