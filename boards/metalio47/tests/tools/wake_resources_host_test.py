#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""S31实际锁屏缓存与唤醒资源回归。/ S31 retained art and wake resources."""
from pathlib import Path
import runpy
for name in ('metalio_lock_resume_host_test.py', 'metalio_sleep_io_host_test.py', 'metalio_mount_resume_host_test.py'):
    runpy.run_path(str(Path(__file__).with_name(name)),run_name='__main__')
