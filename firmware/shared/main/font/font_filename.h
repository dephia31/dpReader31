// 字体文件名只影响标签和安全存储名，不作为字库身份。/ Font names supply labels and safe storage names, never face identity.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

static inline bool font_filename_supported(const char *name) {
    size_t n=name?strlen(name):0;
    return n>4 && (!strcasecmp(name+n-4,".ttf") || !strcasecmp(name+n-4,".otf"));
}
static inline uint32_t font_filename_hash(const char *name) {
    uint32_t hash=2166136261u;
    for(const unsigned char *p=(const unsigned char *)name;*p;++p)hash=(hash^*p)*16777619u;
    return hash;
}
static inline size_t font_filename_codepoint(const unsigned char *p,size_t n) {
    if(!n)return 0;
    if(p[0]<128)return 1;
    unsigned k=p[0]>=0xc2&&p[0]<=0xdf?2:p[0]>=0xe0&&p[0]<=0xef?3:p[0]>=0xf0&&p[0]<=0xf4?4:0;
    if(!k||k>n)return 0;
    uint32_t cp=p[0]&((1u<<(7-k))-1);
    for(unsigned i=1;i<k;++i){if((p[i]&0xc0)!=0x80)return 0;cp=(cp<<6)|(p[i]&63);}
    if((k==3&&cp<0x800)||(k==4&&cp<0x10000)||(cp>=0xd800&&cp<=0xdfff)||cp>0x10ffff)return 0;
    return k;
}
// 截断以完整UTF-8字符为界；发生转换时附哈希，避免同前缀标签/存储名混淆。
// Cut only at UTF-8 boundaries; changed names carry a hash to distinguish matching prefixes.
static inline bool font_filename_make(const char *name,char *out,size_t cap,bool storage) {
    if(!font_filename_supported(name)||cap<20)return false;
    const char *stem=name;
    if(!strncmp(stem,"chat_file_",10)) {
        const char *suffix=strchr(stem+10,'_');
        if(suffix&&suffix[1])stem=suffix+1;
    }
    const size_t len=strlen(stem)-4;
    char clean[256];size_t used=0;
    bool changed=stem!=name;
    for(size_t i=0;i<len;) {
        size_t k=font_filename_codepoint((const unsigned char *)stem+i,len-i);
        bool replace=!k || (k==1 && ((unsigned char)stem[i]<32 || stem[i]==127 || strchr("/\\:*?\"<>|",stem[i])));
        // 标签不依赖字库支持补充平面符号；磁盘名仍保留有效Unicode。
        // Labels avoid supplementary symbols that UI faces may lack; disk names retain valid Unicode.
        if(!storage&&k==4)replace=true;
        if(replace){clean[used++]='_';changed=true;}else{memcpy(clean+used,stem+i,k);used+=k;}
        i+=k?k:1;
        if(used>sizeof(clean)-5){changed=true;break;}
    }
    while(used&&(clean[used-1]==' '||clean[used-1]=='.')){--used;changed=true;}
    for(size_t i=0;i<used;++i)if(clean[i]=='.'&&(i==0||clean[i-1]=='.')){clean[i]='_';changed=true;}
    if(used&&clean[0]==' '){clean[0]='_';changed=true;}
    if(!used){memcpy(clean,"Font",4);used=4;changed=true;}
    size_t suffix=storage?4:0;
    if(used+suffix>=cap)changed=true;
    size_t limit=cap-1-suffix-(changed?9:0);
    size_t keep=0;
    while(keep<used){size_t k=font_filename_codepoint((unsigned char *)clean+keep,used-keep);if(!k||keep+k>limit)break;keep+=k;}
    memcpy(out,clean,keep);size_t n=keep;
    if(changed)n+=(size_t)snprintf(out+n,cap-n,"-%08x",(unsigned)font_filename_hash(name));
    if(storage){memcpy(out+n,name+strlen(name)-4,4);n+=4;}
    out[n]=0;return true;
}
static inline void font_filename_distinguish(char *label,size_t cap,const char *path) {
    if(cap<11)return;
    size_t n=strlen(label),keep=0;
    while(keep<n){size_t k=font_filename_codepoint((unsigned char *)label+keep,n-keep);if(!k||keep+k>cap-10)break;keep+=k;}
    snprintf(label+keep,cap-keep,"-%08x",(unsigned)font_filename_hash(path));
}
