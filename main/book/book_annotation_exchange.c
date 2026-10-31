/* SPDX-License-Identifier: Apache-2.0 */
#include "book_annotation_exchange.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif
#define EXCHANGE_LIMIT (128u*1024u)
#define EXCHANGE_ITEMS 128
static bool mutation_failed;
static void *buffer_alloc(size_t n) {
#ifdef ESP_PLATFORM
    return heap_caps_malloc(n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
#else
    return malloc(n);
#endif
}
static const char *str(const cJSON *o,const char *key) {
    const cJSON *v=cJSON_GetObjectItemCaseSensitive(o,key);
    return cJSON_IsString(v)?v->valuestring:"";
}
static bool integer(const cJSON *o,const char *key,uint32_t *n) {
    const cJSON *v=cJSON_GetObjectItemCaseSensitive(o,key);
    if(!cJSON_IsNumber(v)||v->valuedouble<0||v->valuedouble>UINT32_MAX||v->valuedouble!=(uint32_t)v->valuedouble)return false;
    *n=(uint32_t)v->valuedouble;return true;
}
static void replace(cJSON *o,const char *key,cJSON *value) {
    if(!o||!value){cJSON_Delete(value);mutation_failed=true;return;}
    if(cJSON_HasObjectItem(o,key)) {
        if(!cJSON_ReplaceItemInObjectCaseSensitive(o,key,value)){cJSON_Delete(value);mutation_failed=true;}
    } else if(!cJSON_AddItemToObject(o,key,value)){cJSON_Delete(value);mutation_failed=true;}
}
static void string(cJSON *o,const char *key,const char *v) {replace(o,key,cJSON_CreateString(v));}
static void number(cJSON *o,const char *key,double v) {replace(o,key,cJSON_CreateNumber(v));}
static void boolean(cJSON *o,const char *key,bool v) {replace(o,key,cJSON_CreateBool(v));}
static void timestamp(cJSON *entry,const char *field) {
    time_t now=time(NULL);struct tm date;char out[24];
    if(now>=1609459200&&localtime_r(&now,&date)&&strftime(out,sizeof(out),"%Y-%m-%d %H:%M:%S",&date))string(entry,field,out);
}
bool book_annotation_sidecar(const char *book,char *out,size_t cap) {
    const char *slash=strrchr(book,'/'),*dot=strrchr(book,'.');
    size_t n=dot&&(!slash||dot>slash)?(size_t)(dot-book):strlen(book);
    if(n+5>=cap){errno=ENAMETOOLONG;return false;}
    memcpy(out,book,n);memcpy(out+n,".sdr",5);return true;
}
#ifndef DPREADER_NOTES_ROOT
#ifdef ESP_PLATFORM
#define DPREADER_NOTES_ROOT "/sdcard/dpReader_note"
#else
#define DPREADER_NOTES_ROOT "dpReader_note"
#endif
#endif
/* Mirror the book's card-relative path, including its extension, so books with
 * the same basename in different folders (or TXT/EPUB) never share notes. */
bool book_annotation_directory(const char *book,char *out,size_t cap) {
    const char *relative=book;
    if(!strncmp(relative,"/sdcard/",8))relative+=8;
    else while(*relative=='/')++relative;
    if(!*relative){errno=EINVAL;return false;}
    for(const char *part=relative;*part;) {
        const char *end=strchr(part,'/');size_t n=end?(size_t)(end-part):strlen(part);
        if(!n||(n==1&&part[0]=='.')||(n==2&&!memcmp(part,"..",2))){errno=EINVAL;return false;}
        part=end?end+1:part+n;
        if(end&&!*part){errno=EINVAL;return false;}
    }
    if(snprintf(out,cap,"%s/%s",DPREADER_NOTES_ROOT,relative)>=(int)cap){errno=ENAMETOOLONG;return false;}
    return true;
}
static bool make_directory(char *path) {
    for(char *p=path+1;;p++) {
        if(*p!='/'&&*p)continue;
        char c=*p;*p=0;
        struct stat st;
        if(stat(path,&st)) {
            if(errno!=ENOENT){*p=c;return false;}
            if(mkdir(path,0775)&&errno!=EEXIST){*p=c;return false;}
            if(stat(path,&st)){*p=c;return false;}
        }
        if(!S_ISDIR(st.st_mode)){*p=c;errno=ENOTDIR;return false;}
        *p=c;if(!c)break;
    }
    return true;
}
static bool paths(const char *book,char *file,size_t cap) {
    char dir[512];
    if(!book_annotation_directory(book,dir,sizeof(dir))||!make_directory(dir))return false;
    if(snprintf(file,cap,"%s/dpReader.annotations.json",dir)>=(int)cap){errno=ENAMETOOLONG;return false;}
    return true;
}
static cJSON *read_one(const char *file,bool *exists) {
    FILE *f=fopen(file,"rb");if(!f){if(errno!=ENOENT)*exists=true;return NULL;}
    *exists=true;
    if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}
    long len=ftell(f);
    if(len<=0||len>EXCHANGE_LIMIT||fseek(f,0,SEEK_SET)){fclose(f);errno=EFBIG;return NULL;}
    char *buf=buffer_alloc((size_t)len+1);if(!buf){fclose(f);errno=ENOMEM;return NULL;}
    bool ok=fread(buf,1,(size_t)len,f)==(size_t)len&&!ferror(f);fclose(f);buf[len]=0;
    unsigned depth=0;bool quoted=false,escaped=false;
    for(long i=0;ok&&i<len;i++) {
        char c=buf[i];
        if(quoted){if(escaped)escaped=false;else if(c=='\\')escaped=true;else if(c=='"')quoted=false;}
        else if(c=='"')quoted=true;
        else if(c=='{'||c=='['){if(++depth>16)ok=false;}
        else if(c=='}'||c==']'){if(!depth)ok=false;else --depth;}
    }
    errno=0;
    cJSON *doc=ok&&!depth&&!quoted?cJSON_ParseWithLengthOpts(buf,(size_t)len+1,NULL,true):NULL;free(buf);
    uint32_t version;const cJSON *items=cJSON_GetObjectItemCaseSensitive(doc,"annotations");
    if(!doc){if(errno!=ENOMEM)errno=EINVAL;return NULL;}
    if(strcmp(str(doc,"schema"),"kiikoread-koreader-annotations")||!integer(doc,"version",&version)||version!=1){cJSON_Delete(doc);errno=EPROTONOSUPPORT;return NULL;}
    if(!cJSON_IsArray(items)||cJSON_GetArraySize(items)>EXCHANGE_ITEMS){cJSON_Delete(doc);errno=EFBIG;return NULL;}
    const cJSON *entry=NULL;
    cJSON_ArrayForEach(entry,items) {
        if(!*str(entry,"id")||!*str(entry,"text")||!cJSON_IsString(cJSON_GetObjectItemCaseSensitive(entry,"note"))){cJSON_Delete(doc);errno=EPROTONOSUPPORT;return NULL;}
        const cJSON *other=NULL;cJSON_ArrayForEach(other,items)if(other!=entry&&!strcmp(str(other,"id"),str(entry,"id"))){cJSON_Delete(doc);errno=EPROTONOSUPPORT;return NULL;}
    }
    return doc;
}
static cJSON *read_doc(const char *book,char *file,size_t cap) {
    mutation_failed=false;
    if(!paths(book,file,cap))return NULL;
    bool exists=false;cJSON *doc=read_one(file,&exists);
    const char *base=strrchr(book,'/');base=base?base+1:book;
    if(doc){if(strcmp(str(doc,"book"),base)){cJSON_Delete(doc);errno=EXDEV;return NULL;}return doc;}
    if(errno==ENOMEM||errno==EFBIG||errno==EPROTONOSUPPORT)return NULL;
    char old[640];snprintf(old,sizeof(old),"%s.old",file);
    doc=read_one(old,&exists);if(doc){if(strcmp(str(doc,"book"),base)){cJSON_Delete(doc);errno=EXDEV;return NULL;}return doc;}
    if(exists){errno=EINVAL;return NULL;}
    /* Read old JSON only when neither new primary nor backup exists. Never
     * create/write/delete the old sidecar during automatic migration. */
    char legacy_dir[512],legacy_file[640];
    if(!book_annotation_sidecar(book,legacy_dir,sizeof(legacy_dir)))return NULL;
    if(snprintf(legacy_file,sizeof(legacy_file),"%s/kiikoread.annotations.json",legacy_dir)>=(int)sizeof(legacy_file)){errno=ENAMETOOLONG;return NULL;}
    doc=read_one(legacy_file,&exists);
    if(!doc&&(errno==ENOMEM||errno==EFBIG||errno==EPROTONOSUPPORT))return NULL;
    if(!doc) {
        if(snprintf(legacy_file,sizeof(legacy_file),"%s/kiikoread.annotations.json.old",legacy_dir)>=(int)sizeof(legacy_file)){errno=ENAMETOOLONG;return NULL;}
        doc=read_one(legacy_file,&exists);
    }
    if(doc){if(strcmp(str(doc,"book"),base)){cJSON_Delete(doc);errno=EXDEV;return NULL;}return doc;}
    if(exists){errno=EINVAL;return NULL;}
    doc=cJSON_CreateObject();if(!doc){errno=ENOMEM;return NULL;}
    cJSON_AddStringToObject(doc,"schema","kiikoread-koreader-annotations");cJSON_AddNumberToObject(doc,"version",1);
    const char *name=strrchr(book,'/');cJSON_AddStringToObject(doc,"book",name?name+1:book);
    cJSON_AddArrayToObject(doc,"annotations");return doc;
}
static bool write_doc(cJSON *doc,const char *file) {
    if(mutation_failed||strcmp(str(doc,"schema"),"kiikoread-koreader-annotations")||!*str(doc,"book")||!cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(doc,"annotations"))){errno=ENOMEM;return false;}
    const cJSON *item=NULL;
    cJSON_ArrayForEach(item,cJSON_GetObjectItemCaseSensitive(doc,"annotations")) {
        if(!*str(item,"id")||!*str(item,"text")||!cJSON_IsString(cJSON_GetObjectItemCaseSensitive(item,"note"))){errno=EINVAL;return false;}
    }
    char tmp[640],old[640];snprintf(tmp,sizeof(tmp),"%s.tmp",file);snprintf(old,sizeof(old),"%s.old",file);
    char *buf=buffer_alloc(EXCHANGE_LIMIT);if(!buf){errno=ENOMEM;return false;}
    bool ok=cJSON_PrintPreallocated(doc,buf,EXCHANGE_LIMIT,false);
    if(!ok){free(buf);errno=EFBIG;return false;}
    FILE *f=fopen(tmp,"wb");if(!f){free(buf);return false;}
    size_t n=strlen(buf);ok=fwrite(buf,1,n,f)==n;free(buf);
    if(fflush(f))ok=false;
    if(ok&&fsync(fileno(f)))ok=false;
    if(fclose(f))ok=false;
    /* Never unlink the last valid copy, including recovery from .old. */
    bool exists=false;cJSON *primary=read_one(file,&exists);
    if(ok&&primary) {
        if(unlink(old)&&errno!=ENOENT)ok=false;
        if(ok&&rename(file,old))ok=false;
    } else if(ok&&exists) {if(unlink(file))ok=false;}
    cJSON_Delete(primary);
    if(ok&&rename(tmp,file))ok=false;
    int error=errno;if(!ok)unlink(tmp);if(!ok)errno=error;return ok;
}
bool book_annotation_catalog(const char *book,size_t count,bool (*title)(size_t,char*,size_t)) {
    char file[600];cJSON *doc=read_doc(book,file,sizeof(file));if(!doc)return false;
    const cJSON *existing=cJSON_GetObjectItemCaseSensitive(doc,"chapters");
    if(cJSON_IsArray(existing)&&cJSON_GetArraySize(existing)==(int)count){cJSON_Delete(doc);return true;}
    if(count>2048){cJSON_Delete(doc);errno=EFBIG;return false;}
    cJSON_DeleteItemFromObjectCaseSensitive(doc,"chapters");cJSON *chapters=cJSON_AddArrayToObject(doc,"chapters");
    bool ok=chapters!=NULL;
    for(size_t i=0;ok&&i<count;i++) {
        char name[256];if(!title(i,name,sizeof(name))){ok=false;break;}
        cJSON *item=cJSON_CreateString(name);if(!item){ok=false;break;}cJSON_AddItemToArray(chapters,item);
    }
    if(ok)ok=write_doc(doc,file);
    cJSON_Delete(doc);return ok;
}
static bool target(const cJSON *entry,const book_annotations_t *a) {
    const cJSON *native=cJSON_GetObjectItemCaseSensitive(entry,"kiikoread");uint32_t chapter;
    if(!integer(native,"chapter",&chapter)||chapter!=a->chapter)return false;
    char hash[24];snprintf(hash,sizeof(hash),"%016llx",(unsigned long long)a->text_hash);
    return !*str(native,"text_hash")||!strcmp(hash,str(native,"text_hash"));
}
static bool locate(const cJSON *entry,const book_annotations_t *a,const char *text,uint32_t *start,uint32_t *end) {
    const char *quote=str(entry,"text");size_t len=strlen(quote);if(!len||len>8192)return false;
    const cJSON *native=cJSON_GetObjectItemCaseSensitive(entry,"kiikoread");
    if(integer(native,"start",start)&&integer(native,"end",end)&&*start<*end&&*end<=a->length&&*end-*start==len&&!memcmp(text+*start,quote,len))return true;
    const char *found=strstr(text,quote);if(!found||strstr(found+1,quote))return false;
    *start=(uint32_t)(found-text);*end=*start+(uint32_t)len;return true;
}
bool book_annotation_import(book_annotations_t *a,const char *book,const char *text,unsigned *unresolved) {
    char file[600];cJSON *doc=read_doc(book,file,sizeof(file));if(!doc)return false;
    *unresolved=0;const cJSON *entry=NULL;
    cJSON_ArrayForEach(entry,cJSON_GetObjectItemCaseSensitive(doc,"annotations")) {
        if(!target(entry,a))continue;
        if(strlen(str(entry,"note"))>=BOOK_NOTE_CAP||cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(entry,"conflict"))){++*unresolved;continue;}
        uint32_t sequence;
        if(integer(cJSON_GetObjectItemCaseSensitive(entry,"kiikoread"),"sequence",&sequence)&&sequence<a->sequence){++*unresolved;continue;}
        uint32_t start,end;if(!locate(entry,a,text,&start,&end)){++*unresolved;continue;}
        uint32_t index=0;for(;index<a->count;index++)if(a->items[index].start==start&&a->items[index].end==end)break;
        if(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(entry,"deleted"))) {
            if(index<a->count){memmove(&a->items[index],&a->items[index+1],(a->count-index-1)*sizeof(a->items[0]));--a->count;}
        } else if(strlen(str(entry,"note"))>=BOOK_NOTE_CAP||!book_annotations_put(a,text,start,end,str(entry,"note")))++*unresolved;
    }
    cJSON_Delete(doc);return true;
}
static void anchor(cJSON *entry,const book_annotations_t *a,const book_annotation_t *item) {
    cJSON *native=cJSON_GetObjectItemCaseSensitive(entry,"kiikoread");
    if(!cJSON_IsObject(native)){cJSON_DeleteItemFromObjectCaseSensitive(entry,"kiikoread");native=cJSON_AddObjectToObject(entry,"kiikoread");}
    char hash[24];snprintf(hash,sizeof(hash),"%016llx",(unsigned long long)a->text_hash);
    number(native,"sequence",a->sequence);number(native,"chapter",a->chapter);number(native,"start",item->start);number(native,"end",item->end);string(native,"text_hash",hash);
}
bool book_annotation_publish(const book_annotations_t *a,const char *book,const char *text) {
    char file[600];cJSON *doc=read_doc(book,file,sizeof(file));if(!doc)return false;
    cJSON *entries=cJSON_GetObjectItemCaseSensitive(doc,"annotations"),*entry=NULL;bool seen[BOOK_ANNOTATION_MAX]={false};bool ok=true;
    cJSON_ArrayForEach(entry,entries) {
        if(!target(entry,a))continue;
        uint32_t start,end;if(!locate(entry,a,text,&start,&end))continue;
        unsigned i=0;for(;i<a->count;i++)if(a->items[i].start==start&&a->items[i].end==end)break;
        if(strlen(str(entry,"note"))>=BOOK_NOTE_CAP||cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(entry,"conflict"))){if(i<a->count)seen[i]=true;continue;}
        uint32_t sequence;
        if(!strcmp(str(entry,"source"),"koreader")&&integer(cJSON_GetObjectItemCaseSensitive(entry,"kiikoread"),"sequence",&sequence)&&sequence<a->sequence) {
            bool acknowledged=cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(entry,"deleted"))?i==a->count:i<a->count&&!strcmp(a->items[i].note,str(entry,"note"));
            if(!acknowledged){ok=false;errno=EBUSY;break;}
        }
        if(i==a->count){
            const cJSON *native=cJSON_GetObjectItemCaseSensitive(entry,"kiikoread");uint32_t prior_sequence;
            if(!integer(native,"sequence",&prior_sequence)||!*str(native,"text_hash"))continue;
            number(cJSON_GetObjectItemCaseSensitive(entry,"kiikoread"),"sequence",a->sequence);string(entry,"source","kiikoread");boolean(entry,"deleted",true);timestamp(entry,"datetime_updated");continue;}
        seen[i]=true;
        if(strcmp(str(entry,"note"),a->items[i].note))timestamp(entry,"datetime_updated");
        string(entry,"source","kiikoread");anchor(entry,a,&a->items[i]);string(entry,"note",a->items[i].note);
        boolean(entry,"deleted",false);
    }
    for(unsigned i=0;ok&&i<a->count;i++)if(!seen[i]) {
        const book_annotation_t *item=&a->items[i];size_t n=item->end-item->start;
        if(n>8192||item->end>a->length||cJSON_GetArraySize(entries)>=EXCHANGE_ITEMS){ok=false;errno=EFBIG;break;}
        char *quote=buffer_alloc(n+1);if(!quote){ok=false;errno=ENOMEM;break;}memcpy(quote,text+item->start,n);quote[n]=0;
        entry=cJSON_CreateObject();if(!entry){free(quote);ok=false;errno=ENOMEM;break;}
        char id[96];snprintf(id,sizeof(id),"kiiko-%016llx-%u-%u-%u",(unsigned long long)a->text_hash,(unsigned)a->chapter,(unsigned)item->start,(unsigned)item->end);
        cJSON_AddStringToObject(entry,"source","kiikoread");cJSON_AddStringToObject(entry,"id",id);cJSON_AddStringToObject(entry,"text",quote);free(quote);
        cJSON_AddStringToObject(entry,"note",item->note);cJSON_AddStringToObject(entry,"drawer","underscore");
        boolean(entry,"deleted",false);timestamp(entry,"datetime");anchor(entry,a,item);cJSON_AddItemToArray(entries,entry);
    }
    if(ok)ok=write_doc(doc,file);
    cJSON_Delete(doc);return ok;
}
