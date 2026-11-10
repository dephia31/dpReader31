#!/usr/bin/env python3
"""中文：跨平台测试包的保留数据USB应用安装器。/ English: Data-preserving USB app installer for test bundles."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import struct
import subprocess
import sys
import tempfile

APP_NAME = 'dpReader_metalio47_firmware.bin'
PARTITION_TYPES = {'nvs': (1, 2), 'otadata': (1, 0), 'phy_init': (1, 1),
                   'ota_0': (0, 16), 'ota_1': (0, 17), 'storage': (1, 129), 'coredump': (1, 3)}
EXPECTED = {'nvs': (0x9000, 0x4000), 'otadata': (0xd000, 0x2000),
            'phy_init': (0xf000, 0x1000), 'ota_0': (0x10000, 0x500000),
            'ota_1': (0x510000, 0x500000), 'storage': (0xa10000, 0x5e0000),
            'coredump': (0xff0000, 0x10000)}


LEGACY = dict(EXPECTED)
EXPECTED = {**LEGACY, 'nvs_old': LEGACY['nvs'], 'nvs': (0xfb0000, 0x40000), 'storage': (0xa10000, 0x5a0000)}
PARTITION_TYPES['nvs_old'] = (1, 2)


def validate_bundle(folder):
    info = json.loads((folder / 'verification.json').read_text(encoding='utf-8'))
    data = (folder / APP_NAME).read_bytes()
    field = lambda at: data[at:at + 32].split(b'\0')[0].decode('ascii')
    if (info['board'] != 'metalio47' or info['target'] != 'esp32s31' or info['chip_id'] != 32 or
            len(data) < 288 or data[0] != 0xe9 or struct.unpack_from('<H', data, 12)[0] != 32 or
            struct.unpack_from('<I', data, 32)[0] != 0xabcd5432 or
            field(48) != info['version'] or field(80) != 'Kiikoread_Metalio_S31' or
            hashlib.sha256(data).hexdigest() != info['sha256'] or len(data) != info['size']):
        raise ValueError('Wrong board/version or damaged firmware. No flash performed.')
    if {name: tuple(value[2:4]) for name, value in info['partitions'].items()} != EXPECTED:
        raise ValueError('Bundle partition layout is not the approved Metalio S31 layout.')
    for line in (folder / 'SHA256SUMS.txt').read_text().splitlines():
        digest, name = line.split('  ', 1)
        if Path(name).name != name or hashlib.sha256((folder / name).read_bytes()).hexdigest() != digest:
            raise ValueError('Bundle checksum mismatch: ' + name)
    if not all((folder / name).is_file() for name in ('nvs256_migrate.py', 'fatfs_runtime.zip', 'partitions.bin')):
        raise ValueError('Incomplete migration bundle')
    validate_partitions((folder / 'partitions.bin').read_bytes().ljust(0x1000, b'\xff'), allow_legacy=False)
    return info


def validate_partitions(data, allow_legacy=True):
    if len(data) != 0x1000:
        raise ValueError('Incomplete partition table. No flash performed.')
    found = {}
    for at in range(0, 0x1000 - 31, 32):
        magic, kind, subtype, offset, size, label, flags = struct.unpack_from('<HBBII16sI', data, at)
        if magic != 0x50aa:
            break
        if label.split(b'\0')[0].decode() in found:
            raise ValueError('Duplicate partition label')
        found[label.split(b'\0')[0].decode()] = (kind, subtype, offset, size, flags)
    trailer = data[len(found) * 32:]
    if trailer[:2] == b'\xeb\xeb':
        if hashlib.md5(data[:len(found)*32]).digest() != trailer[16:32]:
            raise ValueError('Partition-table MD5 mismatch')
        trailer = trailer[32:]
    if trailer != b'\xff' * len(trailer):
        raise ValueError('Unexpected partition-table trailer')
    expected = {name: (*PARTITION_TYPES[name], *shape, 0) for name, shape in EXPECTED.items()}
    if found == expected:
        return 'new'
    legacy = {name: (*PARTITION_TYPES[name], *shape, 0) for name, shape in LEGACY.items()}
    if allow_legacy and found == legacy:
        return 'legacy'
    raise ValueError('Device partition layout differs. No flash performed.')


def validate_port(port):
    if not (re.fullmatch(r'COM[1-9][0-9]*', port, re.I) or
            re.fullmatch(r'/dev/cu\.(?:usbmodem|usbserial)[A-Za-z0-9._-]+', port)):
        raise ValueError('Expected COM5 on Windows or /dev/cu.usbmodem... on macOS.')


def command(port, *args, after='no-reset', before='default-reset'):
    return [sys.executable, '-m', 'esptool', '--chip', 'esp32s31', '--port', port,
            '--baud', '115200', '--no-stub', '--before', before,
            '--after', after, *args]


def main():
    parser = argparse.ArgumentParser(description='Metalio E-ink 4 Plus / S31 USB app flash')
    parser.add_argument('--port')
    parser.add_argument('--resume', help='Resume interrupted migration from its verified backup folder')
    args = parser.parse_args()
    folder = Path(__file__).resolve().parent
    info = validate_bundle(folder)
    try:
        import esptool
        from serial.tools import list_ports
    except ImportError:
        raise RuntimeError('Install esptool in your Python environment: python -m pip install "esptool==5.6.0"')
    if tuple(int(x) for x in esptool.__version__.split('.')[:2]) < (5, 6):
        raise RuntimeError('S31 requires esptool >=5.6. Run: python -m pip install "esptool==5.6.0"')
    port = args.port
    if not port:
        ports = [p for p in list_ports.comports() if p.vid == 0x303a]
        if not ports:
            raise RuntimeError('No Espressif USB device. Connect S31 in download mode and retry.')
        for index, item in enumerate(ports, 1):
            print(f'{index}: {item.device} - {item.description}')
        port = ports[0].device if len(ports) == 1 else ports[int(input('Select port number: ')) - 1].device
    validate_port(port)
    print(f'Metalio E-ink 4 Plus / ESP32-S31 | {info["version"]} | {port}', flush=True)
    print('Checking chip and layout. Legacy layouts require verified NVS/storage migration.', flush=True)
    identity = subprocess.check_output(command(port, 'read-mac'), text=True, stderr=subprocess.STDOUT)
    print(identity)
    match = re.search(r'MAC:\s*([0-9a-f:]{17})', identity, re.I)
    if not match:
        raise RuntimeError('Cannot verify device MAC. No flash performed.')
    mac = match[1].lower()
    # 先只读分区表，旧布局由迁移器备份。/ Read the table first; the migration helper backs up legacy data.
    with tempfile.TemporaryDirectory(prefix='metalio-usb-') as tmp:
        table = Path(tmp) / 'partitions.bin'
        subprocess.run(command(port, 'read-flash', '--flash-size', '16MB', '--no-progress',
                               '0x8000', '0x1000', str(table), after='no-reset'), check=True)
        try:
            layout = validate_partitions(table.read_bytes())
        except ValueError:
            if not args.resume:
                raise
            layout = 'interrupted'

        again = subprocess.check_output(command(port, 'read-mac', after='no-reset'), text=True, stderr=subprocess.STDOUT)
        if not re.search(r'MAC:\s*' + re.escape(mac) + r'\b', again, re.I):
            raise RuntimeError('Device identity changed. No flash performed.')
        if layout == 'legacy' or args.resume:
            from nvs256_migrate import migrate_device
            migrate_device(folder, port, mac, table.read_bytes(), info, command, args.resume)
        else:
            clear = Path(tmp) / 'ota-select-ota0.bin'
            clear.write_bytes(b'\xff' * 0x2000)
            print(f'Installing {info["version"]} on migrated layout.', flush=True)
            subprocess.run(command(port, 'write-flash', '0x10000', str(folder / APP_NAME),
                                   '0xd000', str(clear), after='no-reset'), check=True)
    # S31 USB Serial/JTAG can remain in ROM after an RTS-only reset. Use the
    # supported transient watchdog reset after verification; never run on failure.
    print('Verified USB write. Restarting the device into firmware...', flush=True)
    subprocess.run(command(port, 'read-mac', before='no-reset', after='watchdog-reset'), check=True)
    print(f'USB write completed. After reboot, verify the device version is {info["version"]}.')
    print('Boot and screen behavior still require checking on the device.')


if __name__ == '__main__':
    try:
        main()
    except (Exception, KeyboardInterrupt) as error:
        print(f'ERROR: {error}', file=sys.stderr)
        sys.exit(1)
