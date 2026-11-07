include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/board.cmake")
dpreader_board_sources(dpreader_selected_sources "${CMAKE_CURRENT_LIST_DIR}" "metalio_power_adapter.c")
idf_component_register(
    SRCS ${dpreader_selected_sources}
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_i2c metalio_hw esp_timer
    PRIV_REQUIRES freertos esp_rom
)
