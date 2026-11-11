#!/usr/bin/env python3
"""中文：校验S31正式版应用、合并镜像与USB/TF包一致。/ English: Validate matching S31 official app, merged image and USB/TF bundles."""
from pathlib import Path
import hashlib,json,re,struct,zipfile,tempfile
from flash_test_usb import validate_bundle
ROOT=Path(__file__).resolve().parents[1]
def clean_image_identity(folder, app):
    image = bytearray(b'\xff') * 0x1000000
    for name, start, limit in (
        ('bootloader.bin', 0x2000, 0x8000),
        ('partitions.bin', 0x8000, 0x9000),
        ('storage-empty.bin', 0xa10000, 0xfb0000),
    ):
        data = (folder/name).read_bytes()
        assert start + len(data) <= limit
        if name == 'storage-empty.bin':
            assert start + len(data) == limit
        image[start:start+len(data)] = data
    assert len(app) <= 0x500000
    image[0x10000:0x10000+len(app)] = app
    return dict(sha256=hashlib.sha256(image).hexdigest(), md5=hashlib.md5(image).hexdigest())

def verify(folder=None, edition="full"):
    folder=Path(folder or ROOT/'releases')
    manifest=json.loads((folder/'metalio-s31-update.json').read_text())
    version=manifest['version'];assert re.fullmatch(r'\d+\.\d+\.\d+'+('-love-vn' if edition=='love-book' else '-vn'),version)
    assert manifest.get('edition',edition)==edition
    assert len(manifest['notes'].encode())<384
    info=json.loads((folder/'verification.json').read_text())
    usb_only=manifest.get('usb_only') is True
    app=(folder/('web-firmware.bin' if usb_only else 'dpReader_metalio47_firmware.bin')).read_bytes()
    nvs256=bool(manifest.get('usb_parts'))
    assert not usb_only or nvs256
    full=None if nvs256 else (folder/'dpReader_metalio47_full.bin').read_bytes()
    assert app[0]==0xe9 and struct.unpack_from('<H',app,12)[0]==32
    assert app[48:80].split(b'\0')[0].decode()==version==info['version']
    assert app[80:112].split(b'\0')[0]==b'Kiikoread_Metalio_S31'
    digest=hashlib.sha256(app).hexdigest()
    assert digest==manifest['sha256']==info['sha256'] and len(app)==manifest['size']==info['size']
    if usb_only:
        assert manifest['minimum_base_version']=='999.0.0' and manifest['url'].endswith('/usb-required.bin')
    if nvs256:
        assert not any((folder/name).exists() for name in ('usb-required.bin','dpReader_metalio47_full.bin',f'dpReader-metalio47-{version}-TF.zip'))
        if usb_only:
            assert not (folder/'dpReader_metalio47_firmware.bin').exists()
        else:
            relative = 'releases/love-book/' if edition == 'love-book' else 'releases/'
            assert manifest['minimum_base_version']=='1.4.2'
            assert manifest['url']=='https://dephia31.github.io/dpReader31/'+relative+'dpReader_metalio47_firmware.bin'
            assert manifest['clean_image']==clean_image_identity(folder,app)
        assert info['storage_layout']=='nvs256-clean' and hashlib.md5(app).hexdigest()==manifest['md5']
        assert set(manifest['usb_parts'])=={'bootloader.bin','partitions.bin','storage-empty.bin'}
        for name, part in manifest['usb_parts'].items():
            data=(folder/name).read_bytes()
            assert part==dict(size=len(data),sha256=hashlib.sha256(data).hexdigest(),md5=hashlib.md5(data).hexdigest())
    else:
        assert full[0x10000:]==app and full[0x2000]==0xe9 and struct.unpack_from('<H',full,0x200c)[0]==32
    assert not any(v for k,v in info.items() if k.startswith('experimental_') or k=='local_transfer_start_test')
    previous=manifest.get('previous_version')
    if previous:
        assert previous!=version and re.fullmatch(r'\d+\.\d+\.\d+'+('-love-vn' if edition=='love-book' else '-vn'),previous)
        old=json.loads((folder/previous/'metalio-s31-update.json').read_text())
        old_app=(folder/previous/('web-firmware.bin' if old.get('usb_only') else 'dpReader_metalio47_firmware.bin')).read_bytes()
        assert len(old_app)==old['size'] and hashlib.sha256(old_app).hexdigest()==old['sha256']
    else:
        assert edition=='love-book', 'Full must retain a previous release'

    for kind in (('USB',) if nvs256 else ('USB','TF')):
        with zipfile.ZipFile(folder/f'dpReader-metalio47-{version}-{kind}.zip') as z:
            assert not z.testzip() and all('/' not in n and '\\' not in n for n in z.namelist())
            assert z.read('dpReader_metalio47_firmware.bin')==app
            if nvs256:
                with tempfile.TemporaryDirectory() as work:
                    z.extractall(work)
                    validate_bundle(Path(work))
                for name in manifest['usb_parts']:
                    assert z.read(name)==(folder/name).read_bytes()
            elif kind=='USB':
                for name,offset in [('bootloader.bin',0x2000),('partitions.bin',0x8000),('ota_data_initial.bin',0xd000)]:
                    data=z.read(name);assert full[offset:offset+len(data)]==data
                assert 'FLASH_USB.command' in z.namelist() and 'FLASH_USB.bat' in z.namelist()
    sums={p[1]:p[0] for line in (folder/'SHA256SUMS.txt').read_text().splitlines() if (p:=line.split())}
    for name,digest in sums.items():assert hashlib.sha256((folder/name).read_bytes()).hexdigest()==digest,name
    print('Verified official S31 release:',version,'app bytes=',len(app))
    return manifest
def verify_all():
    full=verify()
    love_folder=ROOT/'releases/love-book'
    required=tuple(int(x) for x in full['version'].removesuffix('-vn').split('.')) >= (1,3,3)
    love=verify(love_folder,'love-book') if required or love_folder.exists() else None
    if love:
        assert love['version'].removesuffix('-love-vn')==full['version'].removesuffix('-vn')
    return full,love

if __name__=='__main__':verify_all()
