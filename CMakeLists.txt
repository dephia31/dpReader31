# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/board.cmake")
set(DPREADER_BOARD "${DPREADER_BOARD}" CACHE STRING "Firmware device: metalio47 (dpReader_metalio47)")
set_property(CACHE DPREADER_BOARD PROPERTY STRINGS metalio47 dpReader_metalio47)
if(DEFINED IDF_TARGET AND NOT IDF_TARGET STREQUAL DPREADER_IDF_TARGET)
    message(FATAL_ERROR "${DPREADER_BOARD} requires ${DPREADER_IDF_TARGET}; use a separate build/config directory")
endif()
if(DEFINED ENV{IDF_TARGET} AND NOT "$ENV{IDF_TARGET}" STREQUAL "${DPREADER_IDF_TARGET}")
    message(FATAL_ERROR "IDF_TARGET environment conflicts with ${DPREADER_BOARD}")
endif()
set(IDF_TARGET "${DPREADER_IDF_TARGET}" CACHE STRING "Selected chip")
set(ENV{DPREADER_BOARD} "${DPREADER_BOARD}")
if(NOT DEFINED SDKCONFIG_DEFAULTS)
    set(SDKCONFIG_DEFAULTS "${DPREADER_BOARD_ROOT}/sdkconfig.defaults")
endif()
add_compile_definitions(DPREADER_BOARD_METALIO47=${DPREADER_BOARD_METALIO47})
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)

# 本地测试使用下一正式版的递增 RC 编号；构建器传入与应用描述一致的版本。
# Local tests use increasing RC numbers for the next release; the build driver supplies the app descriptor version.
if(DEFINED ENV{DPREADER_PROJECT_VER} AND NOT "$ENV{DPREADER_PROJECT_VER}" STREQUAL "")
    set(PROJECT_VER "$ENV{DPREADER_PROJECT_VER}")
elseif(DEFINED ENV{PICO_PROJECT_VER} AND NOT "$ENV{PICO_PROJECT_VER}" STREQUAL "")
    set(PROJECT_VER "$ENV{PICO_PROJECT_VER}")
else()
    set(PROJECT_VER "${DPREADER_PROJECT_VER}")
endif()
set(DPREADER_EDITION "full" CACHE STRING "Firmware edition: full or love-book")
if(DPREADER_EDITION STREQUAL "love-book")
    set(DPREADER_LOVE_BOOK 1)
elseif(DPREADER_EDITION STREQUAL "full")
    set(DPREADER_LOVE_BOOK 0)
else()
    message(FATAL_ERROR "Unknown DPREADER_EDITION: ${DPREADER_EDITION}")
endif()
add_compile_definitions(DPREADER_LOVE_BOOK=${DPREADER_LOVE_BOOK} DPREADER_FIRMWARE_VERSION="${PROJECT_VER}")
project(${DPREADER_PROJECT_NAME})

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

# panic 包装器的链接选项在 main/CMakeLists.txt 里挂到 main 组件上：写在 project() 之后
# 用 idf_build_set_property 设全局链接选项已经太晚，链接器看不到 --wrap。
# The panic wrapper's link option hangs off the main component in main/CMakeLists.txt: setting a
# global link option after project() is too late for the linker to see --wrap.

if(DPREADER_BOARD_METALIO47)
    include("${CMAKE_CURRENT_LIST_DIR}/components/ble_page_turner/nimble_identity_sdk.cmake")
endif()
