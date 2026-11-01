#!/usr/bin/env python3
"""Verify the generated S31 identity comparison for every alignment and byte."""
from pathlib import Path
import tempfile,subprocess
root=Path(__file__).resolve().parents[1]
s=(root/'components/ble_page_turner/nimble_identity_sdk.cmake').read_text()
a=s.index('    const volatile uint8_t *identity_bytes = id_addr;');b=s.index('    if (identity_nonzero == 0)',a)
code=s[a:b]
unit=r'''
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
static bool nonzero(const uint8_t *id_addr){
'''+code+r'''
return identity_nonzero!=0;}
int main(void){uint8_t b[16];for(unsigned offset=0;offset<8;++offset){memset(b,0,sizeof(b));assert(!nonzero(b+offset));for(unsigned i=0;i<6;++i)for(unsigned value=1;value<256;++value){b[offset+i]=value;assert(nonzero(b+offset));b[offset+i]=0;}}puts("PASS identity byte comparison: all8 alignments, zero address and every nonzero byte/value");}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(unit);subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
