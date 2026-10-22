# 板级选择的统一入口。根 CMakeLists.txt 在 include(project.cmake) 之前设置正常配置用的缓存变量，
# 并把板名导出到环境变量：IDF 的依赖扫描会以 `cmake -P` 脚本模式 include 各组件 CMakeLists，
# 脚本模式不读 CMakeCache，只有环境变量能到那里。本文件把两处的板名取回来，并重新推导板级目录。
#
# Single entry point for board selection. The root CMakeLists.txt sets the cache variable used by a
# normal configure and exports the board name to the environment: IDF's requirement scan includes
# component CMakeLists in `cmake -P` script mode, which never reads CMakeCache, so only the
# environment reaches it. This file resolves the name from either place and re-derives the board
# directory.
if(NOT KIIKO_BOARD)
    set(KIIKO_BOARD "$ENV{KIIKO_BOARD}")
endif()
if(NOT KIIKO_BOARD)
    set(KIIKO_BOARD "pico")
endif()

get_filename_component(kiiko_cmake_dir "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
get_filename_component(KIIKO_BOARD_DIR "${kiiko_cmake_dir}/../boards/${KIIKO_BOARD}" ABSOLUTE)

if(NOT EXISTS "${KIIKO_BOARD_DIR}/board.cmake")
    message(FATAL_ERROR "未知板级 / Unknown board '${KIIKO_BOARD}' (looked for ${KIIKO_BOARD_DIR}/board.cmake)")
endif()
