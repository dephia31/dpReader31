#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""中文：深睡资源与原图恢复回归入口。/ English: Retained-art and deferred-media regression entry."""
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).with_name('pico_deep_lock_restore_host_test.py')),run_name='__main__')
