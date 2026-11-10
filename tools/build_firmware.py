#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""Build one maintained source tree for Pico or Metalio 4.7; never flash or publish."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shlex
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile
from board_source_view import BOARDS, BOARD_ALIASES, canonical_board, materialize

ROOT = Path(__file__).resolve().parents[1]


def metadata(board, edition="full"):
    board = canonical_board(board)
    if board not in BOARDS:
        raise ValueError('Unknown board: ' + board)
    value = json.loads((ROOT / 'boards' / board / 'board.json').read_text())
    if value['id'] != board or value['idf_target'] not in ('esp32s3', 'esp32s31'):
        raise ValueError('Invalid board metadata')
    if edition not in ('full', 'love-book'):
        raise ValueError('Unknown firmware edition: ' + edition)
    value['edition'] = edition
    if edition == 'love-book':
        value['version'] = value['version'].removesuffix('-vn') + '-love-vn'
        value['name'] += ' / Love Book'
    return value


def partition_entries(data):
    result = {}
    for offset in range(0, len(data) - 31, 32):
        magic, kind, subtype, address, size, label, flags = struct.unpack_from('<HBBII16sI', data, offset)
        if magic != 0x50aa:
            break
        result[label.split(b'\0')[0].decode()] = (kind, subtype, address, size)
    return result


def check_image(data, board, edition="full"):
    info = metadata(board, edition)
    if len(data) < 288 or data[0] != 0xe9 or struct.unpack_from('<I', data, 32)[0] != 0xabcd5432:
        raise ValueError('Invalid ESP app image')
    field = lambda start: data[start:start + 32].split(b'\0')[0].decode('ascii')
    chip = struct.unpack_from('<H', data, 12)[0]
    if chip != info['image_chip_id'] or field(80) not in (info['project'], 'Kiikoread_Metalio_S31', 'dpReader') or field(48) != info['version']:
        raise ValueError('Image chip/project/version does not match selected board')
    return {'chip_id': chip, 'project': field(80), 'version': field(48), 'size': len(data),
            'sha256': hashlib.sha256(data).hexdigest()}


def write_usb_launcher(output):
    shutil.copyfile(ROOT / 'tools/flash_test_usb.py', output / 'flash_usb.py')
    batch = r'''@echo off
setlocal
cd /d "%~dp0"
where py >nul 2>nul
if not errorlevel 1 (
  py -3 flash_usb.py %*
) else (
  where python >nul 2>nul
  if errorlevel 1 (
    echo Install Python 3 from python.org, then retry.
    pause
    exit /b 1
  )
  python flash_usb.py %*
)
set "FLASH_RESULT=%ERRORLEVEL%"
echo.
if not "%FLASH_RESULT%"=="0" echo USB flash failed. Read the error above.
pause
exit /b %FLASH_RESULT%
'''
    (output / 'FLASH_USB.bat').write_bytes(batch.replace('\n', '\r\n').encode('ascii'))

    mac = r'''#!/bin/bash
# 中文：使用兼容S31的本机Python环境。/ English: Use a local Python environment supporting S31.
cd -- "$(dirname -- "$0")" || exit 1
shopt -s nullglob
PYTHON_USB=""
for candidate in "$PWD/.usb-python/bin/python" "$HOME"/.espressif/python_env/*/bin/python; do
  if [ -x "$candidate" ] && "$candidate" -c 'import esptool, serial; assert tuple(int(x) for x in esptool.__version__.split(".")[:2]) >= (5, 6)' >/dev/null 2>&1; then
    PYTHON_USB="$candidate"
    break
  fi
done
if [ -z "$PYTHON_USB" ] && command -v python3 >/dev/null 2>&1; then
  candidate="$(command -v python3)"
  if "$candidate" -c 'import esptool, serial; assert tuple(int(x) for x in esptool.__version__.split(".")[:2]) >= (5, 6)' >/dev/null 2>&1; then
    PYTHON_USB="$candidate"
  fi
fi
if [ -z "$PYTHON_USB" ]; then
  echo 'Python with esptool >=5.6 is missing. In this folder run:'
  echo 'python3 -m venv .usb-python'
  echo '.usb-python/bin/python -m pip install "esptool==5.6.0"'
  read -r -p 'Press Enter to close...'
  exit 1
fi
"$PYTHON_USB" flash_usb.py "$@"
FLASH_RESULT=$?
echo
if [ "$FLASH_RESULT" -ne 0 ]; then
  echo 'USB flash failed. Read the error above.'
  if [ -t 0 ]; then read -r -p 'Press Enter to close...'; fi
  exit "$FLASH_RESULT"
fi
# Close only this launcher's dedicated Terminal window, after the shell exits.
# A shared window with other tabs stays open.
if [ "${TERM_PROGRAM:-}" = "Apple_Terminal" ] && [ -t 0 ]; then
  FLASH_TTY="$(tty)"
  /usr/bin/osascript - "$FLASH_TTY" >/dev/null 2>&1 <<'APPLESCRIPT' &
on run argv
  delay 1
  if application "Terminal" is running then
    tell application "Terminal"
      repeat with flashWindow in windows
        if (count of tabs of flashWindow) is 1 then
          if tty of selected tab of flashWindow is item 1 of argv then
            close flashWindow saving no
            return
          end if
        end if
      end repeat
    end tell
  end if
end run
APPLESCRIPT
fi
exit 0
'''
    (output / 'FLASH_USB.command').write_text(mac)
    (output / 'FLASH_USB.command').chmod(0o755)


