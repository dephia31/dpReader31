#!/usr/bin/env python3
"""Prepare a local StarDict set for kiikoread; expand gzip/dictzip without downloading.
SPDX-License-Identifier: Apache-2.0
"""
from pathlib import Path
import argparse
import gzip
import hashlib
import json
import os
import shutil
import struct
import tempfile

LIMIT=512*1024*1024

def metadata(ifo):
    raw=ifo.read_bytes()
    if len(raw)>128*1024: raise ValueError('IFO exceeds 128 KiB')
    lines=raw.decode('utf-8').splitlines()
    if not lines or lines[0]!="StarDict's dict ifo file": raise ValueError('Invalid StarDict IFO')
    fields=dict(line.split('=',1) for line in lines[1:] if '=' in line)
    if fields.get('version') not in ('2.4.2','3.0.0') or fields.get('idxoffsetbits','32')!='32': raise ValueError('Requires StarDict 2.4.2/3.0.0 with 32-bit offsets')
    types=fields.get('sametypesequence','')
    if not types or len(types)>8 or set(types)-set('mhgxt'): raise ValueError('Supported textual fields: m,h,g,x,t; no binary resources')
    return fields

def prepare(ifo,output):
    ifo=Path(ifo).resolve();output=Path(output).resolve()
    fields=metadata(ifo)
    if output.exists(): raise ValueError('Output exists; choose a new folder to preserve existing files')
    output.parent.mkdir(parents=True,exist_ok=True)
    source=ifo.with_suffix('')
    provenance={}
    with tempfile.TemporaryDirectory(prefix='.stardict-',dir=output.parent) as temp:
        stage=Path(temp)
        for ext in ('.ifo','.idx','.dict','.syn'):
            plain=source.with_suffix(ext)
            candidates=[Path(str(source)+ext)]
            if ext=='.idx': candidates.append(Path(str(source)+'.idx.gz'))
            if ext=='.dict': candidates.append(Path(str(source)+'.dict.dz'))
            found=next((p for p in candidates if p.is_file()),None)
            if found is None:
                if ext=='.syn': continue
                raise ValueError('Missing '+ext+' alongside '+str(ifo))
            dest=stage/(source.name+ext)
            digest=hashlib.sha256()
            with found.open('rb') as f:
                while chunk:=f.read(65536):digest.update(chunk)
            provenance[found.name]=digest.hexdigest()
            opener=gzip.open if found.suffix in ('.gz','.dz') else open
            with opener(found,'rb') as reader,dest.open('wb') as writer:
                total=0
                while chunk:=reader.read(65536):
                    total+=len(chunk)
                    if total>LIMIT: raise ValueError('Expanded part exceeds 512 MiB')
                    writer.write(chunk)
        idx=stage/(source.name+'.idx');data=stage/(source.name+'.dict')
        if idx.stat().st_size!=int(fields['idxfilesize']): raise ValueError('idxfilesize mismatch')
        data_size=data.stat().st_size;count=0
        with idx.open('rb') as f:
            while first:=f.read(1):
                word=bytearray(first)
                while word[-1]!=0:
                    byte=f.read(1)
                    if not byte or len(word)>4096: raise ValueError('Invalid index key')
                    word+=byte
                word[:-1].decode('utf-8')
                raw=f.read(8)
                if len(raw)!=8: raise ValueError('Truncated index')
                offset,length=struct.unpack('>II',raw)
                if offset>data_size or length>data_size-offset: raise ValueError('Definition outside dictionary')
                count+=1
        if count!=int(fields['wordcount']): raise ValueError('wordcount mismatch')
        syn=stage/(source.name+'.syn')
        if syn.exists():
            with syn.open('rb') as f:
                while first:=f.read(1):
                    word=bytearray(first)
                    while word[-1]!=0:
                        byte=f.read(1)
                        if not byte or len(word)>4096: raise ValueError('Invalid synonym key')
                        word+=byte
                    word[:-1].decode('utf-8');raw=f.read(4)
                    if len(raw)!=4 or struct.unpack('>I',raw)[0]>=count: raise ValueError('Invalid synonym index')
        # Preserve source notices; do not silently drop dictionary attribution.
        for name in ('COPYING','LICENSE','LICENSE.txt','README','README.md','AUTHORS'):
            notice=ifo.parent/name
            if notice.is_file() and notice.stat().st_size<1024*1024:shutil.copyfile(notice,stage/name)
        (stage/'SOURCE_SHA256.json').write_text(json.dumps(provenance,indent=2)+'\n')
        os.rename(stage,output)
    return {'words':count,'types':fields['sametypesequence'],'output':str(output)}

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('ifo',type=Path);parser.add_argument('output',type=Path)
    args=parser.parse_args()
    try: print(json.dumps(prepare(args.ifo,args.output),ensure_ascii=False))
    except (ValueError,OSError,UnicodeError,KeyError) as e: parser.exit(1,str(e)+'\n')
