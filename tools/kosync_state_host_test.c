/* SPDX-License-Identifier: Apache-2.0 */
#include "kosync_state.h"
#include <assert.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#undef rename
int rename(const char*,const char*);
static bool fat_mode;
int fake_rename(const char*a,const char*b){if(fat_mode&&!access(b,F_OK)){errno=EEXIST;return -1;}return rename(a,b);}
static time_t now=1800000000;
time_t fake_time(time_t*p){if(p)*p=now;return now;}
int main(void){
 const char*path="/sdcard/books/test.epub";bool known;
 uint64_t h=UINT64_C(14695981039346656037);
 for(const char*p=path;*p;p++)h=(h^(unsigned char)*p)*UINT64_C(1099511628211);
 char file[512],tmp[520];snprintf(file,sizeof(file),KOSYNC_STATE_ROOT "/%016llx.bin",(unsigned long long)h);
 snprintf(tmp,sizeof(tmp),"%s.tmp",file);
 kosync_state_begin(path,123,10,100);assert(kosync_state_timestamp(path,10,100)==0);
 kosync_state_touch(path,123,10,100);assert(access(file,F_OK));
 now++;kosync_state_touch(path,123,10,120);assert(access(file,F_OK));assert(kosync_state_timestamp(path,10,120)==now);
 kosync_state_commit(path);assert(!access(file,F_OK));
 struct stat a,b;assert(!stat(file,&a));kosync_state_commit(path);assert(!stat(file,&b));assert(a.st_mtime==b.st_mtime&&a.st_ino==b.st_ino);
 now+=30;kosync_state_begin(path,123,10,120);assert(kosync_state_timestamp(path,10,120)==now-30);
 assert(kosync_state_remote_newer(now-30,now-10,&known)&&known);
 assert(!kosync_state_remote_newer(now,now-30,&known)&&known);
 assert(!kosync_state_remote_newer(0,now,&known)&&!known);
 assert(!kosync_state_remote_newer(now,now+10000,&known)&&!known);
 kosync_state_accept(path,9,50,now-10);assert(kosync_state_timestamp(path,9,50)==now-10);
 kosync_state_touch(path,123,9,50);assert(kosync_state_timestamp(path,9,50)==now-10);
 now++;kosync_state_touch(path,123,9,40);assert(kosync_state_timestamp(path,9,40)==now);
 assert(!mkdir(tmp,0700));kosync_state_commit(path);assert(!rmdir(tmp));
 kosync_state_begin(path,123,10,120);assert(kosync_state_timestamp(path,10,120)==now-31);
 kosync_state_touch(path,123,9,40);kosync_state_commit(path);
 kosync_state_begin(path,123,9,40);assert(kosync_state_timestamp(path,9,40)==now);
 kosync_state_begin(path,999,9,40);assert(!kosync_state_timestamp(path,9,40));
 FILE*f=fopen(file,"r+b");assert(f);assert(!fseek(f,20,SEEK_SET));int c=fgetc(f);assert(!fseek(f,20,SEEK_SET));assert(fputc(c^1,f)!=EOF);fclose(f);
 kosync_state_begin(path,123,9,40);assert(!kosync_state_timestamp(path,9,40));
 fat_mode=true;now++;kosync_state_touch(path,123,9,42);kosync_state_commit(path);kosync_state_begin(path,123,9,42);assert(kosync_state_timestamp(path,9,42)==now);
 now=100;kosync_state_touch(path,123,9,41);assert(!kosync_state_timestamp(path,9,41));
 puts("Reading event state: RAM-only turns, SD checkpoint, dedup, reopen, backward/forward events, invalid clock, write failure, CRC corruption and FAT replacement recovery PASS");
}
