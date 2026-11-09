#!/usr/bin/env python3
"""Checked SD persistence with full NVS, interrupted writes and process restart."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    exe=Path(directory)/'state'
    subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wno-return-type','-fsanitize=undefined',
        '-DBOOK_STATE_STORE_TEST=1','-DBOOK_STATE_SD_ROOT="card"','-DBOOK_STATE_FLASH_ROOT="internal"','-DBOOK_PROGRESS_SD_ROOT="legacy"',
        '-DESP_ERR_INVALID_RESPONSE=0x108','-DESP_ERR_INVALID_SIZE=0x104','-DESP_ERR_NOT_FOUND=0x105',
        '-I'+str(root/'main/book'),'-I'+str(root/'tools/book_storage_stubs'),
        str(root/'tools/book_state_files_host_test.c'),str(root/'main/book/book_progress.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],cwd=directory,check=True)
    subprocess.run([str(exe),'restart'],cwd=directory,check=True)
