#!/usr/bin/env python3
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as d:
    exe=Path(d)/'history'
    # The progress fixture needs the full iterator declaration from its stub.
    header=Path(d)/'progress_api.h'
    header.write_text('#include "nvs.h"\n#define NVS_DEFAULT_PART_NAME "nvs"\nesp_err_t nvs_entry_find(const char*,const char*,nvs_type_t,nvs_iterator_t*);\n')
    subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wno-return-type','-Wno-unused-variable','-fsanitize=undefined',
        '-include',str(header),'-DBOOK_STATE_STORE_TEST=1','-DBOOK_STATE_SD_ROOT="card"','-DBOOK_STATE_FLASH_ROOT="internal"','-DBOOK_PROGRESS_SD_ROOT="legacy"',
        '-I'+str(root/'tools/settings_backup_stubs'),'-I'+str(root/'main/book'),
        str(root/'tools/book_history_files_host_test.c'),str(root/'main/book/book_state_store.c'),str(root/'main/book/book_progress.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],cwd=d,check=True)
