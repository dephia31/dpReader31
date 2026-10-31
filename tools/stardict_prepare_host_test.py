#!/usr/bin/env python3
"""Check actual gzip/dictzip preparation, provenance and failure preservation.
SPDX-License-Identifier: Apache-2.0
"""
from pathlib import Path
import gzip
import struct
import tempfile
from prepare_stardict import prepare
with tempfile.TemporaryDirectory() as temp:
    root=Path(temp);original=root/'original';original.mkdir()
    ifo=original/'demo.ifo';idx=b'apple\0'+struct.pack('>II',0,7)
    ifo.write_text("StarDict's dict ifo file\nversion=3.0.0\nwordcount=1\nidxfilesize=14\nsametypesequence=m\n")
    (original/'demo.idx.gz').write_bytes(gzip.compress(idx));(original/'demo.dict.dz').write_bytes(gzip.compress(b'A fruit'))
    (original/'COPYING').write_text('Fixture authored for this test.')
    result=prepare(ifo,root/'output');assert result['words']==1
    assert (root/'output/demo.dict').read_bytes()==b'A fruit'
    assert (root/'output/demo.idx').read_bytes()==idx
    assert (root/'output/COPYING').is_file() and (root/'output/SOURCE_SHA256.json').is_file()
    try:prepare(ifo,root/'output')
    except ValueError:pass
    else:raise AssertionError('overwrote output')
    (original/'demo.dict.dz').write_bytes(gzip.compress(b'A'))
    try:prepare(ifo,root/'bad-output')
    except ValueError:pass
    else:raise AssertionError('accepted out-of-range definition')
    assert not (root/'bad-output').exists() and ifo.exists()
print('compressed dictionary preparation and failure preservation: PASS')
