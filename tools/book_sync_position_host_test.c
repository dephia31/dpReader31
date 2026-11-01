/* SPDX-License-Identifier: Apache-2.0 */
#include "book_epub.h"
#include "book_sync_position.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int ttf_font_open_mem(unsigned char *p,size_t n,const char*l){(void)n;(void)l;free(p);assert(!"sync conversion must never load embedded fonts");return -1;}
void ttf_font_close_embedded(void){assert(!"sync conversion must never close the live reader's fonts");}
int main(int argc,char **argv){assert(argc==2);book_epub_t*b=NULL;assert(book_epub_open(argv[1],&b)==ESP_OK);assert(book_epub_chapter_count(b)==2);
 for(unsigned ch=0;ch<2;ch++){
  html_text_t text={0};assert(book_epub_load(b,ch,&text)==ESP_OK);unsigned tests=0;
  for(size_t at=0;at<text.len;at++){
   unsigned char c=text.utf8[at];if((c&0xc0)==0x80||c=='\n'||c==' '||at%97)continue;
   char pointer[BOOK_XPOINTER_CAP];double ratio=-1;uint16_t back_ch=99;uint32_t back=UINT32_MAX;bool exact=false;
   assert(book_sync_export(argv[1],ch,at,pointer,sizeof(pointer),&ratio,NULL)==ESP_OK);assert(ratio>=0&&ratio<=1);
   assert(strstr(pointer,"/text()[")!=NULL);assert(book_sync_import(argv[1],pointer,ratio,&back_ch,&back,&exact)==ESP_OK);
   if(!exact||back_ch!=ch||back!=at){fprintf(stderr,"roundtrip ch%u at%zu gotch%u off%u exact%d %s\n",ch,at,back_ch,back,exact,pointer);abort();}tests++;
  }
  assert(tests>50);html_text_free(&text);
 }
 uint16_t ch;uint32_t off;bool exact;
 assert(book_sync_import(argv[1],"bad-pointer",.75,&ch,&off,&exact)==ESP_OK&&!exact&&ch==1&&off>0);
 assert(book_sync_import(argv[1],"/body/DocFragment[2]/body/div[1]/p[500]/text()[1].6",.01,&ch,&off,&exact)==ESP_OK&&exact&&ch==1&&off>5000);
 book_epub_close(b);puts("Actual EPUB ZIP -> normalized UTF8 -> XPointer -> exact UTF8 roundtrips across long chapters PASS; approximate fallback explicit; no shared font calls");
}
