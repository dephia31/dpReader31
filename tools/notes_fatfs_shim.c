/* SPDX-License-Identifier: Apache-2.0
 * Match ESP-IDF FatFS: rename must fail when destination already exists. */
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
int notes_fatfs_rename(const char *src,const char *dst) {
    struct stat st;if(!stat(dst,&st)){errno=EEXIST;return -1;}return rename(src,dst);
}

/* A mounted root can be stat'ed, but mkdir need not report EEXIST. */
int notes_fatfs_mkdir(const char *path,mode_t mode) {
    struct stat st;if(!stat(path,&st)){errno=ENOENT;return -1;}
    return mkdir(path,mode);
}
