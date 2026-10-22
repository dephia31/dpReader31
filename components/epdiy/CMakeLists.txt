# epdiy：两份输出后端按板二选一。
#   - 默认（本目录）：S31 的 fork，带原子行就绪、欠载诊断与 S31 GDMA/cache 适配。
#   - read_pico/：官方 rc99 原文，Pico 用。Pico 的 app 分区已满，且这套 fork 的新渲染模型对
#     它只有成本没有收益。
# epdiy: two output backends, one per board. The default tree is the S31 fork (atomic line
# readiness, underrun diagnostics, S31 GDMA/cache plumbing); read_pico/ is upstream rc99 verbatim
# for the Pico, whose application partition is full and which gains nothing from that model.
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/board.cmake")

if(KIIKO_BOARD STREQUAL "metalio_s31")
    set(epdiy_root "${CMAKE_CURRENT_LIST_DIR}")
    set(app_sources
        "src/epdiy.c"
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
else()
    set(epdiy_root "${CMAKE_CURRENT_LIST_DIR}/read_pico")
    set(app_sources
        "src/epdiy.c"
        "src/render.c"
        "src/output_lcd/render_lcd.c"
        "src/output_lcd/lcd_driver.c"
        "src/output_common/rmt_compat.c"
        "src/output_common/lut.c"
        "src/output_common/lut.S"
        "src/output_common/line_queue.c"
        "src/output_common/render_context.c"
        "src/font.c"
        "src/displays.c"
        "src/diff.S"
        "src/highlevel.c"
        "src/board/epd_board.c"
    )
endif()

set(kiiko_selected_sources "")
foreach(src ${app_sources})
    list(APPEND kiiko_selected_sources "${epdiy_root}/${src}")
endforeach()

if (${IDF_VERSION_MAJOR} LESS 5)
    message(FATAL_ERROR "epdiy requires ESP-IDF 5.x or newer")
endif()

set(epdiy_requires driver esp_timer esp_adc esp_lcd)
if (${IDF_VERSION_MAJOR} GREATER 5 OR (${IDF_VERSION_MAJOR} EQUAL 5 AND ${IDF_VERSION_MINOR} GREATER 2))
    list(APPEND epdiy_requires esp_driver_i2c)
endif()
if(DEFINED IDF_VERSION_MAJOR AND IDF_VERSION_MAJOR GREATER_EQUAL 6)
    list(APPEND epdiy_requires esp_driver_gpio esp_driver_rmt esp_driver_dma)
endif()

set(epdiy_private_requires hal soc esp_hw_support esp_rom)
if(DEFINED IDF_VERSION_MAJOR AND IDF_VERSION_MAJOR GREATER_EQUAL 6)
    list(APPEND epdiy_private_requires esp_hal_dma esp_hal_rmt esp_hal_lcd)
    # lcd_driver.c 在 IDF6 下用 esp_cache.h 维护 cache 一致性，esp_hal_lcd 的 lcd_periph.h
    # 又引用 I2S 的 LL 头；两块板的实现都编这个文件，两个依赖都按公共依赖给。
    # lcd_driver.c uses esp_cache.h for cache coherency under IDF 6 and esp_hal_lcd's lcd_periph.h
    # pulls in the I2S LL headers; both board implementations compile it, so both dependencies are
    # shared.
    list(APPEND epdiy_private_requires esp_mm esp_hal_i2s)
endif()
if(KIIKO_BOARD STREQUAL "metalio_s31")
    list(APPEND epdiy_private_requires esp_hal_i2s)
endif()

idf_component_register(
    SRCS ${kiiko_selected_sources}
    INCLUDE_DIRS "${epdiy_root}/src/" "${epdiy_root}/include"
    REQUIRES ${epdiy_requires}
    PRIV_REQUIRES ${epdiy_private_requires}
)

# formatting specifiers maybe incompatible between idf versions because of different int definitions
component_compile_options(-Wno-error=format= -Wno-format)

if(NOT KIIKO_BOARD STREQUAL "metalio_s31" AND NOT CMAKE_SCRIPT_MODE_FILE)
    # Pico：LUT 关掉 PSRAM cache 修复，因为它在 IRAM 里被高频调用。
    # Pico: the LUT disables the PSRAM cache fix because it is called at high rate from IRAM.
    set_source_files_properties("${epdiy_root}/src/output_common/lut.c"
        PROPERTIES COMPILE_OPTIONS -mno-fix-esp32-psram-cache-issue)
endif()
