# 板级选择层的组件外壳：只编当前板 boards/<KIIKO_BOARD>/ 下的 board_hw.c。
# The board selection layer's component shell: it compiles only the current board's board_hw.c.
#
# 冻结：组件本身不含板级代码；板级身份在同目录 ../../boards/<板名>/kiiko_board_config.h。
# Frozen: no board-specific code lives here; board identity stays in boards/<name>/.
#
# 注意：IDF 的依赖扫描会以脚本模式 include 本文件，那里读不到 CMakeCache，板名由
# cmake/board.cmake 从环境变量取回。
# Note: IDF's requirement scan includes this file in script mode, where CMakeCache is absent;
# cmake/board.cmake resolves the board name from the environment there.
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/board.cmake")

set(kiiko_board_sources "")
if(EXISTS "${KIIKO_BOARD_DIR}/board_hw.c")
    list(APPEND kiiko_board_sources "${KIIKO_BOARD_DIR}/board_hw.c")
endif()

idf_component_register(
    SRCS ${kiiko_board_sources}
    # 板级头文件目录跟着板型走，共享代码因此能 #include "kiiko_board_config.h"。
    # The board directory joins the include path so shared code can include its config header.
    INCLUDE_DIRS "include" "${KIIKO_BOARD_DIR}"
    REQUIRES esp_common
)
