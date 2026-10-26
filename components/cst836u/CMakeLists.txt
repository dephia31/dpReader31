include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/board.cmake")
kiiko_board_sources(kiiko_selected_sources "${CMAKE_CURRENT_LIST_DIR}" "metalio_ft6336.c" "metalio_touch_decode.c")
idf_component_register(
    SRCS ${kiiko_selected_sources}
    INCLUDE_DIRS "include"
    REQUIRES metalio_hw esp_driver_gpio esp_driver_i2c
    PRIV_REQUIRES metalio_hw freertos
)
