#!/usr/bin/env python3
"""Exercise NVS-full progress fallback and restart using the real filesystem."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    executable = Path(directory) / 'progress-test'
    subprocess.run([
        'cc', '-std=gnu11', '-Wall', '-Wextra', '-Werror',
        '-Wno-misleading-indentation', '-Wno-return-type', '-fsanitize=undefined',
        '-DBOOK_PROGRESS_SD_ROOT="progress"', '-I' + str(root / 'main/book'),
        '-I' + str(root / 'tools/book_storage_stubs'),
        str(root / 'tools/progress_sd_host_test.c'), str(root / 'main/book/book_progress.c'),
        '-o', str(executable),
    ], check=True)
    subprocess.run([str(executable)], cwd=directory, check=True)
    subprocess.run([str(executable), 'restart'], cwd=directory, check=True)
