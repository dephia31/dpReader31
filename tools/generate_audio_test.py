#!/usr/bin/env python3
"""Generate a quiet local16kHz PCM WAV test tone; no external audio resources."""
from pathlib import Path
import argparse,math,struct,wave
p=argparse.ArgumentParser(description=__doc__);p.add_argument('output',type=Path);a=p.parse_args()
a.output.parent.mkdir(parents=True,exist_ok=True)
with wave.open(str(a.output),'wb') as w:
 w.setnchannels(1);w.setsampwidth(2);w.setframerate(16000)
 samples=[]
 for n in range(16000*3):
  t=n/16000;within=t%1.0;envelope=min(1,within/.03,max(0,(.7-within)/.03))
  samples.append(round(6500*envelope*math.sin(2*math.pi*(440 if t<1 else 660 if t<2 else 880)*t)))
 w.writeframes(struct.pack('<%dh'%len(samples),*samples))
print(a.output.resolve())
