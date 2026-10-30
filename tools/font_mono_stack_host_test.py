#!/usr/bin/env python3
"""Regression for RC7's 16KiB local raster pool overflowing ESP task stacks."""
from pathlib import Path
import subprocess,tempfile,re,sys
from font_mono_host_test import FLAGS,SOURCES,COMP,ROOT,UNIT
font=Path(sys.argv[1]) if len(sys.argv)>1 else ROOT/'main/assets/builtin.ttf'
with tempfile.TemporaryDirectory() as directory:
 base=Path(directory);results={}
 for pool in (16384,2048):
  work=base/str(pool);work.mkdir();(work/'test.c').write_text(UNIT)
  flags=[*FLAGS,f'-DKIIKO_MONO_RENDER_POOL_SIZE={pool}']
  subprocess.run(['cc',*flags,'-fstack-usage','-c',str(COMP/'vendor/src/raster/raster.c'),'-o',str(work/'raster.o')],cwd=work,check=True)
  subprocess.run(['cc',*flags,str(work/'test.c'),str(ROOT/'main/font/ttf_mono.c'),*[str(COMP/'vendor/src'/s) for s in SOURCES if s!='raster/raster.c'],str(work/'raster.o'),'-lm','-o',str(work/'test')],cwd=work,check=True)
  out=subprocess.check_output([str(work/'test'),str(font)],text=True)
  digests=re.findall(r'MONO_DIGEST ([0-9a-f]+)',out);assert len(digests)==3
  frames=[]
  for p in work.rglob('*.su'):
   for line in p.read_text().splitlines():
    fields=line.split('\t')
    if len(fields)>1 and re.search(r':(?:Render_Glyph|ft_black_render)(?:\.|$)',fields[0]):frames.append((fields[0],int(fields[1])))
  assert frames,'compiler raster stack evidence missing: '+str([(p.name,p.read_text()[:300]) for p in work.rglob('*.su')])
  results[pool]=(digests,max(n for _,n in frames))
  print(f'pool{pool}: max raster frame={results[pool][1]} bytes, digest={digests[0]}')
 assert results[16384][0]==results[2048][0],'smaller pool changed rendered ink'
 assert results[16384][1]>=16384
 assert results[2048][1]<=4096
 print('PASS actual mono pixels/bounds unchanged across2774renders x3cycles; raster frame bounded below4KiB instead of overflowing16KiB task')
