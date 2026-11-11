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
#define APP_BOOK_PARAGRAPH_COUNT 18
static unsigned s_line_percent;static int s_line_adjust_px,s_paragraph_gap_px,s_word_spacing_px;
'''
for n in ['book_layout_set_spacing','book_layout_set_word_spacing','line_height_for','gap_for']:
 u+=function(n,r/'main/book/book_layout.c')+'\n'
for n in ['app_settings_book_paragraph_value','app_settings_book_paragraph_level']:
 u+=function(n,r/'main/settings.c')+'\n'
u+=r'''
int main(void){
 const int values[]={-70,-60,-50,-40,-30,-20,-10,-5,0,5,10,15,20,25,30,40,50,75};
 for(unsigned i=0;i<APP_BOOK_PARAGRAPH_COUNT;i++){
  uint8_t encoded=app_settings_book_paragraph_value(i);assert((int8_t)encoded==values[i]);assert(app_settings_book_paragraph_level(encoded)==i);
  book_layout_set_spacing(130,encoded);assert(gap_for(100,false)==values[i]*40/100);assert(gap_for(48,false)==gap_for(100,false));assert(gap_for(100,true)>=0);
 }
 const unsigned lines[]={186,196,206,216,226,236,246,0,10,20,30};const int heights[]={24,28,32,36,40,44,48,52,56,60,64};
 for(unsigned i=0;i<sizeof(lines)/sizeof(lines[0]);i++){book_layout_set_spacing(lines[i],0);assert(line_height_for(40)==heights[i]&&line_height_for(48)==heights[i]+10);}
 for(int i=-10;i<=20;i++){book_layout_set_word_spacing(i);assert(s_word_spacing_px==i);}
 book_layout_set_word_spacing(-11);assert(s_word_spacing_px==0);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'t.c').write_text(u);subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'t.c'),'-o',str(p/'t')],check=True);subprocess.run([str(p/'t')],check=True)
assert 'if(kind==READ_PICK_SIZE)return 26;' in s and '40+2*index' in s
assert 's_px>=40&&s_px<=90&&!(s_px%2)?(s_px-40)/2:-1' in s
assert re.search(r's_line_choices\[\]=\{186,196,206,216,226,236,246,0,10,20,30\}',s)
assert 'if(kind==READ_PICK_WORD)return 31;' in s and 'apply_reader_word_spacing(ctx,index-10)' in s
print('PASS: even40..90 size ladder, signed paragraph -70..75 encoding/levels/gap, fixed 4 px steps at 40 px and 48 px type, word -10..20 bounds')
