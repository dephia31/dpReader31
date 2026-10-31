# SPDX-License-Identifier: Apache-2.0
from pathlib import Path
import os
import subprocess
ROOT=Path(__file__).resolve().parents[1]
def cjson_flags():
    if os.environ.get('CJSON_DIR'):
        base=Path(os.environ['CJSON_DIR']);return ['-I'+str(base),str(base/'cJSON.c')]
    try:
        return subprocess.check_output(['pkg-config','--cflags','--libs','libcjson'],stderr=subprocess.DEVNULL,text=True).split()
    except (OSError,subprocess.CalledProcessError):pass
    for parent in [ROOT/'managed_components',ROOT/'build/boards/metalio47/source/managed_components']:
        base=parent/'espressif__cjson/cJSON'
        if (base/'cJSON.c').exists():return ['-I'+str(base),str(base/'cJSON.c')]
    raise RuntimeError('Install libcjson development package or set CJSON_DIR to the SDK cJSON source directory')
