#!/usr/bin/env python3
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as d:
    exe=Path(d)/'profile'
    subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=undefined',
        '-I'+str(root/'tools/settings_backup_stubs'),'-I'+str(root/'components/read_pico_transfer/include'),
        str(root/'tools/book_profile_host_test.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],cwd=d,check=True)
