#!/usr/bin/env python3
"""Stage a verified official NVS256 release with OTA and clean USB installation."""
import json
from pathlib import Path
import re
import shutil
import subprocess

from archive_current_release import archive_s31_flat
from flash_test_usb import validate_bundle
from stage_usb_only_release import APP, ORIGIN, PARTS, ROOT, identity
from verify_metalio_release import clean_image_identity, verify_all


def stage():
    version = json.loads((ROOT/'boards/metalio47/board.json').read_text())['version']
    if not re.fullmatch(r'\d+\.\d+\.\d+-vn', version):
        raise ValueError('An official Full version is required')
    packages = []
    for edition, relative, expected in (
        ('full', 'releases', version),
        ('love-book', 'releases/love-book', version.removesuffix('-vn')+'-love-vn'),
    ):
        package = ROOT/'build/editions'/edition/'packages/metalio47'
        info = validate_bundle(package)
        if info['version'] != expected or info['edition'] != edition:
            raise ValueError(f'Wrong build package: {package}')
        packages.append((package, info, relative))

    for package, info, relative in packages:
        target = ROOT/relative
        version = info['version']
        old = json.loads((target/'metalio-s31-update.json').read_text())
        previous = old['previous_version'] if old['version'] == version else old['version']
        if old['version'] != version:
            old_tag = 'v'+old['version'].replace('-love-vn','-vn')
            old_commit = subprocess.check_output(
                ['git','rev-parse',old_tag+'^{commit}'],cwd=ROOT,text=True).strip()
            archive_s31_flat(target, old_commit)
        zip_name = f'dpReader-metalio47-{version}-USB.zip'
        shutil.copy2(package.parent/zip_name, target/zip_name)
        for name in (APP, *PARTS, 'verification.json'):
            shutil.copy2(package/name, target/name)
        (target/'web-firmware.bin').unlink(missing_ok=True)
        feed = dict(schema=1, edition=info['edition'], version=version, project=info['project'],
                    board='Metalio-EINK4-Plus-S31', layout='metalio-s31-dual-5m-v1',
                    minimum_base_version='1.4.2', usb_only=False,
                    url=ORIGIN+relative+'/'+APP, flash_page=ORIGIN, previous_version=previous,
                    notes='Từ 1.4.1 trở xuống: nạp toàn phần tại dephia31.github.io/dpReader31/. Bản 1.4.2 cập nhật ứng dụng qua USB một lần. OTA hoạt động từ 1.4.3.',
                    usb_parts={name:identity(target/name) for name in PARTS},
                    clean_image=clean_image_identity(target,(target/APP).read_bytes()),
                    **identity(target/APP))
        data = json.dumps(feed,ensure_ascii=False,indent=2)+'\n'
        (target/'metalio-s31-update.json').write_text(data)
        if info['edition']=='full':
            (ROOT/'metalio-s31-update.json').write_text(data)
        names = (APP, *PARTS, 'verification.json', zip_name, 'metalio-s31-update.json')
        (target/'SHA256SUMS.txt').write_text(''.join(
            f"{identity(target/name)['sha256']}  {name}\n" for name in names))
    verify_all()


if __name__ == '__main__':
    stage()
