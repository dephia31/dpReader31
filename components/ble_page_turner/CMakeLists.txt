include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/board.cmake")
# BLE HID 主机（蓝牙翻页器）。依赖 NimBLE 主机与 BT 控制器，都是 IDF 自带组件。
# BLE HID host (Bluetooth page-turner). Needs the NimBLE host and the BT controller, both
# bundled with ESP-IDF.
kiiko_board_sources(kiiko_selected_sources "${CMAKE_CURRENT_LIST_DIR}" "src/ble_page_turner.c")
idf_component_register(
    SRCS ${kiiko_selected_sources}
    INCLUDE_DIRS "include"
    REQUIRES bt nvs_flash esp_timer read_pico
)
