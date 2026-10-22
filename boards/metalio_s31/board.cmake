# 板级：Metalio E-INK4 Plus V1.5 / E0470A14-AF —— ESP32-S31 + 4.7 寸 1216x684 单色面板。
# Board: Metalio E-INK4 Plus V1.5 / E0470A14-AF — ESP32-S31 with the 4.7" 1216x684 mono panel.
#
# 本文件在 include(project.cmake) 之前被根 CMakeLists.txt 引入，只决定「怎么构建」；
# 板级身份与硬件能力在同目录的 kiiko_board_config.h，板级驱动在同目录的 board_hw.c。
# This file is included before include(project.cmake) and only decides how to build. Board
# identity and capabilities live in kiiko_board_config.h; board drivers in board_hw.c.

set(KIIKO_PROJECT_NAME "Kiikoread_Metalio_S31")
set(KIIKO_PROJECT_VER "0.3.3-rc90-metalio-test36")
set(KIIKO_PROJECT_VER_ENV "METALIO_PROJECT_VER")

# S31 是 RISC-V；当前 SDK 需要带 esp32s31 支持的 v6.1 提交（见 README）。
# The S31 is RISC-V; this needs the v6.1 commit carrying esp32s31 support (see README).
set(IDF_TARGET "esp32s31" CACHE STRING "IDF target for board ${KIIKO_BOARD}" FORCE)

# KIIKO_BOARD_METALIO47=1 同时打开上游 rc99 里为 Metalio 板预留的测时日志。
# KIIKO_BOARD_METALIO47=1 also enables the latency logging upstream rc99 reserved for this board.
set(KIIKO_BOARD_DEFINES "KIIKO_BOARD_METALIO47=1")

if(NOT SDKCONFIG)
    set(SDKCONFIG "${CMAKE_CURRENT_LIST_DIR}/../../sdkconfig.metalio_s31")
endif()
if(NOT SDKCONFIG_DEFAULTS)
    set(SDKCONFIG_DEFAULTS
        "${CMAKE_CURRENT_LIST_DIR}/../../sdkconfig.defaults"
        "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults")
endif()

set(KIIKO_BOARD_SOURCES "${CMAKE_CURRENT_LIST_DIR}/board_hw.c")
