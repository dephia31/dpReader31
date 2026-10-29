#!/usr/bin/env python3
"""中文：更新正式清单前归档当前版本，保留原字节。/ English: Archive the current release before replacing its manifest, retaining original bytes."""
import argparse
import hashlib
import json
from pathlib import Path
import re


def archive_s31_flat(folder: Path, commit: str) -> None:
    """中文：保留S31稳定版原文件与目录入口。/ English: Retain original stable S31 files and versioned entries."""
    manifest=json.loads((folder/'metalio-s31-update.json').read_text())
    version=manifest['version']
    assert re.fullmatch(r'\d+\.\d+\.\d+-vn',version)
    assert re.fullmatch(r'[a-f0-9]{40}',commit)
    sums={line.split()[1]:line.split()[0] for line in (folder/'SHA256SUMS.txt').read_text().splitlines() if line.strip()}
    names=('dpReader_metalio47_firmware.bin','dpReader_metalio47_full.bin','SHA256SUMS.txt','metalio-s31-update.json')
    payloads={name:(folder/name).read_bytes() for name in names}
    for name in names[:2]:assert hashlib.sha256(payloads[name]).hexdigest()==sums[name],name
    app=payloads[names[0]]
    assert len(app)==manifest['size'] and hashlib.sha256(app).hexdigest()==manifest['sha256']
    assert app[48:80].split(b'\0')[0].decode()==version and int.from_bytes(app[12:14],'little')==32
    destination=folder/version
    for name,data in payloads.items():assert not (destination/name).exists() or (destination/name).read_bytes()==data,'immutable archive differs: '+name
    destination.mkdir(exist_ok=True)
    for name,data in payloads.items():(destination/name).write_bytes(data)
    path=folder/'releases.json'
    catalog=json.loads(path.read_text()) if path.exists() else {'schema':1,'releases':[]}
    entry={'version':version,'source_commit':commit,'path':'releases/'+version,
           'files':{n:{'size':len(d),'sha256':hashlib.sha256(d).hexdigest()} for n,d in payloads.items()}}
    old=[r for r in catalog['releases'] if r['version']==version]
    assert not old or old==[entry]
    catalog['releases']=[entry]+[r for r in catalog['releases'] if r['version']!=version]
    catalog['previous_version']=version
    path.write_text(json.dumps(catalog,ensure_ascii=False,indent=2)+'\n')
    print('Archived original official S31 release:',version)


def archive(folder: Path, commit: str, payloads: Path | None = None) -> None:
    if not (folder/'manifest.json').exists() and (folder/'metalio-s31-update.json').exists():
        archive_s31_flat(folder,commit)
        return
    from verify_flash_bundle import check_archived_releases
    folder = folder.resolve()
    payloads = (payloads or folder).resolve()
    manifest = json.loads((folder / 'manifest.json').read_text())
    version = manifest['version']
    assert re.fullmatch(r'\d+\.\d+\.\d+-rc\d+', version)
    assert re.fullmatch(r'[a-f0-9]{40}', commit), 'original release commit required'
    assert manifest.get('new_install_prompt_erase') is True
    catalog_path = folder / 'releases.json'
    catalog = json.loads(catalog_path.read_text()) if catalog_path.exists() else {'schema': 1, 'releases': []}
    release_assets = json.loads((folder / 'release-assets.json').read_text())
    metalio = manifest.get('board') == 'metalio47'
    prefix = 'Metalio-S31-update' if metalio else 'Pico-update'
    tag_prefix = 'metalio-s31-v' if metalio else 'v'
    assert release_assets['tag'] == tag_prefix + version
    parts = manifest['builds'][0]['parts']
    expected = {'bootloader.bin': 0x2000 if metalio else 0, 'partitions.bin': 0x8000, 'firmware.bin': 0x10000,
                'ota_data_initial.bin': 0xd000 if metalio else 0xd10000}
    assert len(parts) == 4 and {p['path']: p['offset'] for p in parts} == expected
    files = {}
    for part in parts:
        name = part['path']
        data = (payloads / name).read_bytes()
        identity = {'size': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
        assert identity == {k: release_assets['files'][name][k] for k in identity}
        assert identity == {k: part[k] for k in identity}
        files[name] = identity
    image = (payloads / 'firmware.bin').read_bytes()
    assert image[48:80].split(b'\0')[0].decode() == version
    assert image[80:112].split(b'\0')[0] == (b'Kiikoread_Metalio_S31' if metalio else b'Read_Pico')
    if metalio:
        assert int.from_bytes(image[12:14], 'little') == 32
        assert manifest['builds'][0]['chipFamily'] == 'ESP32-S31'
        assert manifest.get('project') == 'Kiikoread_Metalio_S31'
    archived = json.loads(json.dumps(manifest))
    for part in archived['builds'][0]['parts']:
        if part['path'] == 'firmware.bin':
            part['path'] = ('../../../' if metalio else '../../') + prefix + '-' + version + '.bin'
    entry = {'version': version, 'manifest': ('metalio/' if metalio else '') + f'releases/{version}/manifest.json',
             'bin': f'{prefix}-{version}.bin', 'source_commit': commit, 'files': files}
    existing = [r for r in catalog['releases'] if r['version'] == version]
    for kind in ('usb', 'tf'):
        key = kind + '_package_url'
        url = manifest.get(key) or (existing[0].get(key) if existing else None)
        if url:
            package = 'kiikoread-metalio47' if metalio else 'kiikoread'
            expected_url = f'https://github.com/wegooo-cell/read-pico-reader/releases/download/{tag_prefix}{version}/{package}-{version}-{kind.upper()}.zip'
            assert url == expected_url
            entry[key] = expected_url
    assert not existing or existing == [entry], 'published archive metadata changed'
    destination = folder / 'releases' / version
    candidates = {(folder.parent if metalio else folder) / entry['bin']: image,
                  destination / 'manifest.json': (json.dumps(archived, ensure_ascii=False, indent=2) + '\n').encode()}
    for name in expected:
        if name != 'firmware.bin':
            candidates[destination / name] = (payloads / name).read_bytes()
    # 中文：先校验全部目标，禁止覆盖任何已发布的不同字节。/ English: Validate all targets first; never overwrite different published bytes.
    for target, data in candidates.items():
        assert not target.exists() or target.read_bytes() == data, 'immutable archive changed: ' + str(target)
    for target, data in candidates.items():
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    catalog['releases'] = [entry] + [r for r in catalog['releases'] if r['version'] != version]
    catalog['previous_version'] = version
    catalog_path.write_text(json.dumps(catalog, ensure_ascii=False, indent=2) + '\n')
    if not metalio:
        check_archived_releases(folder, payloads / 'partitions.bin')
    print('Archived original official release:', version)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path)
    parser.add_argument('--source-commit', required=True)
    parser.add_argument('--payloads', type=Path)
    args = parser.parse_args()
    archive(args.folder, args.source_commit, args.payloads)
