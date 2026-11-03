#!/usr/bin/env python3
"""Run maintained S31 BLE integration tests in the required board source view.

The board fixture owns HID/learning, memory admission and current lifecycle
integration. Its aggregate also runs bounded asynchronous stop and Sleep tests.
"""
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
view = root / 'build/ble-regression-host'
subprocess.run([sys.executable, str(root/'tools/board_source_view.py'),
                '--board', 'metalio47', '--tests', '--output', str(view)], check=True)
subprocess.run([sys.executable, str(view/'tools/ble_turner_host_test.py')], check=True)
