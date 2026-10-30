/* SPDX-License-Identifier: Apache-2.0 */
#include <freetype/config/ftoption.h>
#undef FT_CONFIG_OPTION_USE_ZLIB
#undef FT_CONFIG_OPTION_SYSTEM_ZLIB
#undef FT_CONFIG_OPTION_USE_BZIP2
#undef FT_CONFIG_OPTION_USE_PNG
#undef FT_CONFIG_OPTION_USE_HARFBUZZ
#undef FT_CONFIG_OPTION_USE_BROTLI
#undef FT_CONFIG_OPTION_MAC_FONTS
#undef FT_CONFIG_OPTION_SVG
#undef FT_CONFIG_OPTION_COLOR_LAYERS
#undef FT_CONFIG_OPTION_SUBPIXEL_RENDERING

/* The mono raster's pool is a local array, not an FT_Memory allocation.
 * Keep it within the16KiB main/12KiB prep stacks. FreeType sub-bands on overflow. */
#ifndef KIIKO_MONO_RENDER_POOL_SIZE
#define KIIKO_MONO_RENDER_POOL_SIZE 2048L
#endif
#undef FT_RENDER_POOL_SIZE
#define FT_RENDER_POOL_SIZE KIIKO_MONO_RENDER_POOL_SIZE
