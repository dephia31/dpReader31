/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "esp_err.h"
#include <stdbool.h>
typedef enum {METALIO_AUDIO_IDLE,METALIO_AUDIO_STARTING,METALIO_AUDIO_PLAYING,METALIO_AUDIO_DONE,METALIO_AUDIO_ERROR} metalio_audio_state_t;
/* UI-owner calls only; no autonomous playback or peripheral reset at boot. */
esp_err_t metalio_audio_play_wav(const char *path);
/* Cancel and join before media unmount, page exit, sleep or OTA. PA off on return. */
void metalio_audio_stop(void);
metalio_audio_state_t metalio_audio_state(void);
const char *metalio_audio_message(void);
