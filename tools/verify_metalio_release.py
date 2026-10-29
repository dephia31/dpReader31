#!/usr/bin/env python3
"""中文：校验S31正式版应用、合并镜像与USB/TF包一致。/ English: Validate matching S31 official app, merged image and USB/TF bundles."""
from pathlib import Path
import hashlib,json,re,struct,zipfile
ROOT=Path(__file__).resolve().parents[1]
def verify(folder=None):
    folder=Path(folder or ROOT/'releases')
    manifest=json.loads((folder/'metalio-s31-update.json').read_text())
    version=manifest['version'];assert re.fullmatch(r'\d+\.\d+\.\d+-vn',version)
    assert len(manifest['notes'].encode())<384
    info=json.loads((folder/'verification.json').read_text())
    app=(folder/'dpReader_metalio47_firmware.bin').read_bytes();full=(folder/'dpReader_metalio47_full.bin').read_bytes()
    assert app[0]==0xe9 and struct.unpack_from('<H',app,12)[0]==32
    assert app[48:80].split(b'\0')[0].decode()==version==info['version']
    assert app[80:112].split(b'\0')[0]==b'Kiikoread_Metalio_S31'
    digest=hashlib.sha256(app).hexdigest()
    assert digest==manifest['sha256']==info['sha256'] and len(app)==manifest['size']==info['size']
    assert full[0x10000:]==app and full[0x2000]==0xe9 and struct.unpack_from('<H',full,0x200c)[0]==32
    assert not any(v for k,v in info.items() if k.startswith('experimental_') or k=='local_transfer_start_test')
    previous=manifest['previous_version'];assert previous!=version
    old=json.loads((folder/previous/'metalio-s31-update.json').read_text())
    old_app=(folder/previous/'dpReader_metalio47_firmware.bin').read_bytes()
    assert len(old_app)==old['size'] and hashlib.sha256(old_app).hexdigest()==old['sha256']
    for kind in ('USB','TF'):
        with zipfile.ZipFile(folder/f'dpReader-metalio47-{version}-{kind}.zip') as z:
            assert not z.testzip() and all('/' not in n and '\\' not in n for n in z.namelist())
            assert z.read('dpReader_metalio47_firmware.bin')==app
            if kind=='USB':
                for name,offset in [('bootloader.bin',0x2000),('partitions.bin',0x8000),('ota_data_initial.bin',0xd000)]:
                    data=z.read(name);assert full[offset:offset+len(data)]==data
                assert 'FLASH_USB.command' in z.namelist() and 'FLASH_USB.bat' in z.namelist()
    sums={p[1]:p[0] for line in (folder/'SHA256SUMS.txt').read_text().splitlines() if (p:=line.split())}
    for name,digest in sums.items():assert hashlib.sha256((folder/name).read_bytes()).hexdigest()==digest,name
    print('Verified official S31 release:',version,'app bytes=',len(app))
    return manifest
if __name__=='__main__':verify()
