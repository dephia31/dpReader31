# FreeType2.14.3: preserve upstream FTL and compile a bounded TrueType mono subset.
idf_component_register(SRCS
    "vendor/src/base/ftbase.c" "vendor/src/base/ftinit.c"
    "vendor/src/base/ftsystem.c" "vendor/src/base/ftdebug.c"
    "vendor/src/base/ftbitmap.c" "vendor/src/base/ftmm.c" "vendor/src/base/fttype1.c"
    "vendor/src/truetype/truetype.c" "vendor/src/sfnt/sfnt.c"
    "vendor/src/raster/raster.c" "vendor/src/autofit/autofit.c"
    "vendor/src/psnames/psnames.c"
    INCLUDE_DIRS "vendor/include" ".")
target_compile_definitions(${COMPONENT_LIB} PRIVATE FT2_BUILD_LIBRARY
    FT_CONFIG_MODULES_H="kiiko_ftmodule.h" FT_CONFIG_OPTIONS_H="kiiko_ftoption.h")

# Keep compiler stack-frame evidence for the embedded raster; do not grow task stacks.
target_compile_options(${COMPONENT_LIB} PRIVATE -fstack-usage)
