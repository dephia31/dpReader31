#!/usr/bin/env python3
"""Generate a self-authored mini dictionary and text for local reader acceptance.
SPDX-License-Identifier: Apache-2.0
"""
from pathlib import Path
import argparse
import struct
import gzip
import zlib

def pack_dictzip(data,chunk=128):
    compressor=zlib.compressobj(6,zlib.DEFLATED,-15)
    parts=[compressor.compress(data[i:i+chunk])+compressor.flush(zlib.Z_FULL_FLUSH) for i in range(0,len(data),chunk)]
    ra=struct.pack('<HHH',1,chunk,len(parts))+struct.pack('<'+'H'*len(parts),*(len(p) for p in parts))
    extra=b'RA'+struct.pack('<H',len(ra))+ra
    header=b'\x1f\x8b\x08\x04'+b'\0'*5+b'\xff'+struct.pack('<H',len(extra))+extra
    return header+b''.join(parts)+compressor.flush(zlib.Z_FINISH)+struct.pack('<II',zlib.crc32(data),len(data))

def generate(output,compressed=False):
    output=Path(output);dictionary=output/'dict/dpReader-demo';dictionary.mkdir(parents=True,exist_ok=True)
    entries={
      'apple':'quả táo; ví dụ: an apple — một quả táo.',
      'banana':'quả chuối.',
      'book':'quyển sách; đọc một quyển sách.',
      'carrot':'củ cà rốt.',
      'dictionary':'từ điển; bộ dữ liệu để tra nghĩa của từ.',
      'highlight':'đánh dấu một đoạn văn khi đọc.',
      'note':'ghi chú; lời nhắc do người đọc lưu lại.',
      'tiếng Việt':'ngôn ngữ dùng trong ví dụ kiểm tra chọn cụm từ.',
      'water':'nước.',
    }
    keys=sorted(entries,key=lambda s:s.encode());idx=bytearray();data=bytearray()
    for word in keys:
        definition=entries[word].encode();idx+=word.encode()+b'\0'+struct.pack('>II',len(data),len(definition));data+=definition
    if compressed:
        (dictionary/'dpReader-demo.idx.gz').write_bytes(gzip.compress(bytes(idx),mtime=0))
        (dictionary/'dpReader-demo.dict.dz').write_bytes(pack_dictzip(bytes(data)))
    else:
        (dictionary/'dpReader-demo.idx').write_bytes(idx);(dictionary/'dpReader-demo.dict').write_bytes(data)
    (dictionary/'dpReader-demo.syn').write_bytes(b'apples\0'+struct.pack('>I',keys.index('apple')))
    (dictionary/'dpReader-demo.ifo').write_text("StarDict's dict ifo file\nversion=3.0.0\nbookname=dpReader mini demo (9 words only)\nwordcount="+str(len(keys))+"\nidxfilesize="+str(len(idx))+"\nsametypesequence=m\n")
    (dictionary/'README.txt').write_text('Mini fixture tự viết, Apache-2.0; chỉ kiểm tra 9 từ, không phải bộ từ điển đầy đủ.\n')
    text='''dpReader — THỬ CHỌN ĐOẠN VÀ TRA TỪ OFFLINE

apple banana book carrot dictionary highlight note water apples.

Giữ từ apple rồi kéo sang banana, thả tay để hiện menu để chọn cả cụm. Chạm Tô sáng để lưu dấu gạch chân. Chọn một đoạn khác, chạm Ghi chú, nhập lời nhắc rồi bấm Xong để tự lưu.

Chạm Đóng, đổi cỡ chữ và mở lại sách: dấu đã lưu cần nằm đúng đoạn. Giữ một từ, chọn Các ghi chú để đọc lại, sửa hoặc xóa. Bản thử giới hạn danh sách trong chương đang đọc.

Để kiểm tra từ điển mẫu, chọn từng từ apple, book hoặc water rồi Tra từ. Chọn apples để kiểm tra từ đồng nghĩa. Giữ chữ tiếng rồi kéo hết cụm tiếng Việt để tra cụm có dấu.

Đây là đoạn để thử bố cục với chữ tiếng Việt: Một người đọc ngồi bên cửa sổ, mở quyển sách và ghi lại điều mình muốn nhớ. Dấu gạch chân cần bám đúng chữ sau khi đổi font hoặc cỡ chữ.

'''
    (output/'TEST_HIGHLIGHT.txt').write_text(text*4)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('output',type=Path);parser.add_argument('--compressed',action='store_true')
    args=parser.parse_args();generate(args.output,args.compressed)
