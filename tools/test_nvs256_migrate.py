# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""Migration regression tests; no device access."""
from pathlib import Path
import tempfile
import unittest
import struct
import zlib
from unittest.mock import patch
import nvs256_migrate as m
import flash_test_usb as usb
from wl_fatfsgen import WLFATFS


def image(directory, size=m.OLD_STORAGE_SIZE):
    files = {str(p.relative_to(directory)): p.read_bytes() for p in directory.rglob('*') if p.is_file()}
    dirs = [str(p.relative_to(directory)) for p in directory.rglob('*') if p.is_dir()]
    return m.build_storage(files, dirs, size)



class MigrationTest(unittest.TestCase):
    def test_files_hidden_unicode_empty_and_nested_roundtrip(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            src = root / 'src'; src.mkdir()
            (src / 'Thư mục').mkdir()
            (src / 'empty-dir').mkdir()
            files = {'font.ttf': b'font'*1900, 'Thư mục/Sách tiếng Việt.txt': 'Nội dung'.encode(), '.hidden': b'secret', 'empty': b''}
            for name, content in files.items(): (src / name).write_bytes(content)
            before = image(src)
            rebuilt, checksums = m.rebuild_storage(before, root / 'backup')
            result, dirs = m.extract_fat(m.logical_fat(rebuilt))
            self.assertEqual(result, files)
            self.assertIn('empty-dir', dirs)
            self.assertEqual(len(checksums), len(files))
            self.assertEqual(len(rebuilt), m.NEW_STORAGE_SIZE)

    def test_fat_independent_fsck_and_metadata(self):
        import subprocess
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            files = {'MixedCase 🥭.txt': b'test', 'dir/x': b'data'}
            metadata = {name: bytes([18 if name == 'dir' else 3]) + struct.pack('<BHHHHHH', 0, 0, 0x5821, 0x5821, 0, 0, 0x5821) for name in [*files, 'dir']}
            old = m.build_storage(files, ['dir'], m.OLD_STORAGE_SIZE, metadata)
            new, _ = m.rebuild_storage(old, root / 'backup')
            recovered = {}
            actual, _ = m.extract_fat(m.logical_fat(new), recovered)
            self.assertEqual(actual, files)
            self.assertEqual(recovered, metadata)
            if Path('/sbin/fsck_msdos').exists():
                img = root / 'fat.img'; img.write_bytes(m.logical_fat(new))
                check = subprocess.run(['/sbin/fsck_msdos', '-n', str(img)], capture_output=True, text=True)
                self.assertEqual(check.returncode, 0, check.stdout + check.stderr)

    def test_oversize_and_corruption_fail_closed(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); src = root / 'src'; src.mkdir()
            (src / 'big.bin').write_bytes(b'X' * 5900000)
            before = image(src)
            with self.assertRaises(Exception): m.rebuild_storage(before, root / 'backup')
            corrupt = bytearray(before); corrupt[-4096] ^= 1
            with self.assertRaises(ValueError): m.logical_fat(corrupt)
            corrupt = bytearray(before); corrupt[-8192] ^= 1
            with self.assertRaises(ValueError): m.logical_fat(corrupt)

    def test_raw_nvs_preserves_all_bytes_and_adds_erased_pages(self):
        page = bytearray(b'\xff' * 4096)
        struct.pack_into('<I', page, 0, 0xfffffffe)
        struct.pack_into('<I', page, 28, zlib.crc32(page[4:28], 0xffffffff))
        old = bytes(page) * 4
        new = m.expand_nvs(old)
        self.assertEqual(new[:0x4000], old)
        self.assertEqual(new[0x4000:], b'\xff' * 0x3c000)
        page[8] ^= 1
        with self.assertRaises(ValueError): m.expand_nvs(bytes(page) * 4)

    def test_migration_write_order_readbacks_and_resume(self):
        import json
        from types import SimpleNamespace
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            flash = bytearray(b'\xff' * 0x1000000)
            app = b'app-test'
            (folder / usb.APP_NAME).write_bytes(app)
            (folder / 'partitions.bin').write_bytes(b'TABLE')
            info = {'sha256': m.hashlib.sha256(app).hexdigest()}
            writes = []
            incomplete = folder / 'Migration Backups' / '112233445566'
            incomplete.mkdir(parents=True)
            (incomplete / 'partitions-old.bin').write_bytes(b'incomplete')
            fail = [True]
            def run(cmd, **kwargs):
                if 'read-flash' in cmd:
                    self.assertNotIn('--no-stub', cmd)
                    addr, size = int(cmd[-3], 0), int(cmd[-2], 0)
                    Path(cmd[-1]).write_bytes(flash[addr:addr + size])
                else:
                    self.assertIn('--no-stub', cmd)
                    self.assertEqual(cmd[cmd.index('--after')+1], 'no-reset')
                    addr = int(cmd[-2], 0); data = Path(cmd[-1]).read_bytes()
                    writes.append(addr)
                    flash[addr:addr+len(data)] = data
                    if fail[0] and addr == 0xfb0000:
                        raise RuntimeError('Simulated USB interruption')
                return SimpleNamespace(returncode=0)
            mac = '11:22:33:44:55:66'
            with patch('subprocess.run', side_effect=run), patch('subprocess.check_output', return_value='MAC: ' + mac):
                with self.assertRaises(RuntimeError):
                    m.migrate_device(folder, 'COM7', mac, bytes(flash[0x8000:0x9000]), info, usb.command)
                self.assertNotIn(0x8000, writes)
                backup, = incomplete.parent.glob(incomplete.name + '-retry-*')
                self.assertEqual((incomplete / 'partitions-old.bin').read_bytes(), b'incomplete')
                with self.assertRaisesRegex(ValueError, 'already sealed'):
                    m.migrate_device(folder, 'COM7', mac, b'', info, usb.command)
                self.assertTrue((backup / 'migration.json').exists())
                fail[0] = False
                m.migrate_device(folder, 'COM7', mac, b'', info, usb.command, backup)
                self.assertEqual(writes[-2:], [0xd000, 0x8000])
                self.assertTrue((backup / 'COMPLETE').exists())
                self.assertEqual(flash[0x9000:0xd000], b'\xff'*0x4000)


if __name__ == '__main__': unittest.main()
