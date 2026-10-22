# 板级门面由同一选择器决定，需求扫描和正常构建一致。/ Use the same selector for requirement discovery and normal builds.
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/board.cmake")
idf_component_register(SRCS ${KIIKO_BOARD_SOURCES} INCLUDE_DIRS "include" REQUIRES esp_common)
