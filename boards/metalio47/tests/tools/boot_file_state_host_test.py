#!/usr/bin/env python3
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as d:
    exe=Path(d)/'boot'
    subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=undefined',
        '-DDPREADER_BOARD_METALIO47=1','-DBOOK_STATE_STORE_TEST=1','-DBOOK_STATE_SD_ROOT="card"','-DBOOK_STATE_FLASH_ROOT="internal"',
        '-I'+str(root/'tools/settings_backup_stubs'),'-I'+str(root/'main'),
        str(root/'tools/boot_file_state_host_test.c'),str(root/'main/book/book_state_store.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],cwd=d,check=True)
    subprocess.run([str(exe),'restart'],cwd=d,check=True)
