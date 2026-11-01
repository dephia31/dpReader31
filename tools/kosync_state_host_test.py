#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
from pathlib import Path
import tempfile,subprocess
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'freertos').mkdir();(p/'freertos/FreeRTOS.h').write_text('#pragma once\n')
 (p/'freertos/portmacro.h').write_text('#pragma once\ntypedef int portMUX_TYPE;\n#define portMUX_INITIALIZER_UNLOCKED 0\n#define portENTER_CRITICAL(p) ((void)(p))\n#define portEXIT_CRITICAL(p) ((void)(p))\n')
 (p/'clock.h').write_text('#include <time.h>\ntime_t fake_time(time_t*);\n#define time fake_time\nint fake_rename(const char*,const char*);\n#define rename fake_rename\n')
 args=['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-D_DEFAULT_SOURCE', '-DKOSYNC_STATE_PARENT=\"'+str(p/'sd')+'\"', '-DKOSYNC_STATE_ROOT=\"'+str(p/'sd/state')+'\"','-include',str(p/'clock.h'),'-I'+str(p),'-Itools/book_epub_stubs','-Imain/book']
 subprocess.run([*args,'tools/kosync_state_host_test.c','main/book/kosync_state.c','-o',str(p/'test')],cwd=root,check=True)
 subprocess.run([str(p/'test')],cwd=root,check=True)
