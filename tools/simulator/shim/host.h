#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "esp_err.h"
#define CONFIG_IDF_TARGET_ESP32S3 1
#define CONFIG_ESPTOOLPY_FLASHSIZE_16MB 1
#define CONFIG_SPIRAM 1
#define CONFIG_BT_NIMBLE_ENABLED 0
const char *sim_path(const char*);
FILE *sim_fopen(const char*,const char*);
DIR *sim_opendir(const char*);
int sim_stat(const char*,struct stat*);
int sim_mkdir(const char*,mode_t);
int sim_unlink(const char*);
int sim_rename(const char*,const char*);
int sim_open(const char*,int,...);
int sim_access(const char*,int);
#ifndef SIM_PLATFORM
#define fopen(p,m) sim_fopen(p,m)
#define opendir(p) sim_opendir(p)
#define stat(p,s) sim_stat(p,s)
#define mkdir(p,m) sim_mkdir(p,m)
#define unlink(p) sim_unlink(p)
#define remove(p) sim_unlink(p)
#define rename(p,q) sim_rename(p,q)
#define open(...) sim_open(__VA_ARGS__)
#define access(p,m) sim_access(p,m)
#endif
