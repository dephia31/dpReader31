#!/usr/bin/env python3
"""Actual C reader against plain/gzip/dictzip fixtures and a Python zlib oracle.
SPDX-License-Identifier: Apache-2.0
"""
from pathlib import Path
import ctypes as C
import gzip
import json
import os
import random
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/dictionary-compressed-tests';OUT.mkdir(parents=True,exist_ok=True)
LIB=OUT/('reader.dylib' if sys.platform=='darwin' else 'reader.so')
# Small ABI wrapper owns the real production dictionary/stream structs.
(OUT/'api.c').write_text(r'''
#include "book_dictionary.h"
#include <stdlib.h>
book_dictionary_t *probe_new(void){return calloc(1,sizeof(book_dictionary_t));}
void probe_delete(book_dictionary_t *d){book_dictionary_close(d);free(d);}
const char *probe_text(book_dictionary_t *d){return d->text;}
int probe_state(book_dictionary_t *d){return d->state;}
int probe_handles(book_dictionary_t *d){return !!d->index+!!d->data+!!d->syn;}
''')
subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-dynamiclib' if sys.platform=='darwin' else '-shared','-fPIC','-I'+str(ROOT/'main/book'),str(OUT/'api.c'),str(ROOT/'main/book/book_dictionary.c'),str(ROOT/'main/book/book_dict_file.c'),'-lz','-o',str(LIB)],check=True)
lib=C.CDLL(str(LIB));P=C.c_void_p
lib.probe_new.restype=P;lib.probe_delete.argtypes=[P];lib.probe_text.argtypes=[P];lib.probe_text.restype=C.c_char_p
lib.probe_state.argtypes=[P];lib.probe_handles.argtypes=[P]
lib.book_dictionary_open.argtypes=[P,C.c_char_p,C.c_char_p];lib.book_dictionary_open.restype=C.c_bool
lib.book_dictionary_step.argtypes=[P];lib.book_dictionary_close.argtypes=[P]
lib.book_dict_file_open.argtypes=[C.c_char_p,C.c_bool,C.c_bool];lib.book_dict_file_open.restype=P
lib.book_dict_file_begin.argtypes=[P,C.c_uint32,P,C.c_size_t];lib.book_dict_file_begin.restype=C.c_bool
lib.book_dict_file_step.argtypes=[P];lib.book_dict_file_close.argtypes=[P]
lib.book_dict_file_size.argtypes=[P];lib.book_dict_file_size.restype=C.c_uint32
lib.book_dict_file_rewind.argtypes=[P];lib.book_dict_file_rewind.restype=C.c_bool
lib.book_dict_file_end.argtypes=[P];lib.book_dict_file_end.restype=C.c_bool
lib.book_dict_file_read.argtypes=[P,P,C.c_size_t];lib.book_dict_file_read.restype=C.c_bool

def dictzip(data,chunk=257,extra=b'',flags=0,empty_chunk=False):
    compressor=zlib.compressobj(6,zlib.DEFLATED,-15)
    parts=[compressor.compress(data[i:i+chunk])+compressor.flush(zlib.Z_FULL_FLUSH) for i in range(0,len(data),chunk)]
    if empty_chunk:parts.append(b"\x00\x00\x00\xff\xff")
    tail=compressor.flush(zlib.Z_FINISH)
    lengths=[len(p) for p in parts]
    assert len(lengths)<=32762 and all(0<n<=65535 for n in lengths)
    ra=struct.pack('<HHH',1,chunk,len(parts))+struct.pack('<'+'H'*len(parts),*lengths)
    x=extra+b'RA'+struct.pack('<H',len(ra))+ra
    header=b'\x1f\x8b\x08'+bytes([4|flags])+b'\0\0\0\0\0\xff'+struct.pack('<H',len(x))+x
    if flags&8:header+=b'dictionary.dict\0'
    if flags&16:header+=b'fixture comment\0'
    if flags&2:header+=struct.pack('<H',zlib.crc32(header)&65535)
    result=header+b''.join(parts)+tail+struct.pack('<II',zlib.crc32(data),len(data))
    assert gzip.decompress(result)==data
    return result

