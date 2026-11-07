include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/board.cmake")
dpreader_board_sources(dpreader_selected_sources "${CMAKE_CURRENT_LIST_DIR}" "metalio_hw.c" "metalio_cpu.c" "metalio_ui_wake.c" "metalio_light_schedule.c" "metalio_usb.c" "metalio_ai_key.c" "metalio_memory_profile.c" "metalio_task.c")
idf_component_register(SRCS ${dpreader_selected_sources} INCLUDE_DIRS "include"
    REQUIRES nvs_flash esp_driver_i2c esp_driver_gpio esp_driver_ledc fca9555
    PRIV_REQUIRES esp_pm esp_hal_wdt riscv esp_hw_support esp_system freertos esp_timer esp_rom soc efuse esp_driver_usb_serial_jtag esp_hal_usb)
include("${CMAKE_CURRENT_LIST_DIR}/s31_sleep/s31_sleep_sdk.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/s31_sleep/s31_flash_cache_sdk.cmake")
