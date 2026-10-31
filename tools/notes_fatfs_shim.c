/* SPDX-License-Identifier: Apache-2.0
 * Match ESP-IDF FatFS: rename must fail when destination already exists. */
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
int notes_fatfs_rename(const char *src,const char *dst) {
    struct stat st;if(!stat(dst,&st)){errno=EEXIST;return -1;}return rename(src,dst);
}
