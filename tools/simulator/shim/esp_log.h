#pragma once
#include <stdio.h>
#define ESP_LOGE(tag,fmt,...) fprintf(stderr,"[%s] " fmt "\n",tag,##__VA_ARGS__)
#define ESP_LOGW ESP_LOGE
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGD(...) ((void)0)