def export(build, output, board, edition="full", sdk=None, python=None):
    info = metadata(board, edition)
    app = build / (info['project'] + '.bin')
    identity = check_image(app.read_bytes(), board, edition)
    entries = partition_entries((build / 'partition_table/partition-table.bin').read_bytes())
    csv = (ROOT / 'boards' / board / info['partitions']).read_text()
    expected = {}
    for line in csv.splitlines():
        fields = [value.strip() for value in line.split(',')]
        if not fields or not fields[0] or fields[0].startswith('#'):
            continue
        expected[fields[0]] = (int(fields[3], 0), int(fields[4], 0))
    if set(entries) != set(expected) or any(tuple(entries[name][2:]) != shape for name, shape in expected.items()):
        raise ValueError('Built partition table does not match selected board')
    if identity['size'] > entries['ota_0'][3] or identity['size'] > entries['ota_1'][3]:
        raise ValueError('Application does not fit both OTA slots')
    output.mkdir(parents=True, exist_ok=True)
    # 当前包不混入构建缓存中的旧版本应用。/ Exclude older versioned apps left in the package cache.
    for stale in output.glob(f'dpReader-{board}-*.bin'):
        if stale.name != f"dpReader-{board}-{identity['version']}.bin":
            stale.unlink()
    # 从本次SDK构建结果读取地址，S31的bootloader起点与S3不同。
    # Read this build's SDK flash plan: S31 and S3 have different bootloader offsets.
    plan = json.loads((build / 'flasher_args.json').read_text())
    loader = plan['extra_esptool_args']
    if loader['chip'] != info['idf_target']:
        raise ValueError('Flash plan uses the wrong chip')
    expected_offsets = {'bootloader': info['bootloader_offset'], 'partition-table': 0x8000,
                        'app': entries['ota_0'][2], 'otadata': entries['otadata'][2]}
    parts = []
    for key, name in [('bootloader', 'bootloader.bin'), ('partition-table', 'partitions.bin'),
                      ('app', 'firmware.bin'), ('otadata', 'ota_data_initial.bin')]:
        part = plan[key]
        offset = int(part['offset'], 0)
        source = (build / part['file']).resolve()
        if (offset != expected_offsets[key] or not source.is_relative_to(build.resolve()) or
                plan['flash_files'].get(part['offset']) != part['file']):
            raise ValueError('Flash plan does not match the board partition layout')
        if key == 'bootloader' and (source.read_bytes()[0] != 0xe9 or
                struct.unpack_from('<H', source.read_bytes(), 12)[0] != info['image_chip_id']):
            raise ValueError('Bootloader chip does not match the selected board')
        parts.append((name, source, offset))
    cache = build / 'CMakeCache.txt'
    experimental_pie = board == 'metalio47' and cache.exists() and 'METALIO_PIE_PIXELS:BOOL=ON' in cache.read_text()
    experimental_ripple = board == 'metalio47' and cache.exists() and 'METALIO_TEXT_RIPPLE_EXPERIMENT:BOOL=ON' in cache.read_text()
    experimental_panel37 = board == 'metalio47' and cache.exists() and 'METALIO_READER_PANEL37_EXPERIMENT:BOOL=ON' in cache.read_text()
    experimental_shelf37 = board == 'metalio47' and cache.exists() and 'METALIO_SHELF37_EXPERIMENT:BOOL=ON' in cache.read_text()
    experimental_fast_panels = board == 'metalio47' and cache.exists() and 'METALIO_FAST_READER_PANELS:BOOL=ON' in cache.read_text()
    experimental_network = board == 'metalio47' and cache.exists() and 'METALIO_NETWORK_PROBE:BOOL=ON' in cache.read_text()
    transfer_start = board == 'metalio47' and cache.exists() and 'METALIO_LOCAL_TRANSFER_START:BOOL=ON' in cache.read_text()
    manifest = {'edition': edition, 'name': info['name'], 'version': identity['version'], 'board': board,
                'experimental_pie_pixels': bool(experimental_pie),
                'experimental_text_ripple': bool(experimental_ripple),
                'experimental_reader_panel37': bool(experimental_panel37),
                'experimental_shelf37': bool(experimental_shelf37),
                'experimental_fast_reader_panels': bool(experimental_fast_panels),
                'experimental_network_probe': bool(experimental_network),
                'local_transfer_start_test': bool(transfer_start),
                'project': identity['project'], 'new_install_prompt_erase': False,
                'builds': [{'chipFamily': 'ESP32-S3' if board == 'pico' else 'ESP32-S31', 'parts': []}]}
    checksums, args = [], [shlex.join(plan['write_flash_args'])]
    for name, source, offset in parts:
        shutil.copy2(source, output / name)
        digest = hashlib.sha256(source.read_bytes()).hexdigest()
        manifest['builds'][0]['parts'].append({'path': name, 'offset': offset,
                                             'size': source.stat().st_size, 'sha256': digest})
        checksums.append(f'{digest}  {name}\n')
        args.append(f'{offset:#x} {name}')
    shutil.copy2(app, output / 'dpReader_metalio47_firmware.bin')
    shutil.copy2(app, output / f"dpReader-{board}-{identity['version']}.bin")
    # Build merged full image (offset 0x0) for one-file flashing: dpReader_metalio47_full.bin
    max_end = max(offset + source.stat().st_size for _, source, offset in parts)
    full_img = bytearray(b'\xff' * max_end)
    for _, source, offset in parts:
        blob = source.read_bytes()
        full_img[offset:offset + len(blob)] = blob
    full_bin_path = output / 'dpReader_metalio47_full.bin'
    full_bin_path.write_bytes(bytes(full_img))
    checksums.append(f'{hashlib.sha256(full_img).hexdigest()}  dpReader_metalio47_full.bin\n')
    checksums.append(f"{identity['sha256']}  dpReader_metalio47_firmware.bin\n")
    (output / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n')
    (output / 'flash_args').write_text('\n'.join(args) + '\n')
    command = ['python', '-m', 'esptool', '--chip', loader['chip'], '--port', 'PORT',
               '--before', loader['before'], '--after', loader['after']]
    if not loader['stub']:
        command.append('--no-stub')
    command.extend(['write-flash', '@flash_args'])
    (output / 'flash-command.txt').write_text(shlex.join(command) + '\n')
    # FreeType's binary-distribution acknowledgement travels with every bundle.
    ft_license = ROOT / 'components/dpreader_freetype/vendor/docs/FTL.TXT'
    if ft_license.is_file():
        shutil.copyfile(ft_license, output / 'FREETYPE_LICENSE.txt')
        (output / 'FREETYPE_NOTICE.txt').write_text(
            "This software uses the FreeType library, copyright © the FreeType Project (www.freetype.org).\n"
            "FreeType2.14.3; original sources: https://github.com/freetype/freetype/tree/VER-2-14-3\n"
            "Distributed under the FreeType License; see FREETYPE_LICENSE.txt. Bitter font licensing is included in OFL-Bitter.txt.\n")
        for name in ('FREETYPE_LICENSE.txt', 'FREETYPE_NOTICE.txt'):
            checksums.append(f"{hashlib.sha256((output / name).read_bytes()).hexdigest()}  {name}\n")
    shutil.copyfile(ROOT / 'main/assets/OFL-Bitter.txt', output / 'OFL-Bitter.txt')
    checksums.append(f"{hashlib.sha256((output / 'OFL-Bitter.txt').read_bytes()).hexdigest()}  OFL-Bitter.txt\n")
    (output / 'SHA256SUMS.txt').write_text(''.join(checksums))
    (output / 'HUONG_DAN_FLASH.txt').write_text(
        f"Firmware dpReader / {info['name']} / {info['idf_target']} / {identity['version']}\n"
        "- File gộp đầy đủ (nạp tại địa chỉ 0x0): dpReader_metalio47_full.bin\n"
        "- File chỉ riêng ứng dụng (nạp tại 0x10000 hoặc cập nhật thẻ nhớ): dpReader_metalio47_firmware.bin\n"
        "- Chi tiết câu lệnh esptool xem trong flash-command.txt và flash_args.\n")
    config_text = (build / 'config/sdkconfig.h').read_text()
    auto_backup = '#define CONFIG_DPREADER_AUTO_BACKUP 1' in config_text
    auto_restore = '#define CONFIG_DPREADER_AUTO_RESTORE 1' in config_text
    result = {'edition': edition, 'board': board, 'target': info['idf_target'], **identity,
              'partitions': entries, 'bootloader_offset': expected_offsets['bootloader'],
              'esptool_stub': loader['stub'], 'free_app_bytes': entries['ota_0'][3] - identity['size'],
              'firmware_bin': str(output / 'dpReader_metalio47_firmware.bin'),
              'full_bin': str(full_bin_path),
              'experimental_pie_pixels': bool(experimental_pie),
              'experimental_text_ripple': bool(experimental_ripple),
              'experimental_reader_panel37': bool(experimental_panel37),
              'experimental_shelf37': bool(experimental_shelf37),
              'experimental_fast_reader_panels': bool(experimental_fast_panels),
              'experimental_network_probe': bool(experimental_network),
              'local_transfer_start_test': bool(transfer_start),
              'automatic_backup': auto_backup, 'automatic_restore': auto_restore,
              'runtime_memory_validation': 'not_performed_by_build',
              'built': True, 'device_flashed': False, 'published': False}
    (output / 'verification.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    write_usb_launcher(output)
    (output / 'BUILD_NOTES.txt').write_text(
        f"Firmware: {identity['version']} / {info['name']}\n"
        f"Automatic backup: {'ON' if auto_backup else 'OFF'}\n"
        f"Automatic restore: {'ON' if auto_restore else 'OFF'}\n"
        "Compilation does not establish peak internal SRAM usage or device acceptance.\n"
        "USB launchers preserve NVS/books and write only the app and OTA selection.\n"
        "This package has not been flashed or published by the build tool.\n")
    if entries.get('nvs', (0, 0, 0, 0))[2:] == (0xfb0000, 0x40000):
        if sdk is None:
            raise ValueError('NVS256 bundles require the build SDK for FAT migration tools')
        # Generate an empty, mountable FAT/WL image on the build host.
        env = dict(os.environ, PYTHONPATH=os.pathsep.join((str(ROOT / 'tools'), str(sdk / 'components/fatfs'))))
        subprocess.run([str(python or sys.executable), '-c',
                        'from pathlib import Path; import sys; from nvs256_migrate import build_storage; '
                        'Path(sys.argv[1]).write_bytes(build_storage({}, []))',
                        str(output / 'storage-empty.bin')], env=env, check=True)
        for obsolete in ('nvs256_migrate.py', 'fatfs_runtime.zip'):
            (output / obsolete).unlink(missing_ok=True)
        shutil.copyfile(ROOT / 'docs/NVS256_MIGRATION.md', output / 'NVS256_MIGRATION.md')
        for name in ('dpReader_metalio47_full.bin', 'flash_args', 'flash-command.txt', 'manifest.json'):
            (output / name).unlink()
        instructions = (
            f"dpReader {identity['version']} — NVS chung 256 KiB\n"
            "Cài lần đầu CHỈ bằng FLASH_USB.command (macOS) hoặc FLASH_USB.bat (Windows).\n"
            "Python cần esptool >= 5.6 và pyserial. Cài sạch từ bố cục cũ bằng USB.\n"
            "Launcher tự cài sạch từ bố cục cũ, xóa cài đặt và tệp trong /flash. Thẻ SD giữ nguyên.\n"
            "USB trên bố cục mới chỉ ghi app và vùng chọn boot.\n")
        (output / 'HUONG_DAN_FLASH.txt').write_text(instructions)
        (output / 'BUILD_NOTES.txt').write_text(instructions)
        result.pop('full_bin', None)
        result['storage_layout'] = 'nvs256-clean'
        result['usb_clean_install_required_from_legacy'] = True
        (output / 'verification.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    (output / 'SHA256SUMS.txt').write_text(''.join(
        f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.name}\n"
        for path in sorted(output.iterdir()) if path.is_file() and path.name != 'SHA256SUMS.txt'))
    with zipfile.ZipFile(output.parent / f"dpReader-{board}-{identity['version']}-USB.zip", 'w', zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(output.iterdir()):
            if path.is_file():
                archive.write(path, path.name)
    return result


def export_test_bundle(source, destination):
    """Never replace an existing RC with different bytes; publish a complete folder."""
    source, destination = Path(source), Path(destination)
    if destination.exists():
        expected = {p.name: hashlib.sha256(p.read_bytes()).digest()
                    for p in source.iterdir() if p.is_file()}
        actual = {p.name: hashlib.sha256(p.read_bytes()).digest()
                  for p in destination.iterdir() if p.is_file()}
        if actual != expected:
            raise FileExistsError(f"Refusing to overwrite existing test build: {destination}. Choose a new RC.")
        return
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix='.' + destination.name + '-', dir=destination.parent))
    try:
        shutil.copytree(source, temporary, dirs_exist_ok=True)
        temporary.rename(destination)
    finally:
        if temporary.exists():
            shutil.rmtree(temporary)


