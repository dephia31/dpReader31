/* SPDX-License-Identifier: Apache-2.0
 * 中文：Metalio 震动反馈，绝不驱动 Pico 的 GPIO2 蜂鸣器。
 * English: Metalio vibration feedback never drives Pico's GPIO2 buzzer.
 */
#include "read_pico_buzzer.h"
#include "metalio_hw.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
esp_err_t read_pico_buzzer_effect(void) { return metalio_haptic_click(); }
esp_err_t read_pico_buzzer_tone(uint32_t f,uint32_t ms) { (void)f;(void)ms; return read_pico_buzzer_effect(); }
esp_err_t read_pico_buzzer_score_play(void) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t read_pico_buzzer_score_stop(void) { return ESP_OK; }
bool read_pico_buzzer_score_busy(void) { return false; }
esp_err_t read_pico_buzzer_probe(read_pico_buzzer_probe_t p) { (void)p; return ESP_ERR_NOT_SUPPORTED; }
void read_pico_buzzer_out_stats_get(read_pico_buzzer_out_stats_t *s) { memset(s,0,sizeof(*s)); }
uint32_t read_pico_buzzer_pwm_carrier_hz(void) { return 0; }
uint32_t read_pico_buzzer_pwm_carrier_hz_nominal(void) { return 0; }
void read_pico_buzzer_duty_map_get(buzzer_duty_map_t *map) { memset(map,0,sizeof(*map)); }
esp_err_t read_pico_buzzer_duty_map_set(const buzzer_duty_map_t *map) { (void)map; return ESP_ERR_NOT_SUPPORTED; }
