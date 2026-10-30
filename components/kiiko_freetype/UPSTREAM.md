# FreeType TrueType monochrome subset

Upstream: https://github.com/freetype/freetype/tree/VER-2-14-3
Archive: https://github.com/freetype/freetype/archive/refs/tags/VER-2-14-3.tar.gz
Archive SHA-256: dc49de6b01a266eef4876a4dd34d9842c475d3e28ff2eff63bd2fb760ab56261

FreeType2.14.3 sources are unmodified. License: FreeType License (FTL), see vendor/LICENSE.TXT and vendor/docs/FTL.TXT. Select only TrueType/SFNT, native mono rasterizer, auto-hinter and PS names; project configuration disables external codecs, LCD/SVG and old Mac resources. Firmware uses a bounded PSRAM allocator and original-font streams, never an in-memory synthetic glyf window.

This software uses the FreeType library, copyright © the FreeType Project (www.freetype.org).

RC8 fixes the mono raster pool at2048bytes in project configuration. Upstream ftraster.c keeps this pool on the caller stack and sub-bands when it fills; its default16384byte pool exceeded the entire main-task stack inRC7. Vendor files remain unmodified. Build with -fstack-usage and compare mono output against a16384byte reference before delivery.
