include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/board.cmake")
dpreader_board_sources(dpreader_selected_sources "${CMAKE_CURRENT_LIST_DIR}" "metalio_ft6336.c" "metalio_touch_decode.c")
idf_component_register(
    SRCS ${dpreader_selected_sources}
    INCLUDE_DIRS "include"
    REQUIRES metalio_hw esp_driver_gpio esp_driver_i2c
    PRIV_REQUIRES metalio_hw freertos
)
