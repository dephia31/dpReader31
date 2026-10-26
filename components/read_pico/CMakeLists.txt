include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/board.cmake")
set(read_pico_requires metalio_hw epdiy e0470_epaper_waveform sc7a20h cst836u read_pico_pmu fatfs fca9555 sy7636a
                   esp_driver_gpio esp_driver_i2c esp_driver_ledc esp_driver_rmt esp_driver_sdmmc)

kiiko_board_sources(kiiko_selected_sources "${CMAKE_CURRENT_LIST_DIR}" "read_pico_init.c" "read_pico_board.c" "read_pico_epd_timing.c" "metalio_haptic.c" "read_pico_sd.c" "read_pico_crashlog.c")
idf_component_register(
    SRCS ${kiiko_selected_sources}
    INCLUDE_DIRS "include"
    REQUIRES ${read_pico_requires}
    PRIV_REQUIRES bootloader_support spi_flash freertos esp_timer esp_mm
    WHOLE_ARCHIVE
)
