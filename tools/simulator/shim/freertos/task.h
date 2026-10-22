#pragma once
#include "FreeRTOS.h"
typedef struct sim_task *TaskHandle_t;
int xTaskCreatePinnedToCore(void(*fn)(void*),const char*,unsigned,void*,unsigned,TaskHandle_t*,int);
int xTaskCreate(void(*fn)(void*),const char*,unsigned,void*,unsigned,TaskHandle_t*);
void vTaskDelete(TaskHandle_t);
void vTaskDelay(unsigned);
unsigned ulTaskNotifyTake(int,unsigned);
void xTaskNotifyGive(TaskHandle_t);
unsigned uxTaskGetStackHighWaterMark(TaskHandle_t);
TickType_t xTaskGetTickCount(void);
