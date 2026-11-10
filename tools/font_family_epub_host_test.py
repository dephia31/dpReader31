"""Exercise styled EPUB runs through the real ZIP/CSS/chapter entry point."""
from pathlib import Path
import subprocess
import tempfile
import zipfile

root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='font-family-epub-') as temp:
    work=Path(temp)
    book=work/'family.epub'
    with zipfile.ZipFile(book,'w',zipfile.ZIP_DEFLATED) as archive:
        archive.writestr('mimetype','application/epub+zip')
        archive.writestr('META-INF/container.xml','<container><rootfiles><rootfile full-path="book.opf"/></rootfiles></container>')
        archive.writestr('book.opf','<package><manifest><item id="c" href="c.xhtml" media-type="application/xhtml+xml"/></manifest><spine><itemref idref="c"/></spine></package>')
        archive.writestr('c.xhtml','<html><head><link rel="stylesheet" type="text/css" href="style.css"/></head><body><p>A<strong>B<em>C</em>D</strong><i>E</i>F</p></body></html>')
        archive.writestr('style.css','body{font-family:Family}'+''.join(
            '@font-face{font-family:Family;src:url('+name+'.ttf);font-weight:'+weight+';font-style:'+style+'}'
            for name,weight,style in [('R','400','normal'),('B','700','normal'),('I','400','italic'),('X','700','italic')]))
        for name in ('R','B','I','X'):
            archive.writestr(name+'.ttf',name.encode()*128)
    (work/'ttf_font.h').write_text('''#include <stdint.h>
#include <stddef.h>
void ttf_font_close_embedded(void);
int ttf_font_open_mem(uint8_t*,size_t,const char*);
int ttf_font_family_style(int,unsigned);
''')
    (work/'test.c').write_text('''
#include "book_epub.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int ttf_font_open_mem(uint8_t *data,size_t n,const char *label){
 (void)label;assert(n==128);int slot=data[0]=='R'?1:data[0]=='B'?2:data[0]=='I'?3:4;free(data);return slot;
}
void ttf_font_close_embedded(void){}
static unsigned generation;
int ttf_font_family_style(int base,unsigned style){assert(base==0);return 8+style+generation;}
int main(int argc,char **argv){
 assert(argc==2);book_epub_t *book=NULL;
 assert(book_epub_open(argv[1],&book)==ESP_OK);
 for(unsigned user=0;user<2;++user){
  book_epub_set_fonts_enabled(book,!user);html_text_t t={0};
  assert(book_epub_load_target(book,0,NULL,SIZE_MAX,NULL,&t)==ESP_OK);
  assert(!strcmp(t.utf8,"ABCDEF")&&t.run_count==6);
  unsigned expected[]={1,2,4,2,3,1},fallback[]={0,9,11,9,10,0};
  for(unsigned i=0;i<6;++i)assert(t.runs[i].offset==i&&t.runs[i].len==1&&t.runs[i].slot==(user?fallback[i]:expected[i]));
  if(user){
   generation=4;book_epub_prepare_font_runs(book,t.runs,t.run_count);
   for(unsigned i=0;i<6;++i)assert(t.runs[i].slot==(fallback[i]?fallback[i]+4:0));
  }
  html_text_free(&t);
 }
 book_epub_close(book);puts("PASS real EPUB: regular/bold/italic/bold-italic @font-face variants and user-font fallback preserve runs");
}
''')
    source=root/'firmware/shared/main/book'
    subprocess.run(['cc','-std=c11','-D_DEFAULT_SOURCE','-Wall','-Wextra','-Werror','-fsanitize=undefined',
                    '-I'+str(work),'-I'+str(root/'tools/book_epub_stubs'),'-I'+str(root/'tools/zip_host_stubs'),'-I'+str(source),
                    str(work/'test.c'),*[str(source/name) for name in ('book_epub.c','zip_reader.c','html_text.c','book_index_cache.c','book_image_header.c')],'-lz','-o',str(work/'test')],check=True)
    subprocess.run([str(work/'test'),str(book)],check=True)
