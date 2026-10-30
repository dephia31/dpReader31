/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdint.h>
/* KOReader/CREngine-style gamma 1.0 experiment: retain raw coverage until
 * compositing into the panel framebuffer. Never lift zero coverage/background.
 * Independently implemented; no CREngine source or tables are incorporated. */
static inline uint8_t ttf_aa_coverage(uint8_t alpha) {
    return alpha;
}
