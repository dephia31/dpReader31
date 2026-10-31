/* SPDX-License-Identifier: Apache-2.0 */
#include "book_dictionary.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#ifdef ESP_PLATFORM
#include "esp_timer.h"
#endif

void book_dictionary_close(book_dictionary_t *d) {
    book_dict_file_close(d->index);
    book_dict_file_close(d->data);
    if(d->syn) fclose(d->syn);
    d->index=d->data=NULL;d->syn=NULL;
    d->state=0;
}
static int fail(book_dictionary_t *d,int state) {
    book_dictionary_close(d); d->state=state; return state;
}
static bool number(const char *s,uint32_t *n) {
    if(!*s) return false;
    uint64_t v=0;
    for(;*s;s++) { if(*s<'0'||*s>'9') return false; v=v*10+(unsigned)(*s-'0'); if(v>UINT32_MAX) return false; }
    *n=(uint32_t)v; return true;
}
static FILE *part(const char *base,const char *ext) {
    char path[BOOK_DICT_PATH];
    if(snprintf(path,sizeof(path),"%s%s",base,ext)>=(int)sizeof(path)) return NULL;
    return fopen(path,"rb");
}
static book_dict_file_t *file_part(const char *base,const char *plain,const char *packed,bool random) {
    char path[BOOK_DICT_PATH];
    if(snprintf(path,sizeof(path),"%s%s",base,plain)>=(int)sizeof(path))return NULL;
    FILE *probe=fopen(path,"rb");
    if(probe){fclose(probe);return book_dict_file_open(path,false,random);}
    if(errno!=ENOENT||snprintf(path,sizeof(path),"%s%s",base,packed)>=(int)sizeof(path))return NULL;
    return book_dict_file_open(path,true,random);
}
bool book_dictionary_open(book_dictionary_t *d,const char *ifo,const char *query) {
    book_dictionary_close(d);
    memset(d,0,sizeof(*d));
    size_t n=strlen(ifo),q=strlen(query);
    if(n<5||n>=sizeof(d->base)-8||strcmp(ifo+n-4,".ifo")||!q||q>=sizeof(d->query)) return fail(d,-1),false;
    memcpy(d->base,ifo,n-4); memcpy(d->query,query,q+1);
    FILE *f=fopen(ifo,"rb"); if(!f) return fail(d,-1),false;
    char line[512]; unsigned lines=0; bool header=false,version=false,types=false,words=false,idx=false;
    uint32_t index_size=0;
    while(fgets(line,sizeof(line),f)) {
        if(++lines>128) { fclose(f); return fail(d,-1),false; }
        if(!strchr(line,'\n')&&!feof(f)) {
            if(strncmp(line,"description=",12)){fclose(f);return fail(d,-1),false;}
            int c;unsigned skipped=0;
            while((c=fgetc(f))!=EOF&&c!='\n')if(++skipped>65536){fclose(f);return fail(d,-1),false;}
            continue;
        }
        line[strcspn(line,"\r\n")]=0;
        if(lines==1) { header=!strcmp(line,"StarDict's dict ifo file"); continue; }
        char *value=strchr(line,'='); if(!value) continue; *value++=0;
        if(!strcmp(line,"version")) version=!strcmp(value,"2.4.2")||!strcmp(value,"3.0.0");
        else if(!strcmp(line,"wordcount")) words=number(value,&d->words)&&d->words>0;
        else if(!strcmp(line,"idxfilesize")) idx=number(value,&index_size);
        else if(!strcmp(line,"idxoffsetbits")&&strcmp(value,"32")) { fclose(f); return fail(d,-1),false; }
        else if(!strcmp(line,"sametypesequence")) { types=*value&&strlen(value)<sizeof(d->types);
            for(const char *v=value;*v;v++)if(!strchr("mhgxt",*v))types=false;
            if(types)strcpy(d->types,value); }
        else if(!strcmp(line,"bookname")) snprintf(d->name,sizeof(d->name),"%.127s",value);
    }
    bool ok=!ferror(f); fclose(f);
    if(!header||!version||!types||!words||!idx||!ok) return fail(d,-1),false;
    d->index=file_part(d->base,".idx",".idx.gz",false);
    d->data=file_part(d->base,".dict",".dict.dz",true);
    if(!d->index||!d->data||book_dict_file_size(d->index)!=index_size) return fail(d,-1),false;
    d->data_size=book_dict_file_size(d->data);
    d->syn=part(d->base,".syn"); d->state=1; return true;
}
static bool be32(FILE *f,uint32_t *v) {
    unsigned char b[4]; if(fread(b,1,4,f)!=4) return false;
    *v=((uint32_t)b[0]<<24)|((uint32_t)b[1]<<16)|((uint32_t)b[2]<<8)|b[3]; return true;
}
/* Long keys are skipped without allocating from untrusted sizes. */
static int key(book_dict_file_t *f,char *out,size_t cap) {
    size_t i=0,total=0; int c; bool overflow=false;
    while((c=book_dict_file_getc(f))!=EOF&&c) { if(++total>4096)return -1; if(i+1<cap) out[i++]=(char)c; else overflow=true; }
    out[i]=0; return c==EOF?-1:overflow?0:1;
}
static bool index32(book_dict_file_t *f,uint32_t *v) {
    unsigned char b[4];if(!book_dict_file_read(f,b,4))return false;
    *v=(uint32_t)b[0]<<24|(uint32_t)b[1]<<16|(uint32_t)b[2]<<8|b[3];return true;
}
static int synonym_key(FILE *f,char *out,size_t cap) {
    size_t i=0,total=0;int c;bool overflow=false;
    while((c=fgetc(f))!=EOF&&c){if(++total>4096)return -1;if(i+1<cap)out[i++]=(char)c;else overflow=true;}
    out[i]=0;return c==EOF?-1:overflow?0:1;
}
static bool equal(const char *a,const char *b) {
    /* ASCII case variants only; non-ASCII bytes and Vietnamese accents remain exact. */
    while(*a&&*b) { unsigned char x=*a++,y=*b++; if(x>='A'&&x<='Z')x+=32; if(y>='A'&&y<='Z')y+=32; if(x!=y)return false; }
    return !*a&&!*b;
}
static int definition(book_dictionary_t *d,uint32_t off,uint32_t len) {
    if(off>d->data_size||len>d->data_size-off)return fail(d,-1);
    size_t n=len<sizeof(d->text)-1?len:sizeof(d->text)-1;
    if(!book_dict_file_begin(d->data,off,d->text,n))return fail(d,-1);
    d->definition_length=len;d->definition_bytes=n;d->state=5;return 5;
}
static int definition_finish(book_dictionary_t *d) {
    int result=book_dict_file_step(d->data);
    if(result<0)return fail(d,-1);
    if(!result)return 5;
    size_t n=d->definition_bytes;uint32_t len=d->definition_length;
    d->truncated=n<len;
    /* Cut only at a UTF-8 boundary when limiting a long definition. */
    if(d->truncated) { while(n&&((unsigned char)d->text[n-1]&0xc0)==0x80) --n; if(n&&(unsigned char)d->text[n-1]>=0xc0)--n; }
    d->text[n]=0;
    size_t source=0,out=0,fields=strlen(d->types);
    for(size_t field=0;field<fields&&source<n;field++) {
        size_t stop=n;
        if(field+1<fields) {
            char *zero=memchr(d->text+source,0,n-source);
            if(zero)stop=(size_t)(zero-d->text);
            else if(!d->truncated)return fail(d,-1);
        }
        bool markup=strchr("hgx",d->types[field])!=NULL,tag=false;
        for(size_t i=source;i<stop;i++) {
            unsigned char c=(unsigned char)d->text[i];
            if(markup&&c=='<'){tag=true;if(out&&d->text[out-1]!=' ')d->text[out++]=' ';}
            else if(markup&&c=='>')tag=false;
            else if(!tag) {
                if(markup&&c=='&') {
                    const char *entities[]={"&amp;","&lt;","&gt;","&quot;","&apos;","&nbsp;"};
                    const char values[]="&<>\"' ";bool entity=false;
                    for(unsigned e=0;e<6;e++){size_t len=strlen(entities[e]);if(len<=stop-i&&!memcmp(d->text+i,entities[e],len)){d->text[out++]=values[e];i+=len-1;entity=true;break;}}
                    if(entity)continue;
                }
                d->text[out++]=(c<32&&c!='\n'&&c!='\t')?' ':c;
            }
        }
        source=stop<n?stop+1:n;
        if(field+1<fields&&source<n)d->text[out++]='\n';
    }
    d->text[out]=0;
    /* Reject malformed UTF-8 before text reaches glyph/UI decoders. */
    for(size_t i=0;i<out;) {
        unsigned char c=(unsigned char)d->text[i];unsigned count=0;uint32_t cp=0,min=0;
        if(c<128){++i;continue;}
        if(c>=0xc2&&c<=0xdf){count=1;cp=c&31;min=0x80;}
        else if(c>=0xe0&&c<=0xef){count=2;cp=c&15;min=0x800;}
        else if(c>=0xf0&&c<=0xf4){count=3;cp=c&7;min=0x10000;}
        else return fail(d,-1);
        if(i+count>=out)return fail(d,-1);
        for(unsigned j=1;j<=count;j++){unsigned char b=(unsigned char)d->text[i+j];if((b&0xc0)!=0x80)return fail(d,-1);cp=(cp<<6)|(b&63);}
        if(cp<min||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff))return fail(d,-1);
        i+=count+1;
    }
    book_dictionary_close(d); d->state=4; return 4;
}
int book_dictionary_step(book_dictionary_t *d) {
    /* Spend useful CPU time instead of waiting a UI tick after 128 entries.
     * Bound work by both records and elapsed time so input can still cancel. */
#ifdef ESP_PLATFORM
    const int64_t began=esp_timer_get_time();
#endif
    for(unsigned budget=0;budget<4096;budget++) {
#ifdef ESP_PLATFORM
        if(budget&&!(budget&15)&&esp_timer_get_time()-began>=8000)break;
#endif
        if(d->state==1||d->state==3) {
            if(d->scanned>=d->words) {
                if(!book_dict_file_end(d->index))return fail(d,-1);
                if(d->state==1&&d->syn){d->state=2;continue;}
                return fail(d,-2);
            }
            char word[256]; uint32_t off,len;
            int k=key(d->index,word,sizeof(word));
            if(k<0||!index32(d->index,&off)||!index32(d->index,&len)||off>d->data_size||len>d->data_size-off) return fail(d,-1);
            uint32_t at=d->scanned++;
            if((d->state==3&&at==d->target)||(d->state==1&&k&&equal(word,d->query))) return definition(d,off,len);
        } else if(d->state==2) {
            if(feof(d->syn)) return fail(d,-2);
            int first=fgetc(d->syn); if(first==EOF)return fail(d,ferror(d->syn)?-1:-2); ungetc(first,d->syn);
            char word[256]; uint32_t target; int k=synonym_key(d->syn,word,sizeof(word));
            if(k<0||!be32(d->syn,&target)||target>=d->words) return fail(d,-1);
            if(k&&equal(word,d->query)) {
                d->target=target;d->scanned=0;d->state=3;
                if(!book_dict_file_rewind(d->index))return fail(d,-1);
            }
        } else if(d->state==5)return definition_finish(d);
        else return d->state;
    }
    return d->state;
}
