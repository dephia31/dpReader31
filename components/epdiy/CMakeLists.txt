
# Metalio S31 使用 LCD + AXI GDMA。/ Metalio S31 uses LCD with AXI GDMA.
set(app_sources "src/epdiy.c"
                "src/render.c"
                "src/output_lcd/render_lcd.c"
                "src/output_lcd/lcd_driver.c"
                "src/output_common/rmt_compat.c"
                "src/output_common/lut.c"
                "src/riscv_pixels.c"
                "src/output_common/line_queue.c"
                "src/output_common/render_context.c"
                "src/font.c"
                "src/displays.c"
                
                "src/highlevel.c"
                "src/board/epd_board.c"
)

if (${IDF_VERSION_MAJOR} LESS 5)
    message(FATAL_ERROR "epdiy requires ESP-IDF 5.x or newer")
endif()

set(epdiy_requires driver esp_timer esp_adc esp_lcd esp_mm)
if (${IDF_VERSION_MAJOR} GREATER 5 OR (${IDF_VERSION_MAJOR} EQUAL 5 AND ${IDF_VERSION_MINOR} GREATER 2))
    list(APPEND epdiy_requires esp_driver_i2c)
endif()
if(DEFINED IDF_VERSION_MAJOR AND IDF_VERSION_MAJOR GREATER_EQUAL 6)
    list(APPEND epdiy_requires esp_driver_gpio esp_driver_rmt esp_driver_dma)
endif()

set(epdiy_private_requires hal soc esp_hw_support esp_rom)
if(DEFINED IDF_VERSION_MAJOR AND IDF_VERSION_MAJOR GREATER_EQUAL 6)
    list(APPEND epdiy_private_requires esp_hal_dma esp_hal_rmt esp_hal_lcd esp_hal_i2s)
endif()

idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS "src/" "include"
    REQUIRES ${epdiy_requires}
    PRIV_REQUIRES ${epdiy_private_requires}
)

# formatting specifiers maybe incompatible between idf versions because of different int definitions
component_compile_options(-Wno-error=format= -Wno-format)
