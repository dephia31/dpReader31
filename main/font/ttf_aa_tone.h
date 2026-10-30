/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdint.h>
/* Four coverage levels: drop faint haze and keep a solid black core.
 * No spatial dithering; the native gray waveform still drives the panel. */
static inline uint8_t ttf_aa_coverage(uint8_t alpha) {
    return alpha<32?0:alpha<104?85:alpha<176?170:255;
}
