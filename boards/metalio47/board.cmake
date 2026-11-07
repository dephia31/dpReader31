# 按板加载构建元数据，不覆盖已选芯片或其他设备的配置。/ Load board metadata without forcing an existing chip/config.
file(READ "${CMAKE_CURRENT_LIST_DIR}/board.json" DPREADER_BOARD_JSON)
string(JSON DPREADER_IDF_TARGET GET "${DPREADER_BOARD_JSON}" idf_target)
string(JSON DPREADER_PROJECT_NAME GET "${DPREADER_BOARD_JSON}" project)
string(JSON DPREADER_PROJECT_VER GET "${DPREADER_BOARD_JSON}" version)
set(DPREADER_BOARD_SOURCES "${CMAKE_CURRENT_LIST_DIR}/board_hw.c")
if(NOT SDKCONFIG AND NOT CMAKE_SCRIPT_MODE_FILE)
    set(SDKCONFIG "${DPREADER_SOURCE_ROOT}/sdkconfig.${DPREADER_BOARD}")
endif()
