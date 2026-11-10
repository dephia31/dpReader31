# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""Compile the actual ESP-IDF NVS core with a memory-backed NOR partition."""
from pathlib import Path
import argparse
import subprocess
import tempfile
parser = argparse.ArgumentParser()
parser.add_argument('--idf', type=Path, required=True)
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix='nvs256-sdk-') as tmp:
    root = Path(tmp)
    stubs = {
        'sdkconfig.h': '#define CONFIG_NVS_ASSERT_ERROR_CHECK 1\n',
        'esp_heap_caps.h': '',
        'spi_flash_mmap.h': '#define ESP_ERR_FLASH_OP_FAIL 0x6001\n',
        'esp_log.h': '\n'.join('#define '+name+'(...) ((void)0)' for name in ('ESP_LOGE','ESP_LOGW','ESP_LOGI','ESP_LOGD','ESP_LOGV')),
        'esp_rom_crc.h': '#include <stdint.h>\nextern "C" uint32_t esp_rom_crc32_le(uint32_t, const uint8_t*, uint32_t);\n',
    }
    for name, data in stubs.items(): (root/name).write_text(data)
    nvs = args.idf/'components/nvs_flash'
    inc = [root,nvs/'src',nvs/'include',nvs/'private_include',args.idf/'components/esp_common/include']
    sources = [nvs/'src'/name for name in ('nvs_storage.cpp','nvs_page.cpp','nvs_pagemanager.cpp','nvs_types.cpp','nvs_item_hash_list.cpp')]
    exe = root/'stress'
    subprocess.run(['c++','-std=c++17','-DNO_DEBUG_STORAGE','-D_GLIBCXX_ASSERTIONS',*[f'-I{p}' for p in inc],str(Path(__file__).with_suffix('.cpp')),*map(str,sources),'-lz','-o',str(exe)],check=True)
    subprocess.run([str(exe),str(root/'old.bin')],check=True)
    from nvs256_migrate import expand_nvs
    assert len(expand_nvs((root/'old.bin').read_bytes()))==0x40000
    print('Installer accepts real SDK-generated legacy NVS and preserves bytes')
