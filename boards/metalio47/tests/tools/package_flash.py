#!/usr/bin/env python3
"""中文：从同一次编译生成基础包、TF 升级包与 OTA 清单。/ English: Stage base, TF and OTA from one build."""
from pathlib import Path
from datetime import datetime
from zoneinfo import ZoneInfo
import hashlib, json, shutil, sys
from verify_flash_bundle import check
ROOT=Path(__file__).resolve().parents[1]
def package(build: Path, output: Path):
    config=(build/'config/sdkconfig.h').read_text()
    assert '#define CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE 1' in config, 'rollback bootloader required'
    data=(build/'Read_Pico.bin').read_bytes()
    assert data[0]==0xe9 and int.from_bytes(data[32:36],'little')==0xabcd5432
    version=data[48:80].split(b'\0')[0].decode('ascii')
    assert b'PICO_HTTPS_OTA_V1' in data, 'base package lacks online OTA support'
    upgrade_name=f'Pico-update-{version}.bin'
    # 本地同版本测试只能打到构建目录，已发布文件的下载内容必须固定。
    # Same-version local tests belong in build directories; published downloads stay immutable.
    published=ROOT/'flash'/upgrade_name
    if output.resolve()==(ROOT/'flash').resolve() and published.is_file():
        assert published.read_bytes()==data, (
            'published version already exists with different bytes; '
            'stage local tests in build/, or increment the version for an authorized release')
    manifest=json.loads((ROOT/'flash/manifest.json').read_text())
    manifest['version']=version
    manifest['name']='kiikoread'
    manifest['updated_at']=datetime.now(ZoneInfo('Asia/Shanghai')).isoformat(timespec='seconds')
    manifest['pico_ota']={'layout':'pico-dual-4m-v1','base_version':version,
        'minimum_base_version':'0.3.3-rc72','online_ota_base_version':'0.3.3-rc79','rollback':True}
    output.mkdir(parents=True,exist_ok=True)
    parts={'firmware.bin':build/'Read_Pico.bin','bootloader.bin':build/'bootloader/bootloader.bin',
           'partitions.bin':build/'partition_table/partition-table.bin','ota_data_initial.bin':build/'ota_data_initial.bin'}
    for name,path in parts.items():shutil.copy2(path,output/name)
    shutil.copy2(build/'Read_Pico.bin',output/upgrade_name)
    shutil.copy2(build/'Read_Pico.bin',output/'Pico-update.bin')
    release={'schema':1,'version':version,'project':'Read_Pico','board':'RDP-G01-W','layout':'pico-dual-4m-v1',
        'minimum_base_version':'0.3.3-rc72','url':f'https://wegooo-cell.github.io/read-pico-reader/{upgrade_name}',
        'size':len(data),'sha256':hashlib.sha256(data).hexdigest(),
        'notes':'1. kiikoread 品牌与全新官网\n2. 资料卡简化、姓名排版优化\n3. 水波纹主页完整灰阶\n4. TF 卡兼容、缺失字体闪屏修复\n5. 拼贴及密码锁屏、列表书架、首行缩进微调\n6. 主站联网升级，保留 GitHub 更新途径'}
    (output/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
    (output/'update.json').write_text(json.dumps(release,ensure_ascii=False,indent=2)+'\n')
    check(output)
if __name__=='__main__':package(Path(sys.argv[1]),Path(sys.argv[2]) if len(sys.argv)>2 else ROOT/'flash')
