"""Exercise bounded production ZIP prefetch, CRC, cancellation, pressure and suspend coherence."""
from pathlib import Path
import io,os,subprocess,tempfile,zipfile
r=Path(__file__).resolve().parents[1]
unit=r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zip_reader.c"
static void warm(zip_reader_t *z,int index,int expected){
 int result=0,steps=0;
 while(!result){size_t before=z->prefetch_read;result=zip_prefetch_step(z,index);
  assert(++steps<=129);if(z->prefetch)assert(z->prefetch_read<=before+4096&&z->prefetch_size<=512u*1024u);}
 assert(result==expected);
}
int main(int argc,char **argv){
 assert(argc==4);zip_reader_t *z=NULL;assert(zip_open(argv[1],&z)==ESP_OK);
 int index=zip_find(z,"chapter");assert(index>=0);
 int expected=atoi(argv[3]);size_t bytes=zip_entry_size(z,index);uint8_t *out=malloc(bytes?bytes:1);assert(out);
 assert(zip_prefetch_step(NULL,index)==-1&&zip_prefetch_step(z,-1)==-1);
 int first=zip_prefetch_step(z,index);assert(first>=-1&&first<=1);zip_prefetch_clear(z);assert(!z->prefetch);
 warm(z,index,expected<0?-1:1);
 esp_err_t error=zip_extract(z,index,out,bytes);
 if(expected==2)assert(error!=ESP_OK);
 else {
  assert(error==ESP_OK);FILE *f=fopen(argv[2],"rb");assert(f);
  for(size_t i=0;i<bytes;++i)assert(out[i]==fgetc(f));assert(fgetc(f)==EOF);fclose(f);
  if(expected>0){
   assert(z->prefetch_ready);assert(zip_suspend_io(z)==ESP_OK&&!z->prefetch);
   assert(zip_resume_io(z,argv[1])==ESP_OK);warm(z,index,1);
   FILE *change=fopen(argv[1],"ab");assert(change);assert(fputc(' ',change)!=EOF);assert(!fclose(change));
   assert(zip_extract(z,index,out,bytes)==ESP_OK&&!z->prefetch);
  }
 }
 zip_prefetch_clear(z);zip_close(z);free(out);
}
'''
with tempfile.TemporaryDirectory() as d:
 d=Path(d);(d/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined',
  '-I'+str(r/'tools/zip_host_stubs'),'-I'+str(r/'main/book'),str(d/'test.c'),'-lz','-o',str(d/'test')],check=True)
 cases=0
 for mode in [zipfile.ZIP_STORED,zipfile.ZIP_DEFLATED]:
  for data in [b'',b'book',os.urandom(128*1024),b'A'*(768*1024),os.urandom(600*1024)]:
   bio=io.BytesIO()
   with zipfile.ZipFile(bio,'w',compression=mode) as z:z.writestr('chapter',data)
   raw=bio.getvalue();(d/'book.zip').write_bytes(raw);(d/'expected').write_bytes(data)
   expected=-1 if len(raw)>512*1024 else 1
   subprocess.run([str(d/'test'),str(d/'book.zip'),str(d/'expected'),str(expected)],check=True);cases+=1
 # A prefetched CRC-invalid stored body must still fail extraction.
 bio=io.BytesIO()
 with zipfile.ZipFile(bio,'w',compression=zipfile.ZIP_STORED) as z:z.writestr('chapter',b'CRC must still be checked')
 raw=bytearray(bio.getvalue());raw[30+len('chapter')]^=1;(d/'book.zip').write_bytes(raw)
 subprocess.run([str(d/'test'),str(d/'book.zip'),str(d/'expected'),'2'],check=True);cases+=1
 print('PASS:',cases,'stored/deflated, 4KiB steps, 512KiB cap, partial cancel, large fallback, suspend/remount, changed-source invalidation and CRC failure cases')
