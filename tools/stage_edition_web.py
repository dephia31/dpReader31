#!/usr/bin/env python3
"""Stage both verified firmware editions in an ignored local Web Flash preview."""
from pathlib import Path
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
