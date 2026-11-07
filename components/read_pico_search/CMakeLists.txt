include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/board.cmake")
# 离线搜索无需额外运行时依赖。/ Offline search has no extra runtime dependencies.
dpreader_board_sources(dpreader_selected_sources "${CMAKE_CURRENT_LIST_DIR}" "read_pico_search.c")
idf_component_register(SRCS ${dpreader_selected_sources} INCLUDE_DIRS "include")
