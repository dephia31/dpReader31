# FreeType2.14.3: preserve upstream FTL and compile a bounded TrueType mono subset.
execute_process(COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/prepare_autofit.py"
    "${CMAKE_CURRENT_BINARY_DIR}/autofit" RESULT_VARIABLE prepare_result)
if(NOT prepare_result EQUAL 0)
    message(FATAL_ERROR "Unable to prepare bounded FreeType autofit scratch")
endif()
idf_component_register(SRCS
    "vendor/src/base/ftbase.c" "vendor/src/base/ftinit.c"
    "vendor/src/base/ftsystem.c" "vendor/src/base/ftdebug.c"
    "vendor/src/base/ftbitmap.c" "vendor/src/base/ftmm.c" "vendor/src/base/fttype1.c"
    "vendor/src/truetype/truetype.c" "vendor/src/sfnt/sfnt.c"
    "vendor/src/smooth/smooth.c" "vendor/src/raster/raster.c" "kiiko_autofit.c"
    "vendor/src/psnames/psnames.c"
    INCLUDE_DIRS "vendor/include" "."
    PRIV_INCLUDE_DIRS "vendor/src/autofit" "${CMAKE_CURRENT_BINARY_DIR}/autofit")
target_compile_definitions(${COMPONENT_LIB} PRIVATE FT2_BUILD_LIBRARY
    FT_CONFIG_MODULES_H="kiiko_ftmodule.h" FT_CONFIG_OPTIONS_H="kiiko_ftoption.h")

# Keep compiler stack-frame evidence for the embedded raster; do not grow task stacks.
target_compile_options(${COMPONENT_LIB} PRIVATE -fstack-usage -Werror=frame-larger-than=4096)
