# BLE HID 主机（蓝牙翻页器）。依赖 NimBLE 主机与 BT 控制器，都是 IDF 自带组件。
# BLE HID host (Bluetooth page-turner). Needs the NimBLE host and the BT controller, both
# bundled with ESP-IDF.
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/board.cmake")
set(kiiko_selected_sources "src/ble_page_turner.c")
idf_component_register(
    SRCS ${kiiko_selected_sources}
    INCLUDE_DIRS "include"
    # kiiko_board：按板分叉链路回收策略与 S31 专属的挂起/恢复接口。
    # kiiko_board: board split for stack reclaim plus the S31-only suspend/resume pair.
    REQUIRES kiiko_board bt nvs_flash esp_timer read_pico
)
