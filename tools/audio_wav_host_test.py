#!/usr/bin/env python3
"""Execute WAV parser and sample conversion against real malformed/valid files."""
from pathlib import Path
import subprocess,tempfile,struct,wave
R=Path(__file__).resolve().parents[1]
unit=r'''
#include "metalio_wav.h"
#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv){
 assert(argc==3);FILE*f=fopen(argv[1],"rb");assert(f);metalio_wav_info_t info;
 bool ok=metalio_wav_open(f,&info);assert(ok==(atoi(argv[2])!=0));
 if(ok){assert(info.channels==1||info.channels==2);assert(info.data_bytes==16);assert(ftell(f)>0);unsigned char p[16];assert(fread(p,1,16,f)==16);for(int i=0;i<16;++i)assert(p[i]==i);}
 fclose(f);
 int16_t pcm[]={INT16_MIN,INT16_MAX,0,32767};int32_t slots[8];
 metalio_audio_expand(pcm,slots,4,1,100);assert(slots[0]==INT32_MIN&&slots[2]==2147418112&&slots[4]==0);
 for(int i=0;i<4;++i)assert(slots[2*i]==slots[2*i+1]);
 metalio_audio_expand(pcm,slots,2,2,100);assert(slots[0]==0&&slots[2]==16383*65536);
 metalio_audio_expand(pcm,slots,4,1,0);for(int i=0;i<8;++i)assert(!slots[i]);
 for(int i=0;i<65536;++i){int16_t in=(int16_t)i;metalio_audio_expand(&in,slots,1,1,15);assert(slots[0]==(int32_t)((int64_t)in*65536*15/100));}
 assert(metalio_audio_mode_ack("SET MODE 1"));assert(metalio_audio_mode_ack("OK SET MODE 1 OK"));
 assert(!metalio_audio_mode_ack("SET MODE 10"));assert(!metalio_audio_mode_ack("RESET MODE 1"));assert(!metalio_audio_mode_ack(NULL));
}
'''
def chunk(name,payload):return name+struct.pack('<I',len(payload))+payload+(b'\0' if len(payload)%2 else b'')
def riff(body):return b'RIFF'+struct.pack('<I',len(body)+4)+b'WAVE'+body
def fmt(ch=1,rate=16000,bits=16,encoding=1):return chunk(b'fmt ',struct.pack('<HHIIHH',encoding,ch,rate,rate*ch*bits//8,ch*bits//8,bits))
data=chunk(b'data',bytes(range(16)))
cases=[(riff(fmt()+data),True),(riff(fmt(2)+data),True),(riff(chunk(b'JUNK',b'X')+fmt()+chunk(b'LIST',b'abc')+data),True),
 (riff(fmt(rate=24000)+data),False),(riff(fmt(bits=8)+data),False),(riff(fmt(encoding=3)+data),False),(riff(fmt(3)+data),False),
 (riff(data+fmt()),False),(riff(fmt()+fmt()+data),False),(riff(fmt()+chunk(b'data',b'')),False),
 (riff(fmt()+chunk(b'data',b'123')),False),(riff(fmt()+b'data'+struct.pack('<I',0xffffffff)),False),(b'RF64'+riff(fmt()+data)[4:],False)]
valid=riff(fmt()+data)
cases += [(valid[:n],False) for n in range(len(valid))]
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-I'+str(R/'components/metalio_audio/include'),str(p/'test.c'),str(R/'components/metalio_audio/metalio_wav.c'),'-o',str(p/'test')],check=True)
 for i,(blob,expected) in enumerate(cases):
  f=p/f'{i}.wav';f.write_bytes(blob);subprocess.run([str(p/'test'),str(f),str(int(expected))],check=True)
print('PASS actual WAV: validmono/stereo, oddchunks, malformed/truncated, rate/format/bounds, allPCM16values, volume/stereoslots, BHACK')
