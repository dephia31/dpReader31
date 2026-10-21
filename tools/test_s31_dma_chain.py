#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""中文：校验真实S31 DMA链、长度、EOF和越界保护。/ English: Check actual S31 descriptor chains, lengths, EOFs and guards."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#define DMA_DESCRIPTOR_BUFFER_MAX_SIZE_64B_ALIGNED 4032
#define DMA_DESCRIPTOR_BUFFER_OWNER_DMA 1
typedef struct node {struct{unsigned size,length,suc_eof,owner;}dw0;void*buffer;struct node*next;}lcd_dma_desc_t;
static struct {size_t bb_size,nodes_per_bounce,num_dma_nodes;uint8_t*bounce_buffer[2];lcd_dma_desc_t*dma_nodes;}lcd;
'''
body=function('init_dma_descriptors',root/'boards/metalio47/src/components/epdiy/src/output_lcd/lcd_driver.c')
main=r'''
int main(void){unsigned cases=0;for(size_t bytes=1;bytes<=64*1025;bytes+=17){
 lcd.bb_size=bytes;lcd.nodes_per_bounce=(bytes+4031)/4032;lcd.num_dma_nodes=2*lcd.nodes_per_bounce;
 lcd_dma_desc_t*raw=calloc(lcd.num_dma_nodes+2,sizeof(*raw));assert(raw);raw[0].dw0.size=123;raw[lcd.num_dma_nodes+1].dw0.size=456;lcd.dma_nodes=raw+1;
 for(int b=0;b<2;++b){lcd.bounce_buffer[b]=malloc(bytes);assert(lcd.bounce_buffer[b]);}
 init_dma_descriptors();lcd_dma_desc_t*p=lcd.dma_nodes;
 for(unsigned round=0;round<3;++round)for(size_t b=0;b<2;++b){size_t offset=0,eofs=0;for(size_t n=0;n<lcd.nodes_per_bounce;++n){
 assert(p==lcd.dma_nodes+b*lcd.nodes_per_bounce+n);assert(p->buffer==lcd.bounce_buffer[b]+offset);assert(p->dw0.size>0&&p->dw0.size<=4032&&p->dw0.size==p->dw0.length);assert(offset%64==0);assert(p->dw0.owner==1);assert(p->dw0.suc_eof==(n+1==lcd.nodes_per_bounce));eofs+=p->dw0.suc_eof;offset+=p->dw0.size;p=p->next;}
 assert(offset==bytes&&eofs==1);}
 assert(p==lcd.dma_nodes&&raw[0].dw0.size==123&&raw[lcd.num_dma_nodes+1].dw0.size==456);
 for(int b=0;b<2;++b)free(lcd.bounce_buffer[b]);free(raw);++cases;}
 printf("PASS: %u real DMA chain sizes, three wraps, hardware length/alignment limits, one EOF per bounce and descriptor canaries\n",cases);}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(prefix+body+main)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
