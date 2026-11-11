#!/usr/bin/env python3
"""Stage both verified firmware editions in an ignored local Web Flash preview."""
from pathlib import Path
from stage_usb_only_release import APP, identity, PARTS
from verify_metalio_release import clean_image_identity
import argparse
import hashlib
import json
import re
import shutil

ROOT = Path(__file__).resolve().parents[1]


def stage(build_root, output):
    build_root, output = Path(build_root).resolve(), Path(output).resolve()
    if not output.is_relative_to(ROOT / 'build'):
        raise ValueError('Local previews must stay under build/; publication is a separate step')
    packages = {}
    for edition in ('full', 'love-book'):
        package = build_root / edition / 'packages/metalio47'
        info = json.loads((package / 'verification.json').read_text())
        expected = r'\d+\.\d+\.\d+' + ('-love-vn' if edition == 'love-book' else '-vn')
        if not re.fullmatch(expected, info['version']):
            raise ValueError('Web Flash accepts official releases only; RC stays in local USB bundles')
        if info.get('edition') != edition:
            raise ValueError('Wrong package edition: ' + edition)
        for line in (package / 'SHA256SUMS.txt').read_text().splitlines():
            digest, name = line.split(None, 1)
            path = (package / name.lstrip('*')).resolve()
            if not path.is_relative_to(package.resolve()) or hashlib.sha256(path.read_bytes()).hexdigest() != digest:
                raise ValueError('Invalid package checksum: ' + name)
        packages[edition] = (package, info)
    full, love = (packages[e][1] for e in ('full', 'love-book'))
    if full['version'].removesuffix('-vn') != love['version'].removesuffix('-love-vn'):
        raise ValueError('Build both editions at the same candidate version')
    output.mkdir(parents=True, exist_ok=True)
    shutil.copytree(ROOT / 'web', output, dirs_exist_ok=True)
    for edition, (package, info) in packages.items():
        relative = 'releases' if edition == 'full' else 'releases/love-book'
        destination = output / relative
        destination.mkdir(parents=True, exist_ok=True)
        if info.get('storage_layout') == 'nvs256-clean':
            usb_only = info['version'].removesuffix('-vn').removesuffix('-love') == '1.4.2'
            app_name = 'web-firmware.bin' if usb_only else APP
            shutil.copy2(package / APP, destination / app_name)
            for name in PARTS:
                shutil.copy2(package / name, destination / name)
            zip_name = f"dpReader-metalio47-{info['version']}-USB.zip"
            shutil.copy2(package.parent / zip_name, destination / zip_name)
            feed = dict(schema=1, edition=edition, version=info['version'], project=info['project'],
                        board='Metalio-EINK4-Plus-S31', layout='metalio-s31-dual-5m-v1',
                        minimum_base_version='999.0.0' if usb_only else '1.4.2', usb_only=usb_only,
                        url='./usb-required.bin' if usb_only else f'./{relative}/{app_name}',
                        notes='Bản xem trước cài qua USB', usb_parts={n: identity(destination/n) for n in PARTS},
                        **identity(destination/app_name))
            if not usb_only:
                feed['clean_image'] = clean_image_identity(destination,(destination/app_name).read_bytes())
            (destination / 'metalio-s31-update.json').write_text(json.dumps(feed, ensure_ascii=False, indent=2)+'\n')
            continue
        for name in ('dpReader_metalio47_firmware.bin', 'dpReader_metalio47_full.bin'):
            shutil.copy2(package / name, destination / name)
        feed = dict(schema=1, edition=edition, version=info['version'],
                    project=info['project'], board='Metalio-EINK4-Plus-S31',
                    layout='metalio-s31-dual-5m-v1', minimum_base_version='1.0.0',
                    url=f'./{relative}/dpReader_metalio47_firmware.bin',
                    sha256=info['sha256'], size=info['size'],
                    notes='Bản thử cục bộ ' + ('Full' if edition == 'full' else 'Love Book'))
        (destination / 'metalio-s31-update.json').write_text(json.dumps(feed, ensure_ascii=False, indent=2)+'\n')
    print(json.dumps({'local_web_preview': str(output), 'editions': {e:i['version'] for e,(_,i) in packages.items()}}, ensure_ascii=False))
    return output


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-root', type=Path, default=ROOT/'build/editions')
    parser.add_argument('--output', type=Path, default=ROOT/'build/editions/web-preview')
    args = parser.parse_args()
    stage(args.build_root, args.output)
