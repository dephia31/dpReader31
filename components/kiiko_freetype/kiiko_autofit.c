/* SPDX-License-Identifier: Apache-2.0
 * Keep large automatic hint buffers off ESP task stacks. Upstream grows these
 * small inline buffers via the existing bounded FT_Memory PSRAM allocator.
 * Original FreeType files and hinting algorithms remain unmodified.
 */
#define FT_MAKE_OPTION_SINGLE_OBJECT
#ifdef KIIKO_AUTOFIT_STACK_REFERENCE
#include "vendor/src/autofit/afhints.h"
#else
#include "kiiko_afhints.h"
#endif
#include "vendor/src/autofit/autofit.c"
