# 离线搜索无需额外运行时依赖。/ Offline search has no extra runtime dependencies.
set(kiiko_selected_sources "read_pico_search.c")
idf_component_register(SRCS ${kiiko_selected_sources} INCLUDE_DIRS "include")
