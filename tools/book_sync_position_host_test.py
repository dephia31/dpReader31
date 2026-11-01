#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
from pathlib import Path
import subprocess,tempfile,zipfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as d:
 p=Path(d);book=p/'long.epub'
 with zipfile.ZipFile(book,'w',zipfile.ZIP_DEFLATED) as z:
  z.writestr('META-INF/container.xml','<container><rootfiles><rootfile full-path="book.opf"/></rootfiles></container>')
  z.writestr('styles.css','.unused {white-space: pre-wrap;}')
  z.writestr('book.opf','<package><manifest><item id="a" href="a.xhtml" media-type="application/xhtml+xml"/><item id="b" href="b.xhtml" media-type="application/xhtml+xml"/></manifest><spine><itemref idref="a"/><itemref idref="b"/></spine></package>')
  for name in ['a','b']:
   z.writestr(name+'.xhtml','<html><head><link rel="stylesheet" href="styles.css"/></head><body><div>'+''.join(f'<p>P{i:04d} Tiếng Việt có dấu &amp; ký tự &#x1F600;.</p>' for i in range(1,1001))+'</div></body></html>')
 common=['-std=c11','-D_DEFAULT_SOURCE','-Wall','-Wextra','-Werror','-fsanitize=undefined','-Itools/book_epub_stubs','-Itools/zip_host_stubs','-Imain/book']
 sources=['tools/book_sync_position_host_test.c','main/book/book_sync_position.c','main/book/book_xpointer.c','main/book/book_epub.c','main/book/book_index_cache.c','main/book/book_image_header.c','main/book/zip_reader.c','main/book/html_text.c']
 subprocess.run(['cc',*common,*sources,'-lz','-o',str(p/'test')],cwd=root,check=True)
 subprocess.run([str(p/'test'),str(book)],cwd=root,check=True)
 subprocess.run(['cc',*common,'tools/book_xpointer_host_test.c','main/book/book_xpointer.c','main/book/html_text.c','-o',str(p/'map')],cwd=root,check=True)
 subprocess.run([str(p/'map')],cwd=root,check=True)
