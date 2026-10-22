#pragma once
#include <stdint.h>
typedef unsigned DWORD; typedef struct{unsigned csize;unsigned n_fatent;} FATFS;
#define FR_OK 0
int f_getfree(const char*,DWORD*,FATFS**);
