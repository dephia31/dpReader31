# 板级 BSP：两块板的 I2C 接线、EPD 板定义、扫描时序与 TF 卡 bring-up 实现不同，按板各选一份源文件。
# 共享的只有崩溃日志；其余同名接口由每板自己的 .c 提供，上层 include 不变。
# Board BSP: the I2C setup, EPD board definition, scan timing and card bring-up differ per board, so
# each board compiles its own sources. Only the crash log is shared; both implementations export
# the same header, so callers do not change.
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/board.cmake")
set(read_pico_requires epdiy e0470_epaper_waveform sc7a20h cst836u read_pico_pmu fatfs sy7636a fca9555
                   esp_driver_gpio esp_driver_i2c esp_driver_ledc esp_driver_rmt esp_driver_sdmmc)

if(KIIKO_BOARD STREQUAL "metalio_s31")
    # S31：厂商 LCD + AXI GDMA 路径、TPS65185 电源轨、FT6336U 触摸、四线 SDMMC。
    # 蜂鸣器接口由 metalio_haptic.c 接住（改成振动或 NOT_SUPPORTED）。
    # S31: vendor LCD + AXI GDMA path, TPS65185 rails, FT6336U touch, four-bit SDMMC. The buzzer
    # interface is served by metalio_haptic.c (vibration or NOT_SUPPORTED).
    list(APPEND read_pico_requires metalio_hw)
    set(kiiko_selected_sources
        "metalio/read_pico_init.c"
        "metalio/read_pico_board.c"
        "metalio/read_pico_epd_timing.c"
        "metalio/metalio_haptic.c"
        "metalio/read_pico_sd.c"
        "read_pico_crashlog.c")
else()
    # Pico：LCD 路径 + SY7636A/FCA9555 电源轨、CST836U 触摸、单线 SD、Zbit flash HPM 补丁。
    # Pico: LCD path with SY7636A/FCA9555 rails, CST836U touch, one-bit SD and the Zbit HPM patch.
    list(APPEND read_pico_requires pwm_audio)
    set(kiiko_selected_sources
        "read_pico_init.c"
        "read_pico_board.c"
        "read_pico_epd_timing.c"
        "read_pico_buzzer.c"
        "buzzer_score.c"
        "buzzer_duty_map.c"
        "buzzer_1bit.c"
        "read_pico_sd.c"
        "read_pico_flash_hpm.c"
        "read_pico_crashlog.c")
endif()

idf_component_register(
    SRCS ${kiiko_selected_sources}
    INCLUDE_DIRS "include"
    REQUIRES ${read_pico_requires}
    PRIV_REQUIRES bootloader_support spi_flash freertos esp_timer
    # flash_hpm_zb.c 只定义了一个覆盖 IDF weak 符号的表，没有任何
    # 代码引用它，不整包链接的话这个目标文件会被丢掉，覆盖就不生效。
    WHOLE_ARCHIVE
)
