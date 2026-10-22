# 触摸控制器：Pico 用 CST836U（0x15），Metalio 用 FT6336U（0x38）。接口同名，实现整份换。
# Touch controller: the Pico carries a CST836U (0x15), Metalio an FT6336U (0x38). Same interface,
# a different implementation per board.
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/board.cmake")
if(KIIKO_BOARD STREQUAL "metalio_s31")
    idf_component_register(
        SRCS "metalio_ft6336.c" "metalio_touch_decode.c"
        INCLUDE_DIRS "include"
        REQUIRES metalio_hw esp_driver_gpio esp_driver_i2c
        PRIV_REQUIRES metalio_hw freertos
    )
else()
    set(kiiko_selected_sources "cst836u.c")
    idf_component_register(
        SRCS ${kiiko_selected_sources}
        INCLUDE_DIRS "include"
        REQUIRES esp_driver_gpio esp_driver_i2c
        PRIV_REQUIRES freertos
    )
endif()
