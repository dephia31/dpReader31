#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Generate a small-inline-buffer autofit header; upstream dynamic growth is retained."""
from pathlib import Path
import re,sys
CAPACITIES={'AF_SEGMENTS_EMBEDDED':4,'AF_EDGES_EMBEDDED':4,'AF_POINTS_EMBEDDED':8,'AF_CONTOURS_EMBEDDED':2}
def generate(destination):
 source=Path(__file__).parent/'vendor/src/autofit/afhints.h'
 text=source.read_text()
 for name,size in CAPACITIES.items():
  text,count=re.subn(r'(#define\s+'+name+r'\s+)\d+',lambda m:m[1]+str(size),text)
  if count!=1:raise RuntimeError('Upstream autofit header changed: '+name)
 text='/* Generated project adaptation: smaller inline scratch; original allocation fallback retained. */\n'+text
 destination=Path(destination);destination.mkdir(parents=True,exist_ok=True)
 target=destination/'kiiko_afhints.h'
 if not target.exists() or target.read_text()!=text:target.write_text(text)
 return destination
if __name__=='__main__':generate(sys.argv[1])
