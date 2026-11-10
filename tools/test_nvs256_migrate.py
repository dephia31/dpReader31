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

if __name__ == '__main__': unittest.main()
