# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
# 独立热点传书组件与嵌入页面。
# Standalone hotspot transfer component and embedded page.
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/board.cmake")
if(KIIKO_BOARD STREQUAL "metalio_s31")
    # S31 另带联网对时助手；Pico 的 app 分区已顶满 4MB，不编这两个文件，保持官方固件行为。
    # The S31 also carries the network-time helpers; the Pico's application partition is full, so
    # it skips them and keeps the official firmware behaviour.
    set(kiiko_selected_sources "read_pico_transfer.c" "transfer_http_time.c" "transfer_ntp_time.c")
else()
    set(kiiko_selected_sources "read_pico_transfer.c")
endif()
idf_component_register(SRCS ${kiiko_selected_sources}
    INCLUDE_DIRS "include"
    REQUIRES kiiko_board ble_page_turner read_pico_search esp_wifi esp_netif esp_event esp_http_server esp_http_client mbedtls lwip nvs_flash fatfs esp_timer espressif__cjson
    EMBED_TXTFILES "upload.html")
