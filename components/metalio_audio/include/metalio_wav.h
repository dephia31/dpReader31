/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
typedef struct {uint32_t data_bytes;uint16_t channels;} metalio_wav_info_t;
/* Leaves file at data start. Accept only bounded RIFF PCM16/16kHz mono/stereo. */
bool metalio_wav_open(FILE *file,metalio_wav_info_t *info);
/* BH1098G consumes stereo32 slots; volume is linear0..100 percent. */
void metalio_audio_expand(const int16_t *source,int32_t *slots,unsigned frames,unsigned channels,unsigned volume);
/* Complete bounded UART line only; reject truncation and suffix collisions. */
bool metalio_audio_mode_ack(const char *line);
