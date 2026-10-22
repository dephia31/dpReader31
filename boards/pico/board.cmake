# 按板加载构建元数据，不覆盖已选芯片或其他设备的配置。/ Load board metadata without forcing an existing chip/config.
file(READ "${CMAKE_CURRENT_LIST_DIR}/board.json" KIIKO_BOARD_JSON)
string(JSON KIIKO_IDF_TARGET GET "${KIIKO_BOARD_JSON}" idf_target)
string(JSON KIIKO_PROJECT_NAME GET "${KIIKO_BOARD_JSON}" project)
string(JSON KIIKO_PROJECT_VER GET "${KIIKO_BOARD_JSON}" version)
set(KIIKO_BOARD_SOURCES "${CMAKE_CURRENT_LIST_DIR}/board_hw.c")
if(NOT SDKCONFIG AND NOT CMAKE_SCRIPT_MODE_FILE)
    set(SDKCONFIG "${KIIKO_SOURCE_ROOT}/sdkconfig.${KIIKO_BOARD}")
endif()
