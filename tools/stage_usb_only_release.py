#!/usr/bin/env python3
"""Stage the verified S31 layout-transition release for Web Flash and USB."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

from archive_current_release import archive_s31_flat
from flash_test_usb import validate_bundle
from verify_metalio_release import verify_all

ROOT = Path(__file__).resolve().parents[1]
ORIGIN = 'https://dephia31.github.io/dpReader31/'
APP = 'dpReader_metalio47_firmware.bin'
PARTS = ('bootloader.bin', 'partitions.bin', 'storage-empty.bin')


def identity(path):
    data = path.read_bytes()
    return dict(size=len(data), sha256=hashlib.sha256(data).hexdigest(), md5=hashlib.md5(data).hexdigest())


def stage():
    editions = [('full', '1.4.2-vn', 'releases'), ('love-book', '1.4.2-love-vn', 'releases/love-book')]
    old_commit = subprocess.check_output(['git', 'rev-parse', 'v1.4.1-vn^{commit}'], cwd=ROOT, text=True).strip()
    for edition, version, relative in editions:
        build = ROOT / 'build/editions' / edition / 'packages'
        package = build / 'metalio47'
        target = ROOT / relative
        info = validate_bundle(package)
        if info['version'] != version or info['edition'] != edition:
            raise ValueError(f'Wrong package: {package}')
        old = json.loads((target / 'metalio-s31-update.json').read_text())
        if old['version'] != version:
            archive_s31_flat(target, old_commit)
            previous = old['version']
        else:
            previous = old['previous_version']
        shutil.copy2(package / APP, target / 'web-firmware.bin')
        for name in PARTS + ('verification.json',):
            shutil.copy2(package / name, target / name)
        zip_name = f'dpReader-metalio47-{version}-USB.zip'
        shutil.copy2(build / zip_name, target / zip_name)
        for name in (APP, 'dpReader_metalio47_full.bin'):
            (target / name).unlink(missing_ok=True)
        feed = dict(schema=1, edition=edition, version=version, project=info['project'],
                    board='Metalio-EINK4-Plus-S31', layout='metalio-s31-dual-5m-v1',
                    minimum_base_version='999.0.0', usb_only=True,
                    url=ORIGIN + relative + '/usb-required.bin', flash_page=ORIGIN,
                    previous_version=previous,
                    notes='Cập nhật qua USB tại dephia31.github.io/dpReader31/. Cài từ bố cục cũ sẽ xóa cài đặt và /flash, giữ thẻ SD.',
                    usb_parts={name: identity(target / name) for name in PARTS},
                    **identity(target / 'web-firmware.bin'))
        data = json.dumps(feed, ensure_ascii=False, indent=2) + '\n'
        (target / 'metalio-s31-update.json').write_text(data)
        if edition == 'full':
            (ROOT / 'metalio-s31-update.json').write_text(data)
        names = ('web-firmware.bin', *PARTS, 'verification.json', zip_name, 'metalio-s31-update.json')
        (target / 'SHA256SUMS.txt').write_text(''.join(f"{identity(target/name)['sha256']}  {name}\n" for name in names))
    verify_all()


if __name__ == '__main__':
    stage()
