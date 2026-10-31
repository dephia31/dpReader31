/* SPDX-License-Identifier: Apache-2.0 */
#include "book_annotations.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "esp_log.h"
#endif
static uint64_t hash(const void *data,size_t n) {
    const unsigned char *p=data;uint64_t h=UINT64_C(14695981039346656037);
    while(n--){h^=*p++;h*=UINT64_C(1099511628211);}return h;
}
static bool filename(char *out,size_t cap,const char *root,const book_annotations_t *a,unsigned slot) {
    return snprintf(out,cap,"%s/%016llx-%u-%016llx.%u.ann",root,
        (unsigned long long)a->path_hash,(unsigned)a->chapter,(unsigned long long)a->text_hash,slot)<(int)cap;
}
static bool boundary(const char *text,size_t length,size_t at) {
    return at<=length&&(at==length||((unsigned char)text[at]&0xc0)!=0x80);
}
static bool load_slot(book_annotations_t *a,const char *file,const char *text,size_t length) {
    FILE *f=fopen(file,"rb");if(!f)return false;
    /* Local schema uses explicit LE integers; no compiler struct padding on disk. */
    unsigned char header[20];bool ok=fread(header,1,20,f)==20&&!memcmp(header,"KANN\2\0\0\0",8);
    uint32_t count=0;
    if(ok){for(int i=0;i<4;i++)count|=(uint32_t)header[8+i]<<(8*i);ok=count<=BOOK_ANNOTATION_MAX;}
    uint32_t checksum=0;if(ok)for(int i=0;i<4;i++){checksum|=(uint32_t)header[12+i]<<(8*i);a->sequence|=(uint32_t)header[16+i]<<(8*i);}
    for(uint32_t i=0;ok&&i<count;i++) {
        unsigned char offsets[8];book_annotation_t *item=&a->items[i];
        ok=fread(offsets,1,8,f)==8&&fread(item->note,1,BOOK_NOTE_CAP,f)==BOOK_NOTE_CAP&&fread(item->quote,1,BOOK_QUOTE_CAP,f)==BOOK_QUOTE_CAP;
        if(!ok)break;
        for(int j=0;j<4;j++){item->start|=(uint32_t)offsets[j]<<(8*j);item->end|=(uint32_t)offsets[j+4]<<(8*j);}
        ok=item->start<item->end&&boundary(text,length,item->start)&&boundary(text,length,item->end)&&
            memchr(item->note,0,sizeof(item->note))&&memchr(item->quote,0,sizeof(item->quote));
        if(ok){size_t q=strlen(item->quote);ok=q&&q<=item->end-item->start&&!memcmp(text+item->start,item->quote,q);}
    }
    if(ok) ok=fgetc(f)==EOF&&!ferror(f)&&(uint32_t)hash(a->items,count*sizeof(a->items[0]))==checksum;
    fclose(f);if(ok)a->count=count;else memset(a->items,0,sizeof(a->items));return ok;
}
bool book_annotations_load(book_annotations_t *a,const char *root,const char *path,uint32_t chapter,const char *text,size_t length) {
    memset(a,0,sizeof(*a));if(length>UINT32_MAX)return false;
    a->text_hash=hash(text,length);a->path_hash=hash(path,strlen(path));a->chapter=chapter;a->length=(uint32_t)length;
    #ifdef ESP_PLATFORM
    book_annotations_t *candidate=heap_caps_calloc(1,sizeof(*candidate),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
#else
    book_annotations_t *candidate=calloc(1,sizeof(*candidate));
#endif
    if(!candidate)return false;
    bool valid=false,exists=false;
    for(unsigned slot=0;slot<2;slot++) {
        char file[640];if(!filename(file,sizeof(file),root,a,slot)){free(candidate);return false;}
        struct stat st;
        if(stat(file,&st)) {if(errno!=ENOENT)exists=true;continue;}
        exists=true;memset(candidate,0,sizeof(*candidate));
        if(load_slot(candidate,file,text,length)&&(!valid||candidate->sequence>a->sequence)) {
            memcpy(a->items,candidate->items,sizeof(a->items));a->count=candidate->count;a->sequence=candidate->sequence;valid=true;
        }
    }
    free(candidate);return valid||!exists;
}
static bool save_error(const char *stage,const char *path,int error) {
    if(!error)error=EIO;
#ifdef ESP_PLATFORM
    ESP_LOGE("book_annotations","save failed stage=%s errno=%d path=%s",stage,error,path?path:"");
#else
    (void)stage;(void)path;
#endif
    errno=error;return false;
}
static bool make_save_directory(const char *root) {
    char path[640];
    if(!root||!*root)return save_error("directory",root,EINVAL);
    if(snprintf(path,sizeof(path),"%s",root)>=(int)sizeof(path))return save_error("directory",root,ENAMETOOLONG);
    for(char *part=path+1;;++part){
        if(*part&&*part!='/')continue;
        char end=*part;*part=0;
        struct stat st;
        if(stat(path,&st)) {
            if(errno!=ENOENT)return save_error("stat-directory",path,errno);
            if(mkdir(path,0775)&&errno!=EEXIST)return save_error("mkdir",path,errno);
            if(stat(path,&st))return save_error("stat-created-directory",path,errno);
        }
        if(!S_ISDIR(st.st_mode))return save_error("not-directory",path,ENOTDIR);
        *part=end;if(!end)break;
    }
    return true;
}
bool book_annotations_save(book_annotations_t *a,const char *root) {
    if(a->count>BOOK_ANNOTATION_MAX||a->sequence==UINT32_MAX)return save_error("validate",root,EINVAL);
    if(!make_save_directory(root))return false;
    char file[640],tmp[648];
    if(!filename(file,sizeof(file),root,a,(a->sequence+1)&1)||
       snprintf(tmp,sizeof(tmp),"%s.tmp",file)>=(int)sizeof(tmp))return save_error("filename",root,ENAMETOOLONG);
    errno=0;FILE *f=fopen(tmp,"wb");if(!f)return save_error("open",tmp,errno);
    unsigned char header[20]={'K','A','N','N',2,0,0,0};uint32_t sum=(uint32_t)hash(a->items,a->count*sizeof(a->items[0]));
    for(int j=0;j<4;j++){header[8+j]=(unsigned char)(a->count>>(8*j));header[12+j]=(unsigned char)(sum>>(8*j));header[16+j]=(unsigned char)((a->sequence+1)>>(8*j));}
    int error=0;const char *stage="write";
    if(fwrite(header,1,20,f)!=20)error=errno?errno:EIO;
    for(uint32_t i=0;!error&&i<a->count;i++) {
        const book_annotation_t *item=&a->items[i];unsigned char offsets[8];
        for(int j=0;j<4;j++){offsets[j]=(unsigned char)(item->start>>(8*j));offsets[j+4]=(unsigned char)(item->end>>(8*j));}
        if(fwrite(offsets,1,8,f)!=8||fwrite(item->note,1,BOOK_NOTE_CAP,f)!=BOOK_NOTE_CAP||fwrite(item->quote,1,BOOK_QUOTE_CAP,f)!=BOOK_QUOTE_CAP)error=errno?errno:EIO;
    }
    if(!error&&fflush(f)){stage="flush";error=errno?errno:EIO;}
    if(!error&&fsync(fileno(f))){stage="fsync";error=errno?errno:EIO;}
    if(fclose(f)&&!error){stage="close";error=errno?errno:EIO;}
    /* Keep the latest slot intact until the other slot's replacement is durable. */
    if(!error&&unlink(file)&&errno!=ENOENT){stage="unlink-old-slot";error=errno?errno:EIO;}
    if(!error&&rename(tmp,file)){stage="rename";error=errno?errno:EIO;}
    if(error){(void)unlink(tmp);return save_error(stage,file,error);}
    ++a->sequence;
#ifdef ESP_PLATFORM
    ESP_LOGI("book_annotations","saved count=%lu sequence=%lu path=%s",(unsigned long)a->count,(unsigned long)a->sequence,file);
#endif
    return true;
}
bool book_annotations_put(book_annotations_t *a,const char *text,size_t start,size_t end,const char *note) {
    if(start>=end||!boundary(text,a->length,start)||!boundary(text,a->length,end)||strlen(note)>=BOOK_NOTE_CAP)return false;
    uint32_t i=0;for(;i<a->count;i++)if(a->items[i].start==start&&a->items[i].end==end)break;
    if(i==a->count&&i==BOOK_ANNOTATION_MAX)return false;
    book_annotation_t *item=&a->items[i];memset(item,0,sizeof(*item));item->start=(uint32_t)start;item->end=(uint32_t)end;
    size_t q=end-start;if(q>=BOOK_QUOTE_CAP){q=BOOK_QUOTE_CAP-1;while(q&&((unsigned char)text[start+q]&0xc0)==0x80)--q;}
    memcpy(item->quote,text+start,q);strcpy(item->note,note);
    if(i==a->count)++a->count;
    return true;
}
