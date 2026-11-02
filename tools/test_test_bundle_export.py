#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
import tempfile
from pathlib import Path
import unittest
from unittest.mock import patch
import build_firmware as build

class TestBundleExport(unittest.TestCase):
    def test_immutable_rc_and_identical_retry(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder); source=root/'package'; source.mkdir()
            (source/'firmware.bin').write_bytes(b'first build')
            target=root/'Test Build/1.2.7RC5-vn'
            build.export_test_bundle(source,target)
            build.export_test_bundle(source,target)
            self.assertEqual((target/'firmware.bin').read_bytes(),b'first build')
            (source/'firmware.bin').write_bytes(b'different build')
            with self.assertRaises(FileExistsError): build.export_test_bundle(source,target)
            self.assertEqual((target/'firmware.bin').read_bytes(),b'first build')

    def test_failed_copy_does_not_publish_partial_rc(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder); source=root/'package'; source.mkdir()
            target=root/'Test Build/1.2.7RC5-vn'
            with patch.object(build.shutil,'copytree',side_effect=OSError('interrupted copy')):
                with self.assertRaises(OSError): build.export_test_bundle(source,target)
            self.assertFalse(target.exists())
            self.assertEqual(list(target.parent.iterdir()),[])

if __name__=='__main__': unittest.main()
