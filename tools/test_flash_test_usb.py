"""中文：USB测试包校验及保留数据写入计划。/ English: USB test-bundle validation and data-preserving write plan."""
from pathlib import Path
import hashlib
import json
import struct
import tempfile
import unittest
from unittest.mock import patch
import flash_test_usb as usb


class UsbPackageTest(unittest.TestCase):
    def fixture(self, folder):
        data = bytearray(512)
        data[0] = 0xe9
        struct.pack_into('<H', data, 12, 32)
        struct.pack_into('<I', data, 32, 0xabcd5432)
        data[48:48+len(b'1.1.7RC1-vn')] = b'1.1.7RC1-vn'
        project = b'Kiikoread_Metalio_S31'
        data[80:80+len(project)] = project
        (folder / usb.APP_NAME).write_bytes(data)
        info = dict(board='metalio47', target='esp32s31', chip_id=32,
                    version='1.1.7RC1-vn', size=len(data), sha256=hashlib.sha256(data).hexdigest(),
                    partitions={k: [0, 0, *v] for k, v in usb.EXPECTED.items()})
        (folder / 'verification.json').write_text(json.dumps(info))
        return data

    def test_bundle_rejects_damage_wrong_chip_and_version(self):
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            data = self.fixture(folder)
            self.assertEqual(usb.validate_bundle(folder)['version'], '1.1.7RC1-vn')
            for offset in (0, 12, 48, 80, 400):
                changed = bytearray(data)
                changed[offset] ^= 1
                (folder / usb.APP_NAME).write_bytes(changed)
                with self.assertRaises(ValueError):
                    usb.validate_bundle(folder)

    def test_partition_exact_layout_and_size(self):
        data = bytearray(b'\xff' * 0x1000)
        for i, (name, (offset, size)) in enumerate(usb.EXPECTED.items()):
            struct.pack_into('<HBBII16sI', data, i*32, 0x50aa, *usb.PARTITION_TYPES[name],
                             offset, size, name.encode(), 0)
        usb.validate_partitions(data)
        with self.assertRaises(ValueError):
            usb.validate_partitions(data[:-1])
        data[8] ^= 1
        with self.assertRaises(ValueError):
            usb.validate_partitions(data)

    def test_macos_and_windows_ports(self):
        for port in ('COM5', '/dev/cu.usbmodem2301', '/dev/cu.usbserial-A123'):
            usb.validate_port(port)
        for port in ('/dev/disk1', 'COM0', 'COM5;anything', '/tmp/not-a-port'):
            with self.assertRaises(ValueError):
                usb.validate_port(port)

    def test_command_is_s31_rom_without_global_erase(self):
        cmd = usb.command('COM7', 'write-flash', '0x10000', 'app.bin', '0xd000', 'ota.bin')
        self.assertIn('esp32s31', cmd)
        self.assertIn('--no-stub', cmd)
        self.assertNotIn('erase-flash', cmd)
        self.assertEqual(cmd[-5:], ['write-flash', '0x10000', 'app.bin', '0xd000', 'ota.bin'])

    def test_flash_without_backup(self):
        import types
        mac = 'MAC: 1c:29:04:d0:46:64'
        calls = []
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            self.fixture(folder)
            def run(cmd, **kwargs):
                calls.append(cmd)
                if 'read-flash' in cmd:
                    self.assertEqual(cmd[-3:-1], ['0x8000', '0x1000'])
                    table = bytearray(b'\xff' * 4096)
                    for i, (name, (offset, size)) in enumerate(usb.EXPECTED.items()):
                        struct.pack_into('<HBBII16sI', table, i*32, 0x50aa, *usb.PARTITION_TYPES[name], offset, size, name.encode(), 0)
                    Path(cmd[-1]).write_bytes(table)
                else:
                    self.assertIn('write-flash', cmd)
                    self.assertEqual(cmd[-4], '0x10000')
                    self.assertEqual(cmd[-2], '0xd000')
                    self.assertEqual(Path(cmd[-1]).read_bytes(), b'\xff' * 8192)
                return types.SimpleNamespace(returncode=0)
            modules = {'esptool': types.SimpleNamespace(__version__='5.6.0'),
                       'serial.tools': types.SimpleNamespace(list_ports=None)}
            with patch.dict('sys.modules', modules), patch.object(usb, '__file__', str(folder/'flash_usb.py')), \
                    patch('sys.argv', ['flash_usb.py', '--port', 'COM7']), \
                    patch.object(usb.subprocess, 'check_output', return_value=mac), \
                    patch.object(usb.subprocess, 'run', side_effect=run):
                usb.main()
            self.assertEqual(len(calls), 2)
            self.assertFalse((folder/'USB Backups').exists())

    def test_invalid_bundle_never_opens_device(self):
        with patch.object(usb, 'validate_bundle', side_effect=ValueError('invalid')), \
                patch('sys.argv', ['flash_usb.py']), patch.object(usb.subprocess, 'run') as run, \
                patch.object(usb.subprocess, 'check_output') as read:
            with self.assertRaises(ValueError):
                usb.main()
            run.assert_not_called()
            read.assert_not_called()


if __name__ == '__main__':
    unittest.main()
