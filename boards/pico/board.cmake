# 板级：小纸 Pico（Read Pico）—— ESP32-S3 + 4.7 寸 1216x684 单色面板。
# Board: Read Pico — ESP32-S3 with the 4.7" 1216x684 mono panel.
#
# 本文件在 include(project.cmake) 之前被根 CMakeLists.txt 引入，只决定「怎么构建」；
# 板级身份与硬件能力在同目录的 kiiko_board_config.h，板级驱动在同目录的 board_hw.c。
# This file is included before include(project.cmake) and only decides how to build. Board
# identity and capabilities live in kiiko_board_config.h; board drivers in board_hw.c.

set(KIIKO_PROJECT_NAME "Read_Pico")
set(KIIKO_PROJECT_VER "0.3.3-rc99")
set(KIIKO_PROJECT_VER_ENV "PICO_PROJECT_VER")

# 芯片目标跟着板子走：Pico 是 Xtensa 的 esp32s3，Metalio 是 RISC-V 的 esp32s31。
# 工具链、PSRAM 时序与 epdiy 的输出路径都不同，只改引脚和 sdkconfig 不够。
# The chip target follows the board: Pico is Xtensa esp32s3, Metalio is RISC-V esp32s31.
# Toolchain, PSRAM timing and epdiy's output path all differ; pins alone are not enough.
set(IDF_TARGET "esp32s3" CACHE STRING "IDF target for board ${KIIKO_BOARD}" FORCE)

# C 代码可见的板级宏；KIIKO_BOARD_METALIO47 是上游 rc99 已经用到的分支开关。
# Board macros visible to C; KIIKO_BOARD_METALIO47 is the switch upstream rc99 already branches on.
set(KIIKO_BOARD_DEFINES "KIIKO_BOARD_PICO=1" "KIIKO_BOARD_METALIO47=0")

# 每块板独立的 sdkconfig，否则两块板互相改写对方配置；默认落在仓库根。
# A per-board sdkconfig, or the two boards rewrite each other's configuration; it lives at the
# repository root by default.
if(NOT SDKCONFIG)
    set(SDKCONFIG "${CMAKE_CURRENT_LIST_DIR}/../../sdkconfig.pico")
endif()
# 根 sdkconfig.defaults 放两板共性，板级文件在后、因此板级值优先。
# The root sdkconfig.defaults holds the shared half; the board file comes after it and wins.
if(NOT SDKCONFIG_DEFAULTS)
    set(SDKCONFIG_DEFAULTS
        "${CMAKE_CURRENT_LIST_DIR}/../../sdkconfig.defaults"
        "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults")
endif()

# 板级驱动源文件（编进 components/kiiko_board）。
# Board driver sources, compiled into components/kiiko_board.
set(KIIKO_BOARD_SOURCES "${CMAKE_CURRENT_LIST_DIR}/board_hw.c")
