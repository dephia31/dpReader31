/* SPDX-License-Identifier: Apache-2.0 */
#include "metalio_wav.h"
#include <string.h>
#include <limits.h>
static uint16_t u16(const uint8_t *p){return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}
static uint32_t u32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
bool metalio_wav_open(FILE *file,metalio_wav_info_t *info){
    if(!file||!info)return false;
    memset(info,0,sizeof(*info));
    if(fseek(file,0,SEEK_END))return false;
    long size=ftell(file);if(size<12||fseek(file,0,SEEK_SET))return false;
    uint8_t header[12];if(fread(header,1,12,file)!=12||memcmp(header,"RIFF",4)||memcmp(header+8,"WAVE",4))return false;
    uint64_t end=(uint64_t)u32(header+4)+8;
    if(end<12||end>(uint64_t)size||end>LONG_MAX)return false;
    bool format=false;uint16_t channels=0;uint64_t pos=12;
    for(unsigned chunks=0;pos+8<=end&&chunks<4096;++chunks){
        uint8_t chunk[8];if(fread(chunk,1,8,file)!=8)return false;
        uint32_t bytes=u32(chunk+4);pos+=8;
        uint64_t next=pos+bytes+(bytes&1u);if(next>end||next>LONG_MAX)return false;
        if(!memcmp(chunk,"fmt ",4)){
            uint8_t fmt[16];if(format||bytes<16||fread(fmt,1,16,file)!=16)return false;
            channels=u16(fmt+2);
            if(u16(fmt)!=1||(channels!=1&&channels!=2)||u32(fmt+4)!=16000||u16(fmt+14)!=16||
               u16(fmt+12)!=channels*2||u32(fmt+8)!=16000u*channels*2)return false;
            format=true;
        }else if(!memcmp(chunk,"data",4)){
            if(!format||!bytes||bytes%(channels*2))return false;
            *info=(metalio_wav_info_t){bytes,channels};return true;
        }
        if(fseek(file,(long)next,SEEK_SET))return false;
        pos=next;
    }
    return false;
}
void metalio_audio_expand(const int16_t *source,int32_t *slots,unsigned frames,unsigned channels,unsigned volume){
    if(volume>100)volume=100;
    for(unsigned i=0;i<frames;++i){
        int32_t mono=source[i*channels];
        if(channels==2)mono=(mono+source[i*channels+1])/2;
        int32_t value=(int32_t)((int64_t)mono*65536*volume/100);
        slots[i*2]=slots[i*2+1]=value;
    }
}
bool metalio_audio_mode_ack(const char *line){
    if(!line)return false;
    const char *p=strstr(line,"SET MODE 1");
    return p&&(p==line||p[-1]==' ')&&(p[10]=='\0'||p[10]==' ');
}
