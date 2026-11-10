# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""S31 USB migration helpers. Never access hardware at import time."""
from pathlib import Path
import hashlib
import struct
import sys
import zlib

OLD_STORAGE_SIZE = 0x5e0000
NEW_STORAGE_SIZE = 0x5a0000


def require(condition, message):
    if not condition:
        raise ValueError(message)


def expand_nvs(data):
    require(len(data) == 0x4000, 'Incomplete legacy NVS')
    for pos in range(0, len(data), 4096):
        page = data[pos:pos + 4096]
        if page == b'\xff' * 4096:
            continue
        state, = struct.unpack_from('<I', page)
        require(state in (0xfffffffe, 0xfffffffc, 0xfffffff8), 'Unsupported/corrupt NVS page')
        require(zlib.crc32(page[4:28], 0xffffffff) == struct.unpack_from('<I', page, 28)[0], 'NVS header CRC failed')
    # 原样保留所有命名空间和密钥；新增页为空。/ Preserve all namespaces/keys verbatim; append erased pages.
    return data + b'\xff' * (0x40000 - len(data))


def logical_fat(data):
    from wl_fatfsgen import WLFATFS, remove_wl
    from fatfs_utils.utils import crc32
    require(len(data) in (OLD_STORAGE_SIZE, NEW_STORAGE_SIZE), 'Unexpected storage size')
    sectors = len(data) // 4096
    state_size = ((64 + 16 * sectors + 4095) // 4096) * 4096
    a = data[-4096 - 2 * state_size:-4096 - state_size]
    b = data[-4096 - state_size:-4096]
    require(a == b, 'WL state copies differ; boot original firmware cleanly before migration')
    require(crc32(list(a[:60]), 0xffffffff) == struct.unpack_from('<I', a, 60)[0], 'WL state CRC failed')
    cfg = data[-4096:]
    require(crc32(list(cfg[:32]), 0xffffffff) == struct.unpack_from('<I', cfg, 32)[0], 'WL config CRC failed')
    require(struct.unpack_from('<III', cfg, 4) == (len(data), 4096, 4096), 'Unsupported WL geometry')
    require(struct.unpack_from('<I', a, 20)[0] == 4096, 'Unsupported WL block size')
    plain = remove_wl(data)
    require(plain[510:512] == b'\x55\xaa', 'Invalid FAT boot sector')
    return plain


def extract_fat(data, metadata=None):
    """Strict FAT12/16 reader; keep paths and bytes, reject ambiguous/corrupt chains."""
    bps, = struct.unpack_from('<H', data, 11)
    spc = data[13]
    reserved, = struct.unpack_from('<H', data, 14)
    nfats = data[16]
    roots, total16 = struct.unpack_from('<HH', data, 17)
    fatsz, = struct.unpack_from('<H', data, 22)
    total = total16 or struct.unpack_from('<I', data, 32)[0]
    require(bps == 4096 and spc and not spc & (spc - 1) and nfats in (1, 2) and roots and fatsz, 'Unsupported FAT geometry')
    root_sectors = (roots * 32 + bps - 1) // bps
    data_sector = reserved + nfats * fatsz + root_sectors
    count = (total - data_sector) // spc
    require(0 < count < 65525 and total * bps == len(data), 'Invalid FAT bounds')
    bits = 12 if count < 4085 else 16
    fat = data[reserved * bps:(reserved + fatsz) * bps]
    for i in range(1, nfats):
        require(fat == data[(reserved + i * fatsz) * bps:(reserved + (i + 1) * fatsz) * bps], 'FAT copies differ')
    claimed = set()
    def chain(cluster):
        result = bytearray()
        while cluster < (0xff8 if bits == 12 else 0xfff8):
            require(2 <= cluster < count + 2 and cluster not in claimed, 'Invalid, cyclic or cross-linked FAT chain')
            claimed.add(cluster)
            start = (data_sector + (cluster - 2) * spc) * bps
            result.extend(data[start:start + spc * bps])
            at = cluster * 3 // 2 if bits == 12 else cluster * 2
            val, = struct.unpack_from('<H', fat, at)
            cluster = ((val >> 4) if cluster & 1 else (val & 0xfff)) if bits == 12 else val
        return bytes(result)
    files, directories = {}, []
    seen = set()
    def walk(raw, parent='', depth=0):
        require(depth < 64, 'FAT directory nesting too deep')
        lfn = []
        for pos in range(0, len(raw), 32):
            entry = raw[pos:pos + 32]
            if entry[0] == 0:
                break
            if entry[0] == 0xe5:
                lfn = []
                continue
            attr = entry[11]
            if attr == 0x0f:
                lfn.append(entry)
                continue
            if attr & 8:
                lfn = []
                continue
            short = entry[:11]
            base = short[:8].decode('cp437').rstrip()
            ext = short[8:].decode('cp437').rstrip()
            if entry[12] & 8: base = base.lower()
            if entry[12] & 16: ext = ext.lower()
            name = base + ('.' + ext if ext else '')
            if lfn:
                checksum = 0
                for c in short: checksum = (((checksum & 1) << 7) + (checksum >> 1) + c) & 255
                require(lfn[0][0] == (len(lfn) | 0x40), 'Invalid long filename sequence')
                chunks = []
                for index, ent in enumerate(reversed(lfn), 1):
                    require(ent[0] & 0x1f == index and ent[13] == checksum, 'Invalid long filename checksum')
                    chunks.append(ent[1:11] + ent[14:26] + ent[28:32])
                name = b''.join(chunks).decode('utf-16le').split('\0')[0].rstrip('\uffff')
            lfn = []
            if name in ('.', '..'): continue
            require(name and not any(c in name for c in '/\\\x00:') and name not in ('.','..'), 'Unsafe FAT filename')
            path = parent + name
            require(path.casefold() not in seen, 'Duplicate FAT path')
            seen.add(path.casefold())
            if metadata is not None:
                metadata[path] = bytes([entry[11]]) + bytes(entry[13:26])
            cluster, = struct.unpack_from('<H', entry, 26)
            size, = struct.unpack_from('<I', entry, 28)
            require(struct.unpack_from('<H', entry, 20)[0] == 0, 'Unsupported cluster number')
            content = chain(cluster) if cluster else b''
            if attr & 16:
                require(cluster != 0, 'Invalid directory cluster')
                directories.append(path)
                walk(content, path + '/', depth + 1)
            else:
                require(size <= len(content), 'Truncated FAT file')
                files[path] = content[:size]
    root = (reserved + nfats * fatsz) * bps
    walk(data[root:root + roots * 32])
    # 拒绝丢失孤立簇。/ Refuse to silently discard allocated orphan clusters.
    for cluster in range(2, count + 2):
        at = cluster * 3 // 2 if bits == 12 else cluster * 2
        val, = struct.unpack_from('<H', fat, at)
        val = ((val >> 4) if cluster & 1 else (val & 0xfff)) if bits == 12 else val
        require(val == 0 or cluster in claimed, 'Orphan/bad FAT cluster; repair before migration')
    return files, sorted(directories)


def build_storage(files, dirs, size=NEW_STORAGE_SIZE, metadata=None):
    """Build FAT12 with exact UTF-16 LFN bytes; SDK supplies boot/WL geometry."""
    from wl_fatfsgen import WLFATFS
    fs = WLFATFS(size=size, sector_size=4096, long_names_enabled=True, volume_label='DPREADER', device_id=0x44505231)
    raw = fs.plain_fatfs.state.binary_image
    bps = 4096
    reserved = struct.unpack_from('<H', raw, 14)[0]
    nfats = raw[16]
    roots = struct.unpack_from('<H', raw, 17)[0]
    fatsz = struct.unpack_from('<H', raw, 22)[0]
    root_start = (reserved + nfats * fatsz) * bps
    data_start = root_start + ((roots * 32 + bps - 1) // bps) * bps
    cluster_size = raw[13] * bps
    count = (len(raw) - data_start) // cluster_size
    require(count < 4085, 'Only FAT12 output is supported')
    fat = bytearray(raw[reserved*bps:(reserved+fatsz)*bps])
    next_cluster = 2
    def allocate(content):
        nonlocal next_cluster
        if not content: return 0
        n = (len(content) + cluster_size - 1) // cluster_size
        require(next_cluster + n <= count + 2, 'Files do not fit resized storage; original flash untouched')
        first = next_cluster
        next_cluster += n
        for index in range(first, next_cluster):
            value = index + 1 if index + 1 < next_cluster else 0xfff
            at = index * 3 // 2
            old = struct.unpack_from('<H', fat, at)[0]
            struct.pack_into('<H', fat, at, (old & 0x000f) | (value << 4) if index & 1 else (old & 0xf000) | value)
        start = data_start + (first - 2) * cluster_size
        raw[start:start+n*cluster_size] = content.ljust(n * cluster_size, b'\0')
        return first
    def short_entry(alias, attr, cluster, size=0):
        item = bytearray(32); item[:11] = alias
        item[11] = attr
        struct.pack_into('<H', item, 24, 0x21)  # 1980-01-01
        struct.pack_into('<H', item, 26, cluster)
        struct.pack_into('<I', item, 28, size)
        return bytes(item)
    def named_entries(name, alias, attr, cluster, size):
        encoded = name.encode('utf-16le')
        require(0 < len(encoded) <= 510, 'FAT name exceeds 255 UTF-16 code units')
        chunks = (len(encoded) + 25) // 26
        encoded += b'\0\0' if len(encoded) % 26 else b''
        encoded = encoded.ljust(chunks * 26, b'\xff')
        checksum = 0
        for c in alias: checksum = (((checksum & 1) << 7) + (checksum >> 1) + c) & 255
        result = bytearray()
        for i in range(chunks, 0, -1):
            entry = bytearray(32); entry[0] = i | (0x40 if i == chunks else 0)
            entry[11] = 0x0f; entry[13] = checksum
            chunk = encoded[(i-1)*26:i*26]
            entry[1:11], entry[14:26], entry[28:32] = chunk[:10], chunk[10:22], chunk[22:]
            result.extend(entry)
        return result + short_entry(alias, attr, cluster, size)
    raw[root_start:data_start] = b'\0' * (data_start - root_start)
    tree = {'': []}
    for path in dirs:
        tree[path] = []
    for path in dirs + list(files):
        parent, _, name = path.rpartition('/')
        require(parent in tree, 'Missing parent directory')
        tree[parent].append((name, path, path in tree))
    def directory(path, parent_cluster=0):
        children = sorted(tree[path])
        slots = sum(1 + (len(name.encode('utf-16le')) + 25)//26 for name, _, _ in children)
        slots += 1 + (2 if path else 0)
        if path:
            first = allocate(b'\0' * (slots * 32))
            result = bytearray(short_entry(b'.          ', 16, first) + short_entry(b'..         ', 16, parent_cluster))
        else:
            require(slots <= roots, 'Root directory does not fit resized FAT')
            first = 0; result = bytearray()
        for index, (name, child, is_dir) in enumerate(children, 1):
            alias = f'F{index:07d}'.encode() + b'   '
            content = files.get(child, b'')
            cluster = directory(child, first) if is_dir else allocate(content)
            entries = named_entries(name, alias, 16 if is_dir else 32, cluster, 0 if is_dir else len(content))
            if metadata and child in metadata:
                original = metadata[child]
                entries[-32+11] = original[0]
                entries[-32+13:-32+26] = original[1:]
            result.extend(entries)
        start = data_start + (first - 2) * cluster_size if path else root_start
        raw[start:start+len(result)] = result
        return first
    directory('')
    for index in range(nfats):
        start = (reserved + index * fatsz) * bps
        raw[start:start+len(fat)] = fat
    fs.init_wl()
    return bytes(fs.fatfs_binary_image)


def rebuild_storage(data, directory):
    from wl_fatfsgen import WLFATFS
    directory = Path(directory)
    directory.mkdir()
    metadata = {}
    if data == b'\xff' * OLD_STORAGE_SIZE:
        files, dirs = {}, []
    else:
        files, dirs = extract_fat(logical_fat(data), metadata)
    for name in dirs:
        (directory / name).mkdir(parents=True, exist_ok=True)
    for name, content in files.items():
        path = directory / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
    result = build_storage(files, dirs, metadata=metadata)
    require(len(result) == NEW_STORAGE_SIZE, 'Incorrect rebuilt storage size')
    after_metadata = {}
    after, after_dirs = extract_fat(logical_fat(result), after_metadata)
    require(after == files and after_dirs == dirs and after_metadata == metadata, 'Rebuilt filesystem differs from original')
    return result, {name: hashlib.sha256(content).hexdigest() for name, content in files.items()}
