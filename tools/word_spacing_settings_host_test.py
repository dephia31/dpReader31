"""Exercise production word spacing NVS transaction and sealed backup block."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
r=Path(__file__).resolve().parents[1]
u=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
typedef int nvs_handle_t;typedef int esp_err_t;
#define ESP_OK 0
#define NVS_NS "test"
#define NVS_READWRITE 1
static int8_t s_book_word_spacing;static uint8_t stored,pending;static int fault;
static int nvs_open(const char*n,int m,int*h){(void)n;(void)m;*h=1;return fault==1?-1:0;}
static int nvs_set_u8(int h,const char*n,uint8_t p){(void)h;(void)n;pending=p;return fault==2?-1:0;}
static int nvs_commit(int h){(void)h;if(fault==3)return -1;stored=pending;return 0;}
static void nvs_close(int h){(void)h;}
typedef struct {uint8_t encoded,checksum[4];} settings_backup_indent_adjust_t;
'''
for n in ['app_settings_book_word_spacing','app_settings_set_book_word_spacing','backup_indent_adjust_checksum','backup_indent_adjust_valid','backup_word_spacing_valid']:u+=function(n,r/'main/settings.c')+'\n'
u+=r'''
int main(void){
 for(int f=1;f<=3;f++){s_book_word_spacing=stored=0;fault=f;assert(!app_settings_set_book_word_spacing(12));assert(!stored&&!app_settings_book_word_spacing());}
 fault=0;for(int i=-128;i<=127;i++){s_book_word_spacing=stored=0;assert(app_settings_set_book_word_spacing(i)==(i>=-3&&i<=20));assert(stored==(uint8_t)(i>=-3&&i<=20?i:0));}
 settings_backup_indent_adjust_t b={.encoded=12};uint32_t sum=backup_indent_adjust_checksum(&b);for(unsigned i=0;i<4;i++)b.checksum[i]=sum>>(8*i);assert(backup_indent_adjust_valid(&b));b.encoded^=1;assert(!backup_indent_adjust_valid(&b));
 b.encoded=(uint8_t)-3;sum=backup_indent_adjust_checksum(&b);for(unsigned i=0;i<4;i++)b.checksum[i]=sum>>(8*i);assert(backup_word_spacing_valid(&b));b.checksum[0]^=1;assert(!backup_word_spacing_valid(&b));return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'t.c').write_text(u);subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'t.c'),'-o',str(p/'t')],check=True);subprocess.run([str(p/'t')],check=True)
print('PASS: word spacing NVS all faults/bounds and backup payload seal')
