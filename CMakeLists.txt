# 板级选择必须在 include(project.cmake) 之前完成：芯片目标、sdkconfig、分区表与编译宏都来自这里。
# Board selection has to happen before include(project.cmake): chip target, sdkconfig, partition
# table and compile-time board macros all come from this layer.
cmake_minimum_required(VERSION 3.22)

set(KIIKO_BOARD "pico" CACHE STRING "板级层 / Board layer under boards/ (pico | metalio_s31)")
set_property(CACHE KIIKO_BOARD PROPERTY STRINGS pico metalio_s31)
# IDF 的依赖扫描是独立的 `cmake -P` 进程，读不到 CMakeCache；板名要经环境变量传下去，
# 组件 CMakeLists 统一 include cmake/board.cmake 把它取回来。
# IDF's requirement scan is a separate `cmake -P` process that cannot see CMakeCache, so the board
# name also travels through the environment; component CMakeLists include cmake/board.cmake to
# resolve it.
set(ENV{KIIKO_BOARD} "${KIIKO_BOARD}")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/board.cmake")
if(NOT EXISTS "${KIIKO_BOARD_DIR}/board.cmake")
    file(GLOB KIIKO_BOARD_CHOICES RELATIVE "${CMAKE_CURRENT_LIST_DIR}/boards"
         "${CMAKE_CURRENT_LIST_DIR}/boards/*")
    message(FATAL_ERROR
        "未知板级 / Unknown board '${KIIKO_BOARD}'. boards/ 下有 / available: ${KIIKO_BOARD_CHOICES}")
endif()
include("${KIIKO_BOARD_DIR}/board.cmake")
message(STATUS "KIIKO_BOARD=${KIIKO_BOARD} IDF_TARGET=${IDF_TARGET} PROJECT=${KIIKO_PROJECT_NAME}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)

# 板级条件编译宏。放在 project() 之前，main 与所有组件都会继承。
# Board-level compile definitions; set before project() so main and every component inherit them.
foreach(kiiko_define ${KIIKO_BOARD_DEFINES})
    add_compile_definitions("${kiiko_define}")
endforeach()

# 本地测试沿用当前正式版号；仅正式发布或明确的 OTA 验证指定新版本。
# Local tests keep the current release version; override only for publication or explicit OTA validation.
if(DEFINED ENV{${KIIKO_PROJECT_VER_ENV}} AND NOT "$ENV{${KIIKO_PROJECT_VER_ENV}}" STREQUAL "")
    set(PROJECT_VER "$ENV{${KIIKO_PROJECT_VER_ENV}}")
else()
    set(PROJECT_VER "${KIIKO_PROJECT_VER}")
endif()
# 工程名就是产物名：Read_Pico.bin / Kiikoread_Metalio_S31.bin。OTA 用 project_name 认板型。
# The project name is the artifact name: Read_Pico.bin / Kiikoread_Metalio_S31.bin. OTA identifies
# the board by project_name, so the two must not share one.
project(${KIIKO_PROJECT_NAME})

# 固件描述时间统一为 UTC，与概览页标识一致；保留已有编译缓存启动器。
# Compile app-description timestamps in UTC to match Overview; preserve any compiler-cache launcher.
idf_component_get_property(app_desc_lib esp_app_format COMPONENT_LIB)
get_target_property(app_desc_launcher ${app_desc_lib} C_COMPILER_LAUNCHER)
if(NOT app_desc_launcher)
    set(app_desc_launcher "")
endif()
set(app_desc_utc_launcher "${CMAKE_COMMAND}" -E env TZ=UTC)
if(app_desc_launcher)
    list(APPEND app_desc_utc_launcher ${app_desc_launcher})
endif()
set_property(TARGET ${app_desc_lib} PROPERTY C_COMPILER_LAUNCHER "${app_desc_utc_launcher}")
