/* SPDX-License-Identifier: LGPL-3.0-or-later
 * 中文：S31 Core1 PIE像素运算；仅处理完整对齐块，边界由C负责。
 * English: S31 Core1 PIE pixel operations; C owns alignment and tails.
 * 指令形式参考ESP-IDF test_pie_routines.S及官方RISC-V PIE实现；像素算法沿用本组件。
 * Instruction forms follow ESP-IDF test_pie_routines.S and RISC-V PIE kernels; pixel algorithms follow this component.
 * 冻结：不迁移任务、不改DMA/波形；不得从Core0或ISR直接调用。
 * Frozen: No task migration, DMA or waveform changes; never call from Core0/ISR.
 */
#include "sdkconfig.h"
#if CONFIG_IDF_TARGET_ESP32S31
    .section .iram1,"ax",@progbits
    .align 4
    .global epd_pie_mask_blocks
    .type epd_pie_mask_blocks,@function
epd_pie_mask_blocks:
    // a0行，a1掩码，a2块数；加载后才原位存回。
    // a0=line,a1=mask,a2=blocks; load before the in-place store.
1:
    esp.vld.128.ip q0,a0,0
    esp.vld.128.ip q1,a1,16
    esp.andq q0,q0,q1
    esp.vst.128.ip q0,a0,16
    addi a2,a2,-1
    bnez a2,1b
    ret
    .size epd_pie_mask_blocks,.-epd_pie_mask_blocks

    .align 4
    .global epd_pie_interlace_blocks
    .type epd_pie_interlace_blocks,@function
epd_pie_interlace_blocks:
    // a0目标，a1历史，a2交织输出，a3列脏标记，a4块数。
    // a0=target,a1=previous,a2=interlaced,a3=dirty columns,a4=blocks.
    mv t3,a4
    la t4,.Lpie_low_nibbles
    esp.vld.128.ip q5,t4,0
    esp.movx.r.sar t5
    li t4,4
    esp.movx.w.sar t4
    esp.zero.q q7
2:
    esp.vld.128.ip q0,a0,16
    esp.vld.128.ip q1,a1,16
    esp.xorq q6,q0,q1
    esp.orq q7,q7,q6
    esp.vld.128.ip q4,a3,0
    esp.orq q4,q4,q6
    esp.vst.128.ip q4,a3,16
    esp.notq q4,q5
    // 先取低半字节再用无溢出的U8加法左移，避免旧VSL.32的饱和行为。
    // Isolate low nibbles and double within U8 range,avoiding old VSL.32 saturation.
    esp.andq q2,q0,q5
    esp.vadd.u8 q2,q2,q2
    esp.vadd.u8 q2,q2,q2
    esp.vadd.u8 q2,q2,q2
    esp.vadd.u8 q2,q2,q2
    esp.andq q3,q1,q5
    esp.orq q2,q2,q3
    esp.andq q0,q0,q4
    esp.vsr.u32 q1,q1
    esp.andq q1,q1,q5
    esp.orq q0,q0,q1
    esp.vzip.8 q2,q0
    esp.vst.128.ip q2,a2,16
    esp.vst.128.ip q0,a2,16
    addi t3,t3,-1
    bnez t3,2b
    esp.movi.32.a q7,a0,0
    esp.movi.32.a q7,a1,1
    or a0,a0,a1
    esp.movi.32.a q7,a1,2
    or a0,a0,a1
    esp.movi.32.a q7,a1,3
    or a0,a0,a1
    esp.movx.w.sar t5
    ret
    .size epd_pie_interlace_blocks,.-epd_pie_interlace_blocks

    .section .rodata.pie_pixels,"a",@progbits
    .align 4
.Lpie_low_nibbles:
    .word 0x0f0f0f0f,0x0f0f0f0f,0x0f0f0f0f,0x0f0f0f0f
#endif
