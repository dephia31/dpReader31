/* SPDX-License-Identifier: Apache-2.0
 * Independent dictzip reader from the published gzip/RA format. No dictd code.
 */
#include "book_dict_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <zlib.h>
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif
#define IO_BYTES 4096u
#define EXPANDED_MAX (512u*1024u*1024u)
#define ZLIB_BUDGET (64u*1024u)
struct book_dict_file {
    FILE *fp;
    z_stream z;
    uint32_t size,physical,position,produced;
    uint32_t *offsets;
    uint16_t chunk_size,chunks;
    unsigned char *chunk;
    uint32_t cached_chunk;
    bool compressed,ra,initialized,ended,error;
    size_t zbytes,in_at,out_at,out_size;
    unsigned char input[IO_BYTES],output[IO_BYTES];
    uint32_t wanted;
    unsigned char *destination;
    size_t remaining;
};
static void *allocate(size_t n) {
#ifdef ESP_PLATFORM
    return heap_caps_calloc(1,n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
#else
    return calloc(1,n);
#endif
}
typedef union {max_align_t alignment;size_t size;} allocation_header_t;
static voidpf zalloc(voidpf opaque,uInt items,uInt size) {
    book_dict_file_t *f=opaque;
    if(size&&items>SIZE_MAX/size)return NULL;
    size_t n=(size_t)items*size;
    if(n>ZLIB_BUDGET-f->zbytes)return NULL;
    allocation_header_t *p=allocate(n+sizeof(*p));if(!p)return NULL;
    p->size=n;f->zbytes+=n;return p+1;
}
static void zfree(voidpf opaque,voidpf memory) {
    if(!memory)return;
    book_dict_file_t *f=opaque;allocation_header_t *p=(allocation_header_t*)memory-1;
    f->zbytes-=p->size;free(p);
}
static uint16_t le16(const unsigned char *b){return (uint16_t)b[0]|(uint16_t)b[1]<<8;}
static uint32_t le32(const unsigned char *b){return (uint32_t)b[0]|(uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24;}
static bool seek_to(FILE *fp,uint32_t off) {
#if LONG_MAX < UINT32_MAX
    if(off>(uint32_t)LONG_MAX)return false;
#endif
    return fseek(fp,(long)off,SEEK_SET)==0;
}
static bool header_read(FILE *fp,void *buf,size_t n,uLong *crc) {
    if(fread(buf,1,n,fp)!=n)return false;
    *crc=crc32(*crc,buf,(uInt)n);return true;
}
/* 0 no RA field,1 valid RA,-1 malformed gzip/dictzip. */
static int parse_ra(book_dict_file_t *f) {
    unsigned char h[10];uLong crc=crc32(0,NULL,0);
    if(!header_read(f->fp,h,sizeof(h),&crc)||h[0]!=31||h[1]!=139||h[2]!=8||(h[3]&0xe0))return -1;
    unsigned flags=h[3];bool found=false;
    if(flags&4) {
        unsigned char word[2];if(!header_read(f->fp,word,2,&crc))return -1;
        unsigned extra=le16(word);
        while(extra) {
            unsigned char sub[4];
            if(extra<4||!header_read(f->fp,sub,4,&crc))return -1;
            extra-=4;unsigned n=le16(sub+2);if(n>extra)return -1;extra-=n;
            if(sub[0]=='R'&&sub[1]=='A') {
                unsigned char ra[6];if(found||n<6||!header_read(f->fp,ra,6,&crc))return -1;
                f->chunk_size=le16(ra+2);f->chunks=le16(ra+4);
                if(le16(ra)!=1||!f->chunk_size||!f->chunks||n!=6u+2u*f->chunks)return -1;
                uint64_t count=((uint64_t)f->size+f->chunk_size-1)/f->chunk_size;
                if(f->chunks<count||f->chunks>count+1)return -1;
                f->offsets=allocate(((size_t)f->chunks+1)*sizeof(uint32_t));
                if(!f->offsets)return -1;
                for(unsigned i=0;i<f->chunks;i++) {
                    if(!header_read(f->fp,word,2,&crc))return -1;
                    uint32_t length=le16(word);
                    if(!length||length>UINT32_MAX-f->offsets[i])return -1;
                    f->offsets[i+1]=f->offsets[i]+length;
                }
                found=true;
            } else {
                while(n) {unsigned take=n>IO_BYTES?IO_BYTES:n;if(!header_read(f->fp,f->input,take,&crc))return -1;n-=take;}
            }
        }
    }
    for(unsigned flag=8;flag<=16;flag<<=1)if(flags&flag) {
        unsigned n=0;unsigned char c;
        do {if(++n>4096||!header_read(f->fp,&c,1,&crc))return -1;}while(c);
    }
    if(flags&2) {unsigned char word[2];if(fread(word,1,2,f->fp)!=2||le16(word)!=(uint16_t)crc)return -1;}
    long header=ftell(f->fp);if(header<0||(uint64_t)header+8>f->physical)return -1;
    if(!found)return 0;
    uint32_t start=(uint32_t)header;
    if(f->offsets[f->chunks]>f->physical-8-start)return -1;
    /* dictzip normally leaves the final empty deflate block after RA chunks. */
    if(f->physical-8-start-f->offsets[f->chunks]>16)return -1;
    for(unsigned i=0;i<=f->chunks;i++)f->offsets[i]+=start;
    f->chunk=allocate((size_t)f->chunk_size+1);if(!f->chunk)return -1;
    f->cached_chunk=UINT32_MAX;return 1;
}
void book_dict_file_close(book_dict_file_t *f) {
    if(!f)return;
    if(f->initialized)inflateEnd(&f->z);
    if(f->fp)fclose(f->fp);
    free(f->offsets);free(f->chunk);free(f);
}
book_dict_file_t *book_dict_file_open(const char *path,bool compressed,bool random) {
    book_dict_file_t *f=allocate(sizeof(*f));if(!f)return NULL;
    f->fp=fopen(path,"rb");if(!f->fp)goto bad;
    if(fseek(f->fp,0,SEEK_END))goto bad;
    long n=ftell(f->fp);if(n<0||(uint64_t)n>UINT32_MAX||fseek(f->fp,0,SEEK_SET))goto bad;
    f->physical=(uint32_t)n;f->size=f->physical;f->compressed=compressed;
    if(!compressed)return f;
    if(f->physical<18||!seek_to(f->fp,f->physical-8)||fread(f->input,1,8,f->fp)!=8)goto bad;
    f->size=le32(f->input+4);if(f->size>EXPANDED_MAX||fseek(f->fp,0,SEEK_SET))goto bad;
    if(random) {
        int ra=parse_ra(f);if(ra<0)goto bad;f->ra=ra==1;
    }
    if(fseek(f->fp,0,SEEK_SET))goto bad;
    f->z.zalloc=zalloc;f->z.zfree=zfree;f->z.opaque=f;
    if(inflateInit2(&f->z,f->ra?-MAX_WBITS:MAX_WBITS+16)!=Z_OK)goto bad;
    f->initialized=true;return f;
bad:
    book_dict_file_close(f);return NULL;
}
uint32_t book_dict_file_size(const book_dict_file_t *f){return f?f->size:0;}
/* Fill at most one 4KiB output block, with an input ceiling to keep pathological
 * no-output streams bounded. Gzip CRC/ISIZE are checked when stream ends. */
static bool fill(book_dict_file_t *f) {
    if(f->error||f->ended||f->ra)return false;
    f->out_at=f->out_size=0;
    unsigned reads=0;
    while(!f->out_size&&!f->ended) {
        if(!f->z.avail_in) {
            if(++reads>32){f->error=true;return false;}
            size_t n=fread(f->input,1,IO_BYTES,f->fp);
            if(!n){f->error=true;return false;}
            f->z.next_in=f->input;f->z.avail_in=(uInt)n;
        }
        f->z.next_out=f->output;f->z.avail_out=IO_BYTES;
        uInt before=f->z.avail_in;int result=inflate(&f->z,Z_NO_FLUSH);
        f->out_size=IO_BYTES-f->z.avail_out;
        if(f->out_size>f->size-f->produced){f->error=true;return false;}
        f->produced+=(uint32_t)f->out_size;
        if(result==Z_STREAM_END) {
            if(f->produced!=f->size||f->z.avail_in||fgetc(f->fp)!=EOF||ferror(f->fp)){f->error=true;return false;}
            f->ended=true;
        } else if(result!=Z_OK||(before==f->z.avail_in&&!f->out_size)){f->error=true;return false;}
    }
    return f->out_size!=0;
}
int book_dict_file_getc(book_dict_file_t *f) {
    if(!f||f->error||f->ra)return EOF;
    if(!f->compressed&&f->out_at==f->out_size) {
        f->out_at=0;f->out_size=fread(f->output,1,sizeof(f->output),f->fp);
        if(!f->out_size){if(ferror(f->fp))f->error=true;return EOF;}
    }
    if(f->compressed&&f->out_at==f->out_size&&!fill(f))return EOF;
    ++f->position;return f->output[f->out_at++];
}
bool book_dict_file_read(book_dict_file_t *f,void *out,size_t n) {
    unsigned char *p=out;
    for(size_t i=0;i<n;i++){int c=book_dict_file_getc(f);if(c==EOF)return false;p[i]=(unsigned char)c;}return true;
}
bool book_dict_file_rewind(book_dict_file_t *f) {
    if(!f||f->ra||fseek(f->fp,0,SEEK_SET))return false;
    f->position=f->produced=0;f->out_at=f->out_size=0;f->ended=f->error=false;
    if(f->compressed){if(inflateReset(&f->z)!=Z_OK)return false;f->z.avail_in=0;f->z.next_in=NULL;}
    return true;
}
bool book_dict_file_end(book_dict_file_t *f) {
    return book_dict_file_getc(f)==EOF&&!f->error&&f->position==f->size;
}
static bool chunk_read(book_dict_file_t *f,uint32_t chunk) {
    if(chunk==f->cached_chunk)return true;
    if(chunk>=f->chunks||!seek_to(f->fp,f->offsets[chunk])||inflateReset(&f->z)!=Z_OK)return false;
    f->z.avail_in=0;f->z.next_in=NULL;
    f->z.next_out=f->chunk;f->z.avail_out=(uInt)f->chunk_size+1;
    uint32_t remain=f->offsets[chunk+1]-f->offsets[chunk];
    while(remain||f->z.avail_in) {
        if(!f->z.avail_in){unsigned n=remain>IO_BYTES?IO_BYTES:remain;if(fread(f->input,1,n,f->fp)!=n)return false;remain-=n;f->z.next_in=f->input;f->z.avail_in=n;}
        uInt in=f->z.avail_in,out=f->z.avail_out;int result=inflate(&f->z,Z_NO_FLUSH);
        if(result!=Z_OK&&result!=Z_STREAM_END)return false;
        if(!f->z.avail_out||(in==f->z.avail_in&&out==f->z.avail_out))return false;
        if(result==Z_STREAM_END&&(remain||f->z.avail_in))return false;
    }
    uint32_t start=chunk*(uint32_t)f->chunk_size;
    uint32_t expected=f->size-start;if(expected>f->chunk_size)expected=f->chunk_size;
    if(f->z.total_out!=expected)return false;
    f->cached_chunk=chunk;return true;
}
bool book_dict_file_begin(book_dict_file_t *f,uint32_t offset,void *out,size_t n) {
    if(!f||offset>f->size||n>f->size-offset)return false;
    f->wanted=offset;f->destination=out;f->remaining=n;
    if(!f->compressed){if(!seek_to(f->fp,offset))return false;f->position=offset;f->out_at=f->out_size=0;}
    else if(!f->ra&&f->position>offset&&!book_dict_file_rewind(f))return false;
    return true;
}
int book_dict_file_step(book_dict_file_t *f) {
    if(!f||f->error)return -1;
    if(!f->remaining)return 1;
    if(f->ra) {
        uint32_t chunk=f->wanted/f->chunk_size;
        if(!chunk_read(f,chunk)){f->error=true;return -1;}
        uint32_t within=f->wanted%f->chunk_size;
        size_t n=f->chunk_size-within;if(n>f->remaining)n=f->remaining;
        memcpy(f->destination,f->chunk+within,n);f->destination+=n;f->wanted+=(uint32_t)n;f->remaining-=n;
        return f->remaining?0:1;
    }
    for(unsigned budget=0;budget<32768&&f->remaining;budget++) {
        int c=book_dict_file_getc(f);if(c==EOF){f->error=true;return -1;}
        if(f->position<=f->wanted)continue;
        *f->destination++=(unsigned char)c;--f->remaining;++f->wanted;
    }
    return f->remaining?0:1;
}
