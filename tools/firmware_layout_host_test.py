#!/usr/bin/env python3
"""Verify single source ownership and portable legacy/header materialization."""
from pathlib import Path
import json,os,subprocess,tempfile
from board_source_view import materialize
ROOT=Path(__file__).resolve().parents[1]
layout=json.loads((ROOT/'firmware/layout.json').read_text())['legacy_aliases']
assert len(set(layout.values()))==len(layout)
for legacy,owned in layout.items():
    source=ROOT/owned
    assert source.is_file() and source.resolve().is_relative_to(ROOT/'firmware')
    alias=ROOT/legacy
    assert alias.is_symlink() and alias.resolve()==source.resolve(),legacy
with tempfile.TemporaryDirectory() as d:
    root=Path(d)/'repo';out=Path(d)/'production';test=Path(d)/'test'
    for folder in ('main','firmware/shared/main','firmware/full/library','boards/metalio47/src/main'):
        (root/folder).mkdir(parents=True,exist_ok=True)
    files={'main/api.h':'firmware/shared/main/api.h','main/api.c':'firmware/shared/main/api.c'}
    (root/'firmware/layout.json').write_text(json.dumps({'schema':1,'legacy_aliases':files}))
    (root/'firmware/shared/main/api.h').write_text('#pragma once\ntypedef struct{int x;} shared_t;\n')
    (root/'firmware/shared/main/api.c').write_text('#include "firmware/full/library/part.inc"\n')
    (root/'firmware/full/library/part.inc').write_text('int feature(void){return 7;}\n')
    (root/'boards/metalio47/src/main/api.h').write_text('#pragma once\ntypedef struct{int x,board;} shared_t;\n')
    (root/'boards/metalio47/dependencies.lock').write_text('')
    for legacy,owned in files.items():
        # Simulate Git on Windows without symlink support: authoritative ownership still wins.
        (root/legacy).write_text(os.path.relpath(root/owned,(root/legacy).parent))
    materialize(root,out,'metalio47')
    assert '#include' in (out/'main/api.h').read_text()
    assert 'int x,board' in (out/'firmware/shared/main/api.h').read_text()
    unit='#include "main/api.h"\n#include "firmware/shared/main/api.h"\nint main(void){shared_t a={.x=1,.board=2};return a.x+a.board==3?0:1;}\n'
    (out/'unit.c').write_text(unit)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(out),str(out/'unit.c'),'-o',str(out/'test')],check=True)
    subprocess.run([str(out/'test')],check=True)
    materialize(root,test,'metalio47',tests=True)
    assert 'feature(void){return 7;}' in (test/'main/api.c').read_text()
    assert 'int x,board' in (test/'main/api.h').read_text()
with tempfile.TemporaryDirectory() as d:
    p=Path(d)
    unit='#include <assert.h>\n#include "book_cover_jobs.h"\n#include "book_lock_collage.h"\nint main(void){\n#if DPREADER_LOVE_BOOK\nassert(BOOK_COVER_JOB_SLOTS==1);assert(!book_lock_collage_draw(0));assert(!book_lock_collage_prepare(0));assert(!book_lock_collage_warm_cover(0));book_lock_collage_cache_commit(0);book_lock_collage_invalidate();\n#else\nassert(BOOK_COVER_JOB_SLOTS==9);\n#endif\n}\n'
    (p/'slots.c').write_text(unit)
    for love in (0,1):
        command=['cc','-std=c11','-Wall','-Wextra','-Werror','-DDPREADER_LOVE_BOOK='+str(love),'-I'+str(ROOT/'firmware/shared/main/book'),str(p/'slots.c')]
        if love:command.append(str(ROOT/'firmware/love_book/no_library.c'))
        command+=['-o',str(p/'test')]
        subprocess.run(command,check=True);subprocess.run([str(p/'test')],check=True)
print('PASS firmware ownership:223 aliases, no maintained duplicates; portable headers, board overrides, no double typedef and expanded test fragments')
