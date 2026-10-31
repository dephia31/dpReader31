#!/usr/bin/env python3
"""Large actual StarDict scans must not wait one UI tick per 128 records.
SPDX-License-Identifier: Apache-2.0
"""
from pathlib import Path
import ctypes as C
import gzip
import os
import runpy
import struct
import subprocess
import sys
import tempfile
import time
ROOT=Path(__file__).resolve().parents[1]
api=runpy.run_path(str(ROOT/'tools/book_dict_compressed_host_test.py'))
lib=api['lib']

def measure(ifo,query,expected):
    d=lib.probe_new();assert lib.book_dictionary_open(d,os.fsencode(ifo),query.encode())
    began=time.perf_counter();ticks=0
    while True:
        state=lib.book_dictionary_step(d);ticks+=1
        if state==4 or state<0:break
        assert ticks<1000
    assert state==expected,(query,state)
    if state==4:assert lib.probe_text(d)==b'definition'
    assert lib.probe_handles(d)==0
    lib.probe_delete(d)
    return ticks,time.perf_counter()-began

with tempfile.TemporaryDirectory() as tmp:
    root=Path(tmp);words=100000
    idx=b''.join(f'word{i:06}'.encode()+b'\0'+struct.pack('>II',0,10) for i in range(words))
    for packed in (False,True):
        base=root/('packed' if packed else 'plain')
        Path(str(base)+('.idx.gz' if packed else '.idx')).write_bytes(gzip.compress(idx) if packed else idx)
        Path(str(base)+'.dict.dz').write_bytes(api['dictzip'](b'definition',chunk=13))
        Path(str(base)+'.syn').write_bytes(b'alias\0'+struct.pack('>I',words-1))
        ifo=Path(str(base)+'.ifo')
        ifo.write_text("StarDict's dict ifo file\nversion=3.0.0\nbookname=dpReader speed fixture\nwordcount="+str(words)+"\nidxfilesize="+str(len(idx))+"\nsametypesequence=m\n")
        for query,expected,limit in [('word099999',4,27),('missing',-2,27),('alias',4,52)]:
            ticks,elapsed=measure(ifo,query,expected)
            assert ticks<=limit,(packed,query,ticks)
            print(f'100000 records {"gzip" if packed else "plain"} {query}: {ticks} calls, host {elapsed:.3f}s')
    # Exercise the ESP time gate itself with a deterministic 2ms clock sample.
    (root/'esp_timer.h').write_text('#include <stdint.h>\nint64_t esp_timer_get_time(void);\n')
    (root/'budget.c').write_text("""
#include "book_dictionary.h"
#include <assert.h>
int64_t esp_timer_get_time(void){static int64_t now;now+=2000;return now;}
int main(int argc,char **argv){assert(argc==2);book_dictionary_t d={0};
 assert(book_dictionary_open(&d,argv[1],"word099999"));
 assert(book_dictionary_step(&d)==1&&d.scanned==64);
 book_dictionary_close(&d);assert(!d.index&&!d.data&&!d.syn);return 0;}
""")
    obj=root/'timed.o';exe=root/'budget'
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-DESP_PLATFORM=1','-I'+str(root),'-I'+str(ROOT/'main/book'),'-c',str(ROOT/'main/book/book_dictionary.c'),'-o',str(obj)],check=True)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'main/book'),str(root/'budget.c'),str(obj),str(ROOT/'main/book/book_dict_file.c'),'-lz','-o',str(exe)],check=True)
    subprocess.run([str(exe),str(ifo)],check=True)
    # Buffered plain reads must discard lookahead after each random seek.
    path=root/'ranges.dict';data=bytes(range(256))*300;path.write_bytes(data)
    f=lib.book_dict_file_open(os.fsencode(path),False,True);assert f
    for offset,size in [(0,7),(4090,40),(12,5000),(65000,8000),(0,0)]:
        out=C.create_string_buffer(size or 1)
        assert lib.book_dict_file_begin(f,offset,out,size)
        while lib.book_dict_file_step(f)==0:pass
        assert out.raw[:size]==data[offset:offset+size]
    lib.book_dict_file_close(f)
print('PASS: large plain/gzip last/missing/alias scans, bounded call count and buffered plain random seeks; host timings are not device latency')
