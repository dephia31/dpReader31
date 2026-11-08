#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Run the S31 checkpoint/rotation fault tests in an isolated board source view."""
from pathlib import Path
import subprocess
from board_source_view import materialize

ROOT = Path(__file__).resolve().parents[1]
VIEW = ROOT / 'build/auto-backup-host'
materialize(ROOT, VIEW, 'metalio47', tests=True)
(VIEW / 'build/book-tests/settings-card').mkdir(parents=True, exist_ok=True)
for source, output in [('accounts_backup_host_test.c', 'settings-preferences-backup'),
                       ('simple_ui_backup_host_test.c', 'simple-ui-backup'),
                       ('auto_backup_host_test.c', 'auto-backup'),
                       ('auto_restore_host_test.c', 'auto-restore'),
                       ('book_history_backup_host_test.c', 'book-history-backup')]:
    subprocess.run(['cc', '-std=gnu11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=undefined', '-Itools/settings_backup_stubs',
                    '-Icomponents/read_pico_transfer/include',
                    'tools/' + source, '-o', 'build/book-tests/' + output],
                   cwd=VIEW, check=True)
    subprocess.run(['build/book-tests/' + output], cwd=VIEW, check=True)

subprocess.run(['python3', str(ROOT / 'tools/auto_backup_switch_ui_host_test.py')], cwd=ROOT, check=True)

subprocess.run(['python3', str(ROOT / 'tools/auto_backup_timing_host_test.py')], cwd=ROOT, check=True)
