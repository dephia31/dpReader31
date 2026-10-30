#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""RC6 crash regression: force auto-hinting and compare original/small-inline outputs."""
from pathlib import Path
import tempfile,subprocess,re,sys
from font_mono_host_test import FLAGS,SOURCES,COMP,ROOT
# Importing font_aa_host_test would run its CLI; extract its literal test unit instead.
import ast
module=ast.parse((ROOT/'tools/font_aa_host_test.py').read_text())
unit=next(ast.literal_eval(n.value) for n in module.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='UNIT' for t in n.targets))
unit=unit.replace('#include "ttf_mono.h"','#include "'+str(ROOT/'main/font/ttf_mono.c')+'"')
unit=unit.replace('assert(face);','assert(face);face->force_auto=true;')
unit=unit.replace('unsigned black=0,gray=0,glyphs=0;','uint64_t digest=1469598103934665603ULL;unsigned black=0,gray=0,glyphs=0;')
unit=unit.replace('assert(b.pitch>=b.width);','assert(b.pitch>=b.width);int metrics[]={b.width,b.height,b.left,b.top};for(int i=0;i<4;++i)digest=(digest^(uint32_t)metrics[i])*1099511628211ULL;')
unit=unit.replace('unsigned a=b.pixels[y*b.pitch+x];','unsigned a=b.pixels[y*b.pitch+x];digest=(digest^a)*1099511628211ULL;')
unit=unit.replace('++glyphs;','''ttf_mono_bitmap_t mono;assert(ttf_mono_render_stroke(face,cp,stbtt_ScaleForPixelHeight(&info,px),-1,0,&mono));
 for(int y=0;y<mono.height;++y)for(int x=0;x<mono.width;++x)digest=(digest^((mono.pixels[y*mono.pitch+x/8]>>(7-(x&7)))&1))*1099511628211ULL;
 ++glyphs;''')
unit=unit.replace('assert(black&&gray);','printf("AUTOFIT_DIGEST %016llx\\n",(unsigned long long)digest);assert(black&&gray);')
fonts=[ROOT/'main/assets/builtin.ttf',*(Path(x) for x in sys.argv[1:])]
with tempfile.TemporaryDirectory() as d:
 base=Path(d);results={}
 for reference in (True,False):
  work=base/('reference' if reference else 'fixed');work.mkdir();(work/'test.c').write_text(unit)
  flags=[*FLAGS,*(['-DKIIKO_AUTOFIT_STACK_REFERENCE=1'] if reference else [])]
  subprocess.run(['cc',*flags,'-fstack-usage','-c',str(COMP/'kiiko_autofit.c'),'-o',str(work/'autofit.o')],check=True,cwd=work)
  subprocess.run(['cc',*flags,str(work/'test.c'),*[str(COMP/'vendor/src'/s) for s in SOURCES if s!='../../kiiko_autofit.c'],str(work/'autofit.o'),'-lm','-o',str(work/'test')],check=True)
  frames=[]
  for p in work.glob('*.su'):
   for line in p.read_text().splitlines():
    fields=line.split('\t')
    if len(fields)>1 and fields[1].isdigit():frames.append((fields[0].split(':')[-1],int(fields[1])))
  heavy={name:size for name,size in frames if name in ('af_autofitter_load_glyph','af_latin_metrics_init_widths','af_cjk_metrics_init_widths')}
  assert len(heavy)>=2,heavy  # The host compiler can inline Latin metrics into its caller.
  assert max(heavy.values())>(12000 if reference else 0)
  if not reference:assert max(heavy.values())<6144,heavy
  digests=[]
  for font in fonts:
   out=subprocess.check_output([str(work/'test'),str(font)],text=True)
   match=re.search(r'AUTOFIT_DIGEST ([0-9a-f]+)',out);assert match,out
   digests.append(match[1]);print(work.name,font.name,match[1],flush=True)
  results[reference]=digests;print(work.name,'stack frames',heavy,flush=True)
 assert results[True]==results[False],'inline capacity change altered actual hinted pixels'
 print('PASS forced auto-hint mono+gray identical, builtin+supplied fonts18-90px/3cycles, bounded allocations/cleanup, fixed host stack frames<6KiB',flush=True)
