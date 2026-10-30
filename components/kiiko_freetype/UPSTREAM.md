# FreeType TrueType monochrome subset

Upstream: https://github.com/freetype/freetype/tree/VER-2-14-3
Archive: https://github.com/freetype/freetype/archive/refs/tags/VER-2-14-3.tar.gz
Archive SHA-256: dc49de6b01a266eef4876a4dd34d9842c475d3e28ff2eff63bd2fb760ab56261

FreeType2.14.3 sources are unmodified. License: FreeType License (FTL), see vendor/LICENSE.TXT and vendor/docs/FTL.TXT. Select only TrueType/SFNT, native mono rasterizer, auto-hinter and PS names; project configuration disables external codecs, LCD/SVG and old Mac resources. Firmware uses a bounded PSRAM allocator and original-font streams, never an in-memory synthetic glyf window.

This software uses the FreeType library, copyright © the FreeType Project (www.freetype.org).

RC8 fixes the mono raster pool at2048bytes in project configuration. Upstream ftraster.c keeps this pool on the caller stack and sub-bands when it fills; its default16384byte pool exceeded the entire main-task stack inRC7. Vendor files remain unmodified. Build with -fstack-usage and compare mono output against a16384byte reference before delivery.

1.1.10RC1 adds unmodified upstream smooth raster sources from the same verified archive for hinted grayscale. Both renderers retain the 2048-byte project pool and bounded dynamic allocator. No LCD/subpixel rendering is enabled.

RC7 adds a project-generated `kiiko_afhints.h` overlay and `kiiko_autofit.c` wrapper. Original vendor files stay byte-identical. The overlay reduces inline capacities for segments18→4, edges12→4, points96→8 and contours8→2. Larger glyphs use FreeType's existing dynamic-growth path through the bounded FT_Memory allocator. Hinting algorithms and glyph limits are unchanged; all generated declarations and their consumers compile in the same wrapper translation unit. Generation fails if expected macro anchors change. This addresses a device-confirmed stack protection fault at af_latin_metrics_init_widths: nested target frames7824+6912bytes exceeded the16KiB main stack. Fixed target frames are2288+1392bytes, with no task-stack growth.

Embedded builds now reject FreeType function stack frames larger than4096bytes at compile time, in addition to emitting stack-usage evidence. This is a per-function guard; nested call chains still require review.
