# Dùng cùng cơ chế chọn bo khi quét phụ thuộc và khi build.
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/board.cmake")
idf_component_register(SRCS ${DPREADER_BOARD_SOURCES} INCLUDE_DIRS "include" REQUIRES esp_common)
