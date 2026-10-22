#pragma once
#include "sc7a20h.h"
#include "esp_err.h"
bool read_pico_sensor_ready(void);
esp_err_t read_pico_sensor_wake(void);
