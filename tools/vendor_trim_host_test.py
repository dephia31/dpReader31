"""Compile the actual vendor initialization beside production; compare every LUT byte."""
from pathlib import Path
import re,subprocess,tempfile,hashlib,json
r=Path(__file__).resolve().parents[1]
v=r.parent/'validation/metalio-2970d347-review/official/main/boards/metalio-e-ink4-plus/display/waveforms'
files=[v/n for n in ['my_waveform.c','my_waveform_trim.c','my_waveform.h','my_waveform_trim.h','my_waveform_du.h','my_waveform_gc.h','my_waveform_gl.h']]
names=set()
for p in files:names.update(re.findall(r'\bmy_[a-zA-Z0-9_]+\b',p.read_text()))
defines=['-D'+n+'=vendor_'+n for n in sorted(names)]+['-DMY_WAVEFORM=VENDOR_WAVEFORM']
unit=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "e0470_epaper_waveform.h"
extern const EpdWaveform VENDOR_WAVEFORM;
void vendor_my_waveform_init(void);
int main(void){
 e0470_waveform_init();vendor_my_waveform_init();
 for(int m=1;m<=5;++m){
  const EpdWaveformPhases *a=e0470_waveform_phases(&E0470_WAVEFORM,m);
  const EpdWaveformPhases *b=e0470_waveform_phases(&VENDOR_WAVEFORM,m);
  assert((a==NULL)==(b==NULL));if(!a)continue;
  assert(a->phases==b->phases&&!a->phase_times&&!b->phase_times);
  assert(!memcmp(a->luts,b->luts,(size_t)a->phases*16*4));
 }
 assert(e0470_waveform_phases(&E0470_FULL_WAVEFORM,2)->phases==48);
 assert(e0470_waveform_phases(&E0470_FULL_WAVEFORM,5)->phases==48);
 puts("PASS: production DU20/GC36/GL37 byte-exact to actual vendor my_waveform_init; full48 retained");
}
'''
with tempfile.TemporaryDirectory() as d:
 d=Path(d);(d/'esp_log.h').write_text('#define ESP_LOGD(...) ((void)0)\n#define ESP_LOGI(...) ((void)0)\n')
 flags=['cc','-std=c11','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-fsanitize=address,undefined',
  '-I'+str(d),'-I'+str(v),'-I'+str(r/'tools/waveform_host_stubs'),'-I'+str(r/'components/epdiy/include'),'-I'+str(r/'components/epdiy/src'),
  '-I'+str(r/'components/e0470_epaper_waveform/include'),'-I'+str(r/'components/e0470_epaper_waveform/metalio')]
 objects=[]
 for i,p in enumerate(files[:2]):
  obj=d/(str(i)+'.o');subprocess.run(flags+defines+['-c',str(p),'-o',str(obj)],check=True);objects.append(str(obj))
 (d/'test.c').write_text(unit)
 subprocess.run(flags+[str(d/'test.c'),str(r/'components/e0470_epaper_waveform/e0470_epaper_waveform.c'),str(r/'components/e0470_epaper_waveform/e0470_waveform_trim.c'),*objects,'-o',str(d/'test')],check=True)
 subprocess.run([str(d/'test')],check=True)
 print(json.dumps({p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in files},indent=2))
