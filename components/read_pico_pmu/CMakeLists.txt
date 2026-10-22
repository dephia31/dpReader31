# 板级源选择：S31 用 BQ27220/CX25601N/PCF8563/TCA 适配器，Pico 保留 CW32L010 协议主机。
# Board source selection: the S31 uses its gauge/charger/RTC/key adapter, the Pico keeps the
# CW32L010 protocol host. Both implement the same snapshot API in include/read_pico_pmu.h.
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/board.cmake")
if(KIIKO_BOARD STREQUAL "metalio_s31")
    set(kiiko_selected_sources "metalio_power_adapter.c")
    set(kiiko_selected_requires esp_driver_i2c metalio_hw esp_timer)
    set(kiiko_selected_priv_requires freertos esp_rom)
else()
    set(kiiko_selected_sources "read_pico_pmu.c" "pico_soc_filter.c")
    set(kiiko_selected_requires esp_driver_i2c)
    set(kiiko_selected_priv_requires freertos esp_rom esp_timer)
endif()

idf_component_register(
    SRCS ${kiiko_selected_sources}
    INCLUDE_DIRS "include"
    REQUIRES ${kiiko_selected_requires}
    PRIV_REQUIRES ${kiiko_selected_priv_requires}
)
