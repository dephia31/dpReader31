include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/board.cmake")
kiiko_board_sources(kiiko_selected_sources "${CMAKE_CURRENT_LIST_DIR}" "sc7a20h.c")
idf_component_register(
    SRCS ${kiiko_selected_sources}
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio esp_driver_i2c
    PRIV_REQUIRES esp_timer freertos esp_hw_support
)
