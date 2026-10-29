"""中文：实际KoSync散列在PSRAM不足时关闭文件，不回退内部RAM。/ English: Actual KoSync hashing closes files on PSRAM OOM without internal fallback."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'main/book/kosync_client.c').read_text();start=s.index('typedef struct {');end=s.index('/* ---- Quản lý kết nối',start)
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_ARG 1
#define ESP_ERR_NOT_FOUND 2
#define ESP_ERR_NO_MEM 3
#define MALLOC_CAP_SPIRAM 4
#define MALLOC_CAP_8BIT 8
static bool fail;static int closes,allocs;
static void *heap_caps_malloc(size_t n,int caps){assert(n==1024&&caps==12);allocs++;return fail?NULL:malloc(n);}
static int checked_close(FILE*f){closes++;return fclose(f);}
#define fclose checked_close
'''+s[start:end]+r'''
int main(int argc,char**argv){
 assert(argc==2);char hash[33];kosync_md5_string("abc",hash);assert(!strcmp(hash,"900150983cd24fb0d6963f7d28e17f72"));
 fail=true;assert(kosync_compute_doc_md5(argv[1],hash)==ESP_ERR_NO_MEM);assert(closes==1&&allocs==1&&!hash[0]);
 fail=false;assert(kosync_compute_doc_md5(argv[1],hash)==ESP_OK);assert(closes==2&&allocs==2&&strlen(hash)==32);
 puts("PASS: actual KoSync MD5 uses PSRAM; OOM closes file and returns error; normal retry succeeds");
}
'''
assert 'if (!resp) resp = malloc' not in s and 'if (!buf) buf = malloc' not in s
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(unit);(p/'book.txt').write_text('abc')
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test'),str(p/'book.txt')],check=True)
