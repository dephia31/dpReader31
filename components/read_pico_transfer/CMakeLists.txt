# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
# 独立热点传书组件与嵌入页面。/ Standalone hotspot transfer component and embedded page.
idf_component_register(SRCS "read_pico_transfer.c" "transfer_http_time.c" "transfer_ntp_time.c"
    INCLUDE_DIRS "include"
    REQUIRES ble_page_turner read_pico_search esp_wifi esp_netif esp_event esp_http_server esp_http_client mbedtls lwip nvs_flash fatfs esp_timer espressif__cjson
    EMBED_TXTFILES "upload.html")
