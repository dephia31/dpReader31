#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""Validate EPUB3 cover extraction/ownership against ZIP bytes; optionally use a local EPUB."""
from pathlib import Path
import argparse,hashlib,subprocess,tempfile,zipfile,xml.etree.ElementTree as ET,posixpath
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--epub',type=Path);args=p.parse_args()
unit=r'''
#include "book_epub.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int ttf_font_open_mem(uint8_t *data,size_t n,const char *label){(void)n;(void)label;free(data);return -1;}
void ttf_font_close_embedded(void){}
int main(int argc,char **argv){
 assert(argc==4);FILE *f=fopen(argv[2],"rb");assert(f);fseek(f,0,SEEK_END);size_t size=ftell(f);rewind(f);
 uint8_t *expected=malloc(size);assert(expected&&fread(expected,1,size,f)==size);fclose(f);
 for(int iteration=0;iteration<4;++iteration){
  uint8_t *image=NULL;size_t n=0;bool png=false;
  assert(book_epub_cover_bounded(argv[1],&image,&n,&png,size-1)!=ESP_OK&&!image&&!n);
  assert(book_epub_cover_bounded(argv[1],&image,&n,&png,size)==ESP_OK);
  assert(image&&n==size&&!memcmp(image,expected,size)&&png==(!strcmp(argv[3],"png")));free(image);
 }
 char title[256],author[128];assert(book_epub_metadata(argv[1],title,sizeof(title),author,sizeof(author))==ESP_OK&&*title);
 free(expected);puts("PASS: real EPUB cover extraction matches source ZIP bytes; exact budget, repeat ownership and metadata validated under ASan/UBSan");
}
'''
with tempfile.TemporaryDirectory(prefix='ticket-cover-epub-') as folder:
 tmp=Path(folder);epub=args.epub
 if epub is None:
  epub=tmp/'fixture.epub'
  with zipfile.ZipFile(epub,'w',zipfile.ZIP_DEFLATED) as z:
   z.writestr('META-INF/container.xml','<container><rootfiles><rootfile full-path="OEBPS/package.opf"/></rootfiles></container>')
   z.writestr('OEBPS/package.opf','<package><metadata><title>Ticket cover regression</title><creator>Reader</creator></metadata><manifest><item id="cover-image" href="images/cover.jpg" media-type="image/jpeg" properties="cover-image"/></manifest><spine/></package>')
   z.writestr('OEBPS/images/cover.jpg',bytes(range(256))*8)
 with zipfile.ZipFile(epub) as z:
  container=ET.fromstring(z.read('META-INF/container.xml'))
  opf=next(x.attrib['full-path'] for x in container.iter() if x.tag.split('}')[-1]=='rootfile')
  doc=ET.fromstring(z.read(opf));cover=next(x for x in doc.iter() if x.tag.split('}')[-1]=='item' and 'cover-image' in x.attrib.get('properties','').split())
  data=z.read(posixpath.normpath(posixpath.join(posixpath.dirname(opf),cover.attrib['href'])))
 (tmp/'expected').write_bytes(data);(tmp/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined','-I'+str(ROOT/'tools/book_epub_stubs'),'-I'+str(ROOT/'tools/zip_host_stubs'),'-I'+str(ROOT/'main/book'),str(tmp/'test.c')]+[str(ROOT/'main/book'/n) for n in ('book_epub.c','zip_reader.c','html_text.c','book_index_cache.c','book_image_header.c')]+['-lz','-o',str(tmp/'test')],check=True)
 subprocess.run([str(tmp/'test'),str(epub),str(tmp/'expected'),'png' if cover.attrib['media-type']=='image/png' else 'jpeg'],check=True)
 print('cover_bytes='+str(len(data))+' sha256='+hashlib.sha256(data).hexdigest())