def read_range(path,data,offset,size,expected_steps=None):
    f=lib.book_dict_file_open(os.fsencode(path),True,True);assert f,path
    assert lib.book_dict_file_size(f)==len(data)
    buf=C.create_string_buffer(size or 1)
    assert lib.book_dict_file_begin(f,offset,buf,size)
    steps=0
    while True:
        result=lib.book_dict_file_step(f);steps+=1
        assert result>=0,(path,offset,size,result)
        if result==1:break
        assert steps<100000
    assert buf.raw[:size]==data[offset:offset+size],(offset,size)
    if expected_steps is not None:assert steps<=expected_steps,(steps,expected_steps)
    # Cached/random backwards read is valid too.
    assert lib.book_dict_file_begin(f,0,buf,min(size,16))
    while lib.book_dict_file_step(f)==0:pass
    assert buf.raw[:min(size,16)]==data[:min(size,16)]
    assert not lib.book_dict_file_begin(f,len(data)+1,buf,1)
    lib.book_dict_file_close(f)

def dictionary(root,packed_data=True,packed_index=True):
    words={'apple':'quả táo', 'banana':'quả chuối', 'carrot':'cà rốt', 'water':'nước'}
    data=b'';idx=b''
    for word,meaning in words.items():
        raw=meaning.encode();idx+=word.encode()+b'\0'+struct.pack('>II',len(data),len(raw));data+=raw
    root.mkdir();base=root/'test'
    Path(str(base)+('.dict.dz' if packed_data else '.dict')).write_bytes(dictzip(data,chunk=13,flags=26) if packed_data else data)
    Path(str(base)+('.idx.gz' if packed_index else '.idx')).write_bytes(gzip.compress(idx) if packed_index else idx)
    Path(str(base)+'.syn').write_bytes(b'apples\0'+struct.pack('>I',0))
    Path(str(base)+'.ifo').write_text("StarDict's dict ifo file\nversion=3.0.0\nbookname=dpReader compressed fixture\nwordcount=4\nidxfilesize="+str(len(idx))+"\nsametypesequence=m\n")
    return Path(str(base)+'.ifo'),words

def lookup(ifo,word,expected=4):
    d=lib.probe_new();opened=lib.book_dictionary_open(d,os.fsencode(ifo),word.encode())
    if not opened:
        assert expected==-1;assert lib.probe_handles(d)==0;lib.probe_delete(d);return None
    for steps in range(10000):
        state=lib.book_dictionary_step(d)
        if state==4 or state<0:break
    else:raise AssertionError('unbounded lookup')
    assert state==expected,(word,state,expected)
    result=lib.probe_text(d).decode() if state==4 else None
    assert lib.probe_handles(d)==0
    lib.probe_delete(d);return result

