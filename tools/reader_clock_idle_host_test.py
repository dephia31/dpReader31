#!/usr/bin/env python3
"""Exercise the production reader NTP admission without a radio or clock worker."""
from pathlib import Path
import tempfile,subprocess
s=(Path(__file__).resolve().parents[1]/'main/app/app_loop.c').read_text()
line=next(l.strip() for l in s.splitlines() if l.strip().startswith('bool clock_allowed='))
code='''#include <stdbool.h>
#include <assert.h>
static int app_book,other;static bool body;
static bool app_book_reader_body_visible(void){return body;}
static bool allowed(int *current,bool online){struct {bool network_ready;}clock_network={online};
'''+line+'''
return clock_allowed;}
int main(void){body=true;assert(!allowed(&app_book,false));assert(allowed(&app_book,true));
assert(allowed(&other,false));body=false;assert(allowed(&app_book,false));}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.c').write_text(code)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS NTP admission: offline reader defers, online reader/other pages/reader tools admitted')