def build_one(board, args):
    info = metadata(board, args.edition)
    sdk_argument = getattr(args, 'idf_' + board)
    sdk = Path(sdk_argument or os.environ.get('DPREADER_IDF_' + board.upper(), '') or os.environ.get('IDF_PATH', '')).resolve()
    if not (sdk / 'tools/idf.py').is_file() or not (sdk / 'components/soc' / info['idf_target']).is_dir():
        raise ValueError(f"{board} requires an ESP-IDF SDK supporting {info['idf_target']}: {sdk}")
    work = args.output.resolve() / board
    source, build = work / 'source', work / 'build'
    # 虚拟环境解释器常为符号链接；保留入口路径，不能解析成系统Python。
    # Venv executables are often symlinks; retain that entry path instead of resolving to system Python.
    python = args.python.absolute()
    version_marker = work / '.dpreader-built-version'
    reconfigure_needed = not (build / 'build.ninja').exists() or not version_marker.exists() or version_marker.read_text().strip() != info['version']
    if reconfigure_needed:
        for obj in build.glob('**/esp_app_desc.c.obj'):
            try:
                obj.unlink()
            except OSError:
                pass
        for f in (build / (info['project'] + '.elf'), build / (info['project'] + '.bin')):
            if f.exists():
                f.unlink()
    command = [str(python), str(sdk / 'tools/idf.py'), '-B', str(build),
               '-DIDF_TARGET=' + info['idf_target'], '-DDPREADER_BOARD=' + board,
               '-DDPREADER_EDITION=' + args.edition,
               '-DSDKCONFIG=' + str(work / 'sdkconfig'),
               '-DSDKCONFIG_DEFAULTS=' + str(source / 'boards' / board / info['sdkconfig']),
               '-DCCACHE_ENABLE=OFF', '-DMETALIO_LOCAL_LATENCY=OFF',
               '-DMETALIO_PIE_PIXELS=' + ('ON' if board == 'metalio47' and args.s31_pie else 'OFF'),
               '-DMETALIO_TEXT_RIPPLE_EXPERIMENT=' + ('ON' if board == 'metalio47' and args.s31_text_ripple else 'OFF'),
               '-DMETALIO_READER_PANEL37_EXPERIMENT=' + ('ON' if board == 'metalio47' and args.s31_reader_panel37 else 'OFF'),
               '-DMETALIO_SHELF37_EXPERIMENT=' + ('ON' if board == 'metalio47' and args.s31_shelf37 else 'OFF'),
               '-DMETALIO_FAST_READER_PANELS=' + ('ON' if board == 'metalio47' and args.s31_fast_reader_panels else 'OFF'),
               '-DMETALIO_UI_GRAY22_EXPERIMENT=OFF',
               '-DMETALIO_NETWORK_PROBE=' + ('ON' if board == 'metalio47' and args.s31_network_probe else 'OFF'),
               '-DMETALIO_LOCAL_TRANSFER_START=' + ('ON' if board == 'metalio47' and getattr(args, 's31_transfer_startup_test', False) else 'OFF'),
               *(['reconfigure'] if reconfigure_needed else []), 'build']
    if args.dry_run:
        print(json.dumps({'board': board, 'sdk': str(sdk), 'command': command}, ensure_ascii=False))
        return None
    materialize(ROOT, source, board)
    if board == 'metalio47' and (work / 'sdkconfig').exists():
        # 旧缓存不能关闭S31供屏的缓存安全保证；其余选项保留。/ Migrate old S31 caches to cache-safe feeding, retaining every unrelated option.
        path = work / 'sdkconfig'
        required = ('CONFIG_LCD_RGB_ISR_IRAM_SAFE', 'CONFIG_GDMA_CTRL_FUNC_IN_IRAM', 'CONFIG_PM_ENABLE')
        lines = [line for line in path.read_text().splitlines()
                 if not any(line.startswith(key + '=') or line == '# ' + key + ' is not set'
                            for key in required)]
        # 显式迁移旧配置：使用本轮S31原厂对照策略，避免旧sdkconfig掩盖defaults。
        # Migrate old sdkconfig explicitly so it cannot hide the S31 comparison profile.
        transfer={'CONFIG_PM_DFS_INIT_AUTO':'n','CONFIG_ESP_MAIN_TASK_STACK_SIZE':'24576','CONFIG_TINYUSB_MSC_BUFSIZE':'16384','CONFIG_LWIP_TCP_WND_DEFAULT':'65535',
                  'CONFIG_LWIP_TCP_RECVMBOX_SIZE':'64','CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM':'6',
                  'CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM':'64','CONFIG_ESP_WIFI_RX_BA_WIN':'12',
                  'CONFIG_ESP_WIFI_STATIC_TX_BUFFER':'y','CONFIG_ESP_WIFI_DYNAMIC_TX_BUFFER':'n',
                  'CONFIG_ESP_WIFI_TX_BUFFER_TYPE':'0','CONFIG_ESP_WIFI_STATIC_TX_BUFFER_NUM':'6',
                  'CONFIG_ESP_WIFI_CACHE_TX_BUFFER_NUM':'4','CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP':'y',
                  'CONFIG_ESP_WIFI_IRAM_OPT':'y','CONFIG_ESP_WIFI_RX_IRAM_OPT':'y','CONFIG_ESP_WIFI_SLP_IRAM_OPT':'y',
                  'CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL':'16','CONFIG_ESP_WIFI_RX_MGMT_BUF_NUM_DEF':'5',
                  'CONFIG_LWIP_TCP_OOSEQ_MAX_PBUFS':'12','CONFIG_LWIP_TCP_SACK_OUT':'y','CONFIG_LWIP_TCP_SND_BUF_DEFAULT':'5760',
                  'CONFIG_LWIP_TCPIP_RECVMBOX_SIZE':'64','CONFIG_LWIP_TCP_MSS':'1440',
                  'CONFIG_ESP_WIFI_AMPDU_TX_ENABLED':'y','CONFIG_ESP_WIFI_AMPDU_RX_ENABLED':'y',
                  'CONFIG_ESP_WIFI_TX_BA_WIN':'64','CONFIG_LWIP_SO_RCVBUF':'y'}
        lines=[line for line in lines if not any(line.startswith(key+'=') or line=='# '+key+' is not set' for key in transfer)]
        new_sdkconfig = '\n'.join(lines+[key+'=y' for key in required]+
                                  [key+'='+value for key,value in transfer.items()])+'\n'
        if path.read_text() != new_sdkconfig:
            path.write_text(new_sdkconfig)
    cache = getattr(args, 'managed_' + board)
    if cache:
        shutil.copytree(cache, source / 'managed_components', dirs_exist_ok=True)
    env = dict(os.environ)
    env.update(IDF_PATH=str(sdk), IDF_TARGET=info['idf_target'], DPREADER_BOARD=board,
               DPREADER_PROJECT_VER=info['version'], IDF_PYTHON_ENV_PATH=str(python.parent.parent),
               IDF_PYTHON_CHECK_CONSTRAINTS='no', IDF_SKIP_CHECK_SUBMODULES='1',
               IDF_CCACHE_ENABLE='0', IDF_VER='v6.1', ESP_IDF_VERSION='6.1.0')
    for key in ('PICO_PROJECT_VER', 'METALIO_PROJECT_VER', 'DPREADER_LOCAL_LATENCY', 'METALIO_LOCAL_LATENCY'):
        env.pop(key, None)
    extra = [str(python.parent)]
    if args.tools:
        tools = args.tools.resolve()
        env['IDF_TOOLS_PATH'] = str(tools)
        architecture = 'xtensa-esp-elf' if board == 'pico' else 'riscv32-esp-elf'
        extra.extend(str(path) for path in (tools / 'tools' / architecture).glob('**/bin') if path.is_dir())
        extra.extend(str(path) for path in (tools / 'tools/esp32ulp-elf').glob('**/bin') if path.is_dir())
    env['PATH'] = os.pathsep.join(extra + [env.get('PATH', '')])
    with (work / 'build.log').open('w') as log:
        result = subprocess.run(command, cwd=source, env=env, stdout=log, stderr=subprocess.STDOUT)
    if result.returncode:
        print((work / 'build.log').read_text(errors='replace')[-8000:])
        raise RuntimeError(f'{board} build failed; see {work / "build.log"}')
    result = export(build, args.output.resolve() / 'packages' / board, board, args.edition, sdk, python)
    if 'RC' in info['version'] and not getattr(args, 'no_test_export', False):
        destination = ROOT / 'Test Build' / info['version']
        export_test_bundle(args.output.resolve() / 'packages' / board, destination)
    (work / '.dpreader-built-version').write_text(info['version'] + '\n')
    print(json.dumps(result, ensure_ascii=False))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--board', choices=(*BOARDS, *BOARD_ALIASES, 'all'), default='metalio47')
    parser.add_argument('--edition', choices=('full', 'love-book', 'all'), default='all')
    parser.add_argument('--output', type=Path, help='Build root; edition subdirectories are always isolated')
    parser.add_argument('--python', type=Path, default=Path(sys.executable))
    parser.add_argument('--tools', type=Path)
    for board in BOARDS:
        parser.add_argument('--idf-' + board, dest='idf_' + board)
        parser.add_argument('--managed-' + board, dest='managed_' + board, type=Path)
    parser.add_argument('--no-test-export', action='store_true', help='Compile/package verification only; do not write Test Build/')
    parser.add_argument('--dry-run', action='store_true')
    parser.add_argument('--list', action='store_true')
    parser.add_argument('--s31-pie', action='store_true', help='Explicit local S31 PIE experiment; normal builds stay C')
    parser.add_argument('--s31-text-ripple', action='store_true', help='Explicit local S31 four-level text-ripple experiment; ordinary builds remain full grayscale')
    parser.add_argument('--s31-reader-panel37', action='store_true', help='Explicit local S31 GL37 panel experiment; cleanup and normal builds stay native48')
    parser.add_argument('--s31-shelf37', action='store_true', help='Explicit local S31 GL37 shelf art/chrome experiment; other pages/cleanup stay unchanged')
    parser.add_argument('--s31-fast-reader-panels', action='store_true', help='Explicit local four-gray16 font/reading settings experiment')
    parser.add_argument('--s31-network-probe', action='store_true', help='Local S31 read-only radio counters; not a delivery default')
    parser.add_argument('--s31-transfer-startup-test', action='store_true', help='Unattended local S31 transfer startup; never deliver')
    args = parser.parse_args()
    args.board = canonical_board(args.board)
    if args.s31_pie and args.board == 'pico':
        parser.error('--s31-pie requires metalio47 or all')
    if args.s31_text_ripple and args.board == 'pico':
        parser.error('--s31-text-ripple requires metalio47 or all')
    if args.s31_reader_panel37 and args.board == 'pico':
        parser.error('--s31-reader-panel37 requires metalio47 or all')
    if args.s31_shelf37 and args.board == 'pico':
        parser.error('--s31-shelf37 requires metalio47 or all')
    if args.s31_fast_reader_panels and args.board=='pico':
        parser.error('--s31-fast-reader-panels requires metalio47 or all')
    if args.s31_network_probe and args.board == 'pico':
        parser.error('--s31-network-probe requires metalio47 or all')
    if args.s31_transfer_startup_test and args.board == 'pico':
        parser.error('--s31-transfer-startup-test requires metalio47 or all')
    boards = BOARDS if args.board == 'all' else (args.board,)
    if args.list:
        print(json.dumps([metadata(board, edition) for board in boards for edition in (('full','love-book') if args.edition=='all' else (args.edition,))], ensure_ascii=False, indent=2))
        return
    editions = ('full', 'love-book') if args.edition == 'all' else (args.edition,)
    output = args.output or ROOT / 'build/editions'
    for edition in editions:
        args.edition = edition
        args.output = output / edition
        for board in boards:
            build_one(board, args)
    if editions == ('full', 'love-book') and 'metalio47' in boards and not args.dry_run and not args.no_test_export and 'RC' not in metadata('metalio47')['version']:
        from stage_edition_web import stage
        stage(output, output / 'web-preview')


if __name__ == '__main__':
    main()
