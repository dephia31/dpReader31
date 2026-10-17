# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)

# 本地测试沿用当前正式版号；仅正式发布或明确的 OTA 验证指定新版本。
# Local tests keep the current release version; override only for publication or explicit OTA validation.
if(DEFINED ENV{METALIO_PROJECT_VER} AND NOT "$ENV{METALIO_PROJECT_VER}" STREQUAL "")
    set(PROJECT_VER "$ENV{METALIO_PROJECT_VER}")
else()
set(PROJECT_VER "0.3.3-rc90-metalio-test30")
endif()
project(Kiikoread_Metalio_S31)

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
