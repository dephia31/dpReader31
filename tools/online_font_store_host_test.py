#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""Run real file/hash/deflate/SFNT installation with failure preservation under ASan/UBSan."""
import argparse, hashlib, json, os, shutil, subprocess, tempfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--fonts-dir',type=Path)
args=p.parse_args()
(ROOT/'build').mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(dir=ROOT/'build',prefix='font-store-') as tmp:
    w=Path(tmp);(w/'psa').mkdir()
    (w/'esp_err.h').write_text('''#pragma once
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_INVALID_SIZE 0x104
#define ESP_ERR_INVALID_RESPONSE 0x108
''')
    (w/'psa/crypto.h').write_text('''#pragma once
#include <CommonCrypto/CommonDigest.h>
typedef CC_SHA256_CTX psa_hash_operation_t;
#define PSA_SUCCESS 0
#define PSA_ALG_SHA_256 0x02000009
#define PSA_HASH_OPERATION_INIT {0}
static inline int psa_crypto_init(void){return 0;}
static inline int psa_hash_abort(psa_hash_operation_t *c){(void)c;return 0;}
static inline int psa_hash_setup(psa_hash_operation_t *c,int b){return b!=PSA_ALG_SHA_256||CC_SHA256_Init(c)!=1;}
static inline int psa_hash_update(psa_hash_operation_t *c,const unsigned char *p,size_t n){return CC_SHA256_Update(c,p,(CC_LONG)n)!=1;}
static inline int psa_hash_finish(psa_hash_operation_t *c,unsigned char *p,size_t cap,size_t *n){if(cap<32)return -1;*n=32;return CC_SHA256_Final(p,c)!=1;}
''')
    (w/'test.c').write_text(r'''
#include "online_font_store.h"
#include "online_font_catalog.inc"
#include "psa/crypto.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static unsigned calls;
static bool stop(void *p){return ++calls>*(unsigned *)p;}
static void hash_file(const char *path,uint8_t digest[32],uint32_t *size){
 FILE *f=fopen(path,"rb");assert(f);uint8_t b[8192];size_t n;*size=0;
 psa_hash_operation_t h=PSA_HASH_OPERATION_INIT;assert(psa_crypto_init()==PSA_SUCCESS);assert(psa_hash_setup(&h,PSA_ALG_SHA_256)==PSA_SUCCESS);
 while((n=fread(b,1,sizeof(b),f))){*size+=(uint32_t)n;assert(psa_hash_update(&h,b,n)==PSA_SUCCESS);}
 assert(!ferror(f));assert(!fclose(f));size_t length;assert(psa_hash_finish(&h,digest,32,&length)==PSA_SUCCESS);assert(length==32);psa_hash_abort(&h);
}
int main(int argc,char **argv){
 assert(argc==5);const char *mode=argv[1],*source=argv[2],*dir=argv[3];int index=atoi(argv[4]);
 online_font_item_t item=s_fonts[index];bool catalog=!strcmp(mode,"catalog")||!strcmp(mode,"zipcancel");
 if(!catalog){item.filename="kiiko-test.ttf";item.license="Copyright test\nSIL OPEN FONT LICENSE Version 1.1\n";
  item.deflate_offset=item.deflate_size=0;hash_file(source,item.sha256,&item.size);item.source_size=item.size;memcpy(item.source_sha256,item.sha256,32);}
 char target[512],notice[540],temp[512];snprintf(target,sizeof(target),"%s/%s",dir,item.filename);
 snprintf(notice,sizeof(notice),"%s.OFL.txt",target);
 unsigned budget=~0u;online_font_cancel_t cancel=NULL;
 if(!strcmp(mode,"wronghash"))item.source_sha256[0]^=1;
 if(!strcmp(mode,"cancel")){budget=0;cancel=stop;}
 if(!strcmp(mode,"zipcancel")){budget=800;cancel=stop;}
 if(!strcmp(mode,"exists")){FILE *f=fopen(target,"wb");assert(f);assert(fwrite("keep",1,4,f)==4);fclose(f);}
 if(!strcmp(mode,"licensefail")){snprintf(temp,sizeof(temp),"%s/.kiiko-font.notice",dir);assert(!mkdir(temp,0700));}
 esp_err_t err=online_font_store_commit(&item,dir,source,cancel,&budget);
 if(!strcmp(mode,"catalog")||!strcmp(mode,"valid")){
  assert(err==ESP_OK);assert(online_font_store_installed(&item,dir));assert(online_font_store_validate(target,item.size)==ESP_OK);
 }else{
  assert(err!=ESP_OK);assert(!online_font_store_installed(&item,dir));
  if(!strcmp(mode,"exists")){char s[5]={0};FILE *f=fopen(target,"rb");assert(f);assert(fread(s,1,4,f)==4);fclose(f);assert(!strcmp(s,"keep"));}
  else assert(access(target,F_OK));
  assert(access(notice,F_OK));
 }
 snprintf(temp,sizeof(temp),"%s/.kiiko-font.unpack",dir);assert(access(temp,F_OK));
 puts("PASS");return 0;
}
''')
    command=[os.environ.get('CC','cc'),'-std=c11','-g','-O1','-fsanitize=address,undefined',
             '-fno-sanitize-recover=all','-Wno-deprecated-declarations','-I'+str(w),'-I'+str(ROOT/'main/font'),
             str(w/'test.c'),str(ROOT/'main/font/online_font_store.c'),'-lz','-o',str(w/'test')]
    subprocess.run(command,check=True)
    for mode in ('valid','wronghash','cancel','exists','licensefail','malformed'):
        d=w/mode;d.mkdir();source=d/'source.part'
        if mode=='malformed':source.write_bytes(bytes(2048))
        else:shutil.copyfile(ROOT/'main/assets/builtin.ttf',source)
        subprocess.run([str(w/'test'),mode,str(source),str(d),'0'],check=True,stdout=subprocess.DEVNULL)
    if args.fonts_dir:
        catalog=json.loads((ROOT/'main/font/online_fonts.json').read_text())
        for i,item in enumerate(catalog):
            d=w/item['id'];d.mkdir();source=d/'source.part';shutil.copyfile(args.fonts_dir/(item['id']+'.source'),source)
            subprocess.run([str(w/'test'),'catalog',str(source),str(d),str(i)],check=True,stdout=subprocess.DEVNULL)
            assert hashlib.sha256((d/item['filename']).read_bytes()).hexdigest()==item['sha256']
            assert (d/(item['filename']+'.OFL.txt')).read_bytes()==item['license'].encode()
        d=w/'zipcancel';d.mkdir();source=d/'source.part';shutil.copyfile(args.fonts_dir/'zhuque-fangsong.source',source)
        subprocess.run([str(w/'test'),'zipcancel',str(source),str(d),'3'],check=True,stdout=subprocess.DEVNULL)
    print('PASS: real SFNT install/hash/notice, corruption, invalid format, cancellation, existing-file preservation, license-write failure'+
          ('; all five pinned author files and bounded ZIP cancellation' if args.fonts_dir else '')+' (ASan/UBSan)')
