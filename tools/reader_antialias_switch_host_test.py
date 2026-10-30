#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Real AA setting transaction resets optical mode only after a successful save."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
R=Path(__file__).resolve().parents[1]
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
static bool saved,active,fail,s_reader_cleanup;static unsigned locks,invalidations;static int s_turns;
static void lock_draw(void){assert(!locks++);}
static void unlock_draw(void){assert(locks--==1);}
static void invalidate_prep(void){assert(locks==1);++invalidations;}
static bool app_settings_set_book_antialias(bool on){assert(locks==1);if(fail)return false;saved=on;return true;}
static void ttf_set_antialias(bool on){assert(locks==1);active=on;}
'''+function('reader_apply_antialias',R/'main/apps/app_book.c')+r'''
int main(void){
 saved=active=true;s_turns=7;assert(reader_apply_antialias(false));assert(!saved&&!active&&!locks&&invalidations==1);
#if KIIKO_BOARD_METALIO47
 assert(s_reader_cleanup&&s_turns==0);
#else
 assert(!s_reader_cleanup&&s_turns==7);
#endif
 s_reader_cleanup=false;s_turns=6;fail=true;assert(!reader_apply_antialias(true));assert(!saved&&!active&&!s_reader_cleanup&&s_turns==6&&!locks);
 fail=false;assert(reader_apply_antialias(true));assert(saved&&active&&!locks&&invalidations==3);
#if KIIKO_BOARD_METALIO47
 assert(s_reader_cleanup&&s_turns==0);
#endif
 puts("PASS actual AA switch: both directions, reset only after save, failed save preserves mode/count, locks balanced, board isolation");}
'''
source=(R/'main/apps/app_book.c').read_text();part=source[source.index('bool ok = reader_apply_antialias(on);'):];assert 'if(ok)return APP_REDRAW_FULL;' in part[:700]
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(unit)
 for board in (0,1):
  subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',f'-DKIIKO_BOARD_METALIO47={board}',str(p/'test.c'),'-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test')],check=True)
