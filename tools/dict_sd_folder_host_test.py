#!/usr/bin/env python3
"""Run the real S31 media-folder setup against a temporary card tree.
SPDX-License-Identifier: Apache-2.0
"""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
ROOT=Path(__file__).resolve().parents[1]
source=ROOT/'boards/metalio47/src/components/read_pico/read_pico_sd.c'
unit=r'''
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static const char *root;
static int mapped_mkdir(const char *path,int mode){char target[1024];assert(!strncmp(path,"/sdcard/",8));snprintf(target,sizeof(target),"%s/%s",root,path+8);return mkdir(target,mode);}
#define mkdir mapped_mkdir
#define ESP_LOGW(...) ((void)0)
'''+function('ensure_media_dirs',source)+r'''
#undef mkdir
int main(int argc,char **argv){assert(argc==2);root=argv[1];char path[1024];struct stat st;
 snprintf(path,sizeof(path),"%s/dict",root);FILE *f=fopen(path,"wb");assert(f);assert(fputs("keep",f)>=0);fclose(f);
 ensure_media_dirs();assert(!stat(path,&st)&&S_ISREG(st.st_mode));assert(!unlink(path));
 ensure_media_dirs();assert(!stat(path,&st)&&S_ISDIR(st.st_mode));
 snprintf(path,sizeof(path),"%s/dict/user.dict.dz",root);f=fopen(path,"wb");assert(f);assert(fputs("user dictionary",f)>=0);fclose(f);
 ensure_media_dirs();f=fopen(path,"rb");assert(f);char data[32]={0};assert(fread(data,1,31,f)==15);fclose(f);assert(!strcmp(data,"user dictionary"));
 puts("PASS: real SD setup creates dict idempotently and preserves existing folder contents / conflicting user file");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);c=p/'test.c';exe=p/'test';card=p/'card';card.mkdir();c.write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(c),'-o',str(exe)],check=True)
 subprocess.run([str(exe),str(card)],check=True)
