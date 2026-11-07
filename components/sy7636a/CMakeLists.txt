include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/board.cmake")
dpreader_board_sources(dpreader_selected_sources "${CMAKE_CURRENT_LIST_DIR}" "sy7636a.c")
idf_component_register(
    SRCS ${dpreader_selected_sources}
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio esp_driver_i2c
    PRIV_REQUIRES esp_timer freertos esp_rom
)