with tempfile.TemporaryDirectory() as temp:
    root=Path(temp);rng=random.Random(20261031)
    data=bytes(rng.randrange(256) for _ in range(10000))
    path=root/'random.dict.dz';path.write_bytes(dictzip(data,chunk=257,extra=b'XY\x03\0abc',flags=26))
    for offset,size in [(0,1),(256,2),(257,257),(len(data)-1,1),(len(data),0),(0,len(data))]+[(rng.randrange(len(data)-500),rng.randrange(1,500)) for _ in range(30)]:
        read_range(path,data,offset,size,(size+256)//257+1)
    # Exact multiples and optional final empty RA chunk (dictzip boundary convention).
    for extra in [False,True]:
        path.write_bytes(dictzip(data[:1028],chunk=257,empty_chunk=extra))
        read_range(path,data[:1028],1020,8,1)
    # Ordinary gzip .dz fallback is streamed, bounded and rewindable.
    large=b'A'*200000+b'Vietnamese: '+ 'tiếng Việt'.encode()
    path.write_bytes(gzip.compress(large));read_range(path,large,190000,1000,7)
    path.write_bytes(dictzip(large,chunk=58315));read_range(path,large,190000,1000,1)
    for pd,pi in [(False,False),(False,True),(True,False),(True,True)]:
        ifo,words=dictionary(root/f'dict-{pd}-{pi}',pd,pi)
        for word,meaning in words.items():assert lookup(ifo,word)==meaning
        assert lookup(ifo,'apples')==words['apple'];assert lookup(ifo,'APPLE')==words['apple']
        assert lookup(ifo,'missing',-2) is None
        d=lib.probe_new();assert lib.book_dictionary_open(d,os.fsencode(ifo),b'water')
        lib.book_dictionary_step(d);lib.book_dictionary_close(d);assert lib.probe_handles(d)==0;lib.probe_delete(d)
    # Truncation, invalid RA sizes/chunk count/version, over-limit expanded bytes.
    good=dictzip(data,chunk=257)
    corrupt=[]
    corrupt.extend([good[:i] for i in [0,2,9,12,20,len(good)-9]])
    bad=bytearray(good);struct.pack_into('<H',bad,16,2);corrupt.append(bad)
    bad=bytearray(good);struct.pack_into('<H',bad,18,0);corrupt.append(bad)
    bad=bytearray(good);struct.pack_into('<H',bad,20,65535);corrupt.append(bad)
    bad=bytearray(good);struct.pack_into('<H',bad,22,0);corrupt.append(bad)
    bad=bytearray(good);struct.pack_into('<I',bad,len(bad)-4,0xffffffff);corrupt.append(bad)
    bad=bytearray(good);bad[3]|=0x80;corrupt.append(bad)
    for bad in corrupt:
        path.write_bytes(bad);f=lib.book_dict_file_open(os.fsencode(path),True,True)
        assert not f,('accepted malformed RA header',len(bad))
    # Bad gzip CRC must reject a fully consumed index; no-match scans reach trailer.
    ifo,words=dictionary(root/'bad-index',True,True);idx=ifo.with_suffix('.idx.gz');bad=bytearray(idx.read_bytes());bad[-8]^=1;idx.write_bytes(bad)
    lookup(ifo,'missing',-1)
    # Index ISIZE mismatch is rejected at open.
    bad=bytearray(gzip.compress(b'x'));idx.write_bytes(bad);lookup(ifo,'apple',-1)
    # A deflate chunk damaged in the selected range is never accepted as a definition.
    path.write_bytes(good);bad=bytearray(good);start=12+struct.unpack_from('<H',good,10)[0];bad[start]=0xff;path.write_bytes(bad)
    f=lib.book_dict_file_open(os.fsencode(path),True,True);assert f
    buf=C.create_string_buffer(100);assert lib.book_dict_file_begin(f,0,buf,100);assert lib.book_dict_file_step(f)==-1;lib.book_dict_file_close(f)
    # No SD extraction or sidecar cache files were created by any lookup.
    before=sorted(str(p.relative_to(root)) for p in root.rglob('*'))
    ifo=root/'dict-True-True/test.ifo';lookup(ifo,'apple')
    assert before==sorted(str(p.relative_to(root)) for p in root.rglob('*'))
print('plain/gzip/dictzip RA, cross-chunk/random/boundary/cancel/corruption/no SD writes: PASS')
# Compile the actual stream with failure-injected allocation hooks, UBSan enabled.
obj=OUT/'stream-faults.o'
subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-g','-I'+str(ROOT/'main/book'),'-include',str(ROOT/'tools/book_dict_allocation_stubs.h'),'-Dcalloc=dict_test_calloc','-Dfree=dict_test_free','-c',str(ROOT/'main/book/book_dict_file.c'),'-o',str(obj)],check=True)
exe=OUT/'allocation-test'
subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-g','-I'+str(ROOT/'main/book'),str(ROOT/'tools/book_dict_allocation_host_test.c'),str(ROOT/'main/book/book_dictionary.c'),str(obj),'-lz','-o',str(exe)],check=True)
ra_path=OUT/'allocation.dict.dz';idx_path=OUT/'allocation.idx.gz'
ra_path.write_bytes(dictzip(b'abcdefghij'*1000,chunk=257));idx_path.write_bytes(gzip.compress(b'abcdefghij'*1000))
allocation_dict=OUT/'allocation-dictionary'
if allocation_dict.exists():shutil.rmtree(allocation_dict)
allocation_ifo,_=dictionary(allocation_dict,True,True)
subprocess.run([str(exe),str(ra_path),str(idx_path),str(allocation_ifo)],check=True)
# Real downloaded primary-source dictionary, if available locally (never network in tests).
real=ROOT/'build/notes-real-dictionary/original/eng-fra/eng-fra.ifo'
if real.exists():
    real_data=gzip.decompress(real.with_suffix('.dict.dz').read_bytes())
    packed=real.with_suffix('.dict.dz')
    for offset in [0,58314,58315,len(real_data)-1]+[rng.randrange(len(real_data)-100) for _ in range(20)]:
        n=min(100,len(real_data)-offset);read_range(packed,real_data,offset,n,2)
    for word in ['apple','book','water']:
        value=lookup(real,word);assert value;print('raw FreeDict .idx.gz/.dict.dz:',word,'PASS')
