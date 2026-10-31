"""Production signed spacing ranges, paragraph gap arithmetic and reader choice ladders."""
from pathlib import Path
import re,subprocess,tempfile
from home_cover_cache_host_test import function
r=Path(__file__).resolve().parents[1]
s=(r/'main/apps/app_book.c').read_text()
u=r'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#define APP_BOOK_PARAGRAPH_COUNT 12
static unsigned s_line_percent;static int s_paragraph_percent,s_word_spacing_px;
'''
for n in ['book_layout_set_spacing','book_layout_set_word_spacing','line_height_for','gap_for']:
 u+=function(n,r/'main/book/book_layout.c')+'\n'
for n in ['app_settings_book_paragraph_value','app_settings_book_paragraph_level']:
 u+=function(n,r/'main/settings.c')+'\n'
u+=r'''
int main(void){
 const int values[]={-10,-5,0,5,10,15,20,25,30,40,50,75};
 for(unsigned i=0;i<12;i++){
  uint8_t encoded=app_settings_book_paragraph_value(i);assert((int8_t)encoded==values[i]);assert(app_settings_book_paragraph_level(encoded)==i);
  book_layout_set_spacing(130,encoded);assert(gap_for(100,false)==values[i]);assert(gap_for(100,true)>=0);
 }
 const unsigned lines[]={236,246,0,100,110,120,130};const int heights[]={124,127,130,100,110,120,130};
 for(unsigned i=0;i<7;i++){book_layout_set_spacing(lines[i],0);assert(line_height_for(100)==heights[i]&&line_height_for(100)>0);}
 for(int i=-3;i<=20;i++){book_layout_set_word_spacing(i);assert(s_word_spacing_px==i);}
 book_layout_set_word_spacing(-4);assert(s_word_spacing_px==0);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'t.c').write_text(u);subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'t.c'),'-o',str(p/'t')],check=True);subprocess.run([str(p/'t')],check=True)
assert 'if(kind==READ_PICK_SIZE)return 26;' in s and '40+2*index' in s
assert 's_px>=40&&s_px<=90&&!(s_px%2)?(s_px-40)/2:-1' in s
assert re.search(r's_line_choices\[\]=\{236,246,0,100,110,120,130\}',s)
assert 'if(kind==READ_PICK_WORD)return 24;' in s and 'apply_reader_word_spacing(ctx,index-3)' in s
print('PASS: even40..90 size ladder, signed paragraph -10..75 encoding/levels/gap, negative line leading arithmetic and seven choices, word -3..20 bounds')
