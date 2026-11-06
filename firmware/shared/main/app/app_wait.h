/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stddef.h>
#include <stdint.h>
/* Absolute monotonic deadlines; only opt-in pages can extend the legacy wait. */
static inline uint32_t app_wait_ms(int64_t now,uint32_t legacy,int eligible,
                                  int pending,const int64_t *deadlines,size_t count) {
    if(pending)return 0;
    if(!eligible)return legacy;
    int64_t until=now+200;
    for(size_t i=0;i<count;i++)if(deadlines[i]<until)until=deadlines[i];
    return until<=now?0:(uint32_t)(until-now);
}
