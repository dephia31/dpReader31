#!/usr/bin/env python3
"""字体真实目录/命名边界与上传规范化。/ Real catalogue and upload-name normalization boundaries."""
from pathlib import Path
import subprocess,tempfile
from board_source_view import select_source
from home_cover_cache_host_test import function
R=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as d:
 p=Path(d);src=p/'font.c';src.write_text(select_source((R/'main/font/ttf_font.c').read_text(),'metalio47'))
 transfer=p/'transfer.c';transfer.write_text(select_source((R/'components/read_pico_transfer/read_pico_transfer.c').read_text(),'metalio47'))
 unit=r'''
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include "font_filename.h"
#define TTF_FONT_MAX 128
#define TTF_FONT_NAME_MAX 64
#define TTF_FONT_PATH_MAX 160
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define heap_caps_calloc(n,b,c) calloc(n,b)
typedef struct{char name[64],path[160];}ttf_font_item_t;
static ttf_font_item_t *s_catalog;static int s_catalog_n;
static const char *k_font_dirs[2],*user;
static const char *app_settings_fonts_dir(void){return user;}
'''
 for f in ('font_basename','is_junk_name','is_ttf_name','font_stem','font_item_cmp','catalog_has_name','catalog_alloc','ttf_font_scan','ttf_font_count','ttf_font_item'):
  unit+=function(f,src)+'\n'
 unit+=function('decode_font_name',transfer)
 unit+=r'''
static void put(const char *dir,const char *name,const char *data){char path[512];assert(snprintf(path,sizeof(path),"%s/%s",dir,name)<512);FILE*f=fopen(path,"wb");assert(f);fwrite(data,1,strlen(data),f);fclose(f);}
static void encode(const char *raw,char out[1024]){const char *hex="0123456789ABCDEF";size_t n=0;for(const unsigned char*p=(const unsigned char*)raw;*p;++p){out[n++]='%';out[n++]=hex[*p>>4];out[n++]=hex[*p&15];}out[n]=0;}
int main(int argc,char**argv){assert(argc==3);k_font_dirs[0]=argv[1];k_font_dirs[1]=NULL;user=argv[2];
 const char *names[]={"中文 字体（测试）.TTF","a+b_[]&%#@!.ttf",".hidden.ttf","._real.ttf","same.ttf","example.OTF"};
 for(unsigned i=0;i<sizeof(names)/sizeof(*names);++i)put(user,names[i],"font-content");
 put(k_font_dirs[0],"same.ttf","asset-content");put(user,"._sidecar.ttf","\x00\x05\x16\x07");
 // 用真实AppleDouble魔数，不能把同前缀真字体一起过滤。/ Filter the AppleDouble signature, not real fonts sharing its prefix.
 {char path[512];snprintf(path,sizeof(path),"%s/._sidecar.ttf",user);FILE*f=fopen(path,"wb");assert(f);const unsigned char m[]={0,5,22,7};fwrite(m,1,4,f);fclose(f);}
 put(user,"not.ttf.zip","ignored");put(user,"not.woff","ignored");put(user,"a.ttf.txt","ignored");
 char a[240],b[240];memset(a,'a',210);strcpy(a+210,"-first.ttf");memcpy(b,a,210);strcpy(b+210,"-second.ttf");
 put(user,a,"long-first-font-bytes");put(user,b,"long-second-font-bytes");
 char collision[160];assert(font_filename_make(a,collision,160-strlen(user)-1-5,true));put(user,collision,"existing-file-never-overwritten");
 assert(ttf_font_scan()==10&&ttf_font_count()==10);
 bool first=false,second=false,otf=false,hidden=false,untouched=false;
 for(int i=0;i<10;++i){const ttf_font_item_t*it=ttf_font_item(i);assert(it&&strlen(it->path)<160);assert(font_filename_supported(it->path));
  for(int j=0;j<i;++j){assert(strcasecmp(it->path,ttf_font_item(j)->path));assert(strcasecmp(it->name,ttf_font_item(j)->name));}
  for(size_t n=0;n<strlen(it->name);){size_t k=font_filename_codepoint((const unsigned char*)it->name+n,strlen(it->name)-n);assert(k);n+=k;}
  FILE*f=fopen(it->path,"rb");assert(f);char bytes[64]={0};fread(bytes,1,sizeof(bytes)-1,f);fclose(f);
  if(!strcmp(bytes,"long-first-font-bytes"))first=true;if(!strcmp(bytes,"long-second-font-bytes"))second=true;
  if(!strcmp(bytes,"existing-file-never-overwritten"))untouched=true;
  if(strstr(it->path,".OTF"))otf=true;if(strstr(it->path,"/.hidden.ttf"))hidden=true;
 }
 assert(first&&second&&otf&&hidden&&untouched);assert(ttf_font_scan()==10);
 const char *uploads[]={"a:b?c*.ttf","特殊符号📚名称.ttf","../font.otf","font .ttf",".ttf","font.ttf.txt","font.woff","font.ttf ","a%z.ttf","无后缀","a/b\\c.otf"};
 char encoded[1024],safe[121];
 for(unsigned i=0;i<sizeof(uploads)/sizeof(*uploads);++i){encode(uploads[i],encoded);bool expected=font_filename_supported(uploads[i]);
  assert(decode_font_name(encoded,safe,sizeof(safe))==expected);
  if(expected){assert(safe[0]!='.'&&safe[0]!=' '&&strlen(safe)<121&&font_filename_supported(safe));assert(!strpbrk(safe,"/\\:*?\"<>|")&&!strstr(safe,".."));}
 }
 assert(!decode_font_name("bad%00.ttf",safe,sizeof(safe))&&!decode_font_name("bad%XY.ttf",safe,sizeof(safe)));
 const char *bad="bad\xff-name.ttf";encode(bad,encoded);assert(decode_font_name(encoded,safe,sizeof(safe)));assert(!strchr(safe,(char)0xff));
 char utf[256]={0};for(int i=0;i<70;++i)strcat(utf,"字");strcat(utf,".ttf");encode(utf,encoded);
 for(size_t cap=20;cap<=121;++cap){assert(decode_font_name(encoded,safe,cap)&&strlen(safe)<cap);for(size_t i=0;i<strlen(safe);){size_t k=font_filename_codepoint((unsigned char*)safe+i,strlen(safe)-i);assert(k);i+=k;}}
 free(s_catalog);puts("PASS: real .ttf/.otf catalogue; hidden/AppleDouble distinction, same-label paths, 220-byte name normalization with exact font bytes, stable128 bound; upload invalid symbols/UTF8/encoded traversal bounded safely and unsupported extensions ignored");
}
'''
 (p/'test.c').write_text(unit);(p/'assets').mkdir();(p/'fonts').mkdir()
 subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(R/'main/font'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test'),str(p/'assets'),str(p/'fonts')],check=True)
