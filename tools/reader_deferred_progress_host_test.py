#!/usr/bin/env python3
"""Check the actual post-scan checkpoint with success/failure and retry state."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'main/apps/app_book.c').read_text()
line=next(l for l in s.splitlines() if 'if (s_view==READING && s_unsaved>=8' in l)
present=s[s.index('static bool present('):s.index('static app_redraw_t paint_reading(')]
assert present.index('reader_prefetch_end(prep);')<present.index(line)
assert present.index('guard_draw_result(ctx->hl, err);')<present.index(line)
turn=s[s.index('static app_redraw_t turn_page('):s.index('static app_redraw_t turn_page(')+4000]
assert 'if (s_unsaved >= 8 && !s_save_failed) save_progress();' not in turn
code='''#include <assert.h>
#include <stdbool.h>
enum {READING=1};static int s_view,s_unsaved,calls;static bool s_save_failed,fail;
static void save_progress(void){++calls;if(fail)s_save_failed=true;else s_unsaved=0;}
static void finish(void){'''+line+'''}
int main(void){s_view=READING;s_unsaved=7;finish();assert(!calls);
s_unsaved=8;finish();assert(calls==1&&!s_unsaved);
s_unsaved=8;fail=true;finish();assert(calls==2&&s_unsaved==8&&s_save_failed);
finish();assert(calls==2);s_save_failed=false;fail=false;finish();assert(calls==3&&!s_unsaved);
s_view=0;s_unsaved=8;finish();assert(calls==3);}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.c').write_text(code)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS post-scan progress checkpoint: threshold, view, failure, retry; source scan/join ordering')
