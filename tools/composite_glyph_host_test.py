#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""中文：复合字形子引用早于父字形时，父字形必须仍指向自己的轮廓。/ English: A composite whose children precede the parent must still resolve to the parent's own outline."""
from pathlib import Path
import subprocess, tempfile

ROOT = Path(__file__).resolve().parents[1]
FONT = ROOT / 'main/assets/builtin.ttf'

STUBS = {
    'esp_err.h': '''#pragma once
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_NOT_FOUND 0x105
#define ESP_ERR_INVALID_RESPONSE 0x108
''',
    'esp_log.h': '''#pragma once
static inline void composite_test_log(const char *tag, const char *fmt, ...){(void)tag;(void)fmt;}
#define ESP_LOGI(...) composite_test_log(__VA_ARGS__)
#define ESP_LOGW(...) composite_test_log(__VA_ARGS__)
#define ESP_LOGE(...) composite_test_log(__VA_ARGS__)
#define ESP_LOGD(...) composite_test_log(__VA_ARGS__)
''',
    'esp_timer.h': '''#pragma once
#include <stdint.h>
static inline int64_t esp_timer_get_time(void){return 0;}
''',
    'esp_heap_caps.h': '''#pragma once
#include <stdlib.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
void *heap_caps_malloc(size_t n, int caps);
void *heap_caps_calloc(size_t n, size_t s, int caps);
void heap_caps_free(void *p);
static inline size_t heap_caps_get_largest_free_block(int caps){(void)caps;return 0;}
static inline size_t heap_caps_get_free_size(int caps){(void)caps;return 8u*1024u*1024u;}
''',
}

HARNESS = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ttf_font.h"

void *heap_caps_malloc(size_t n,int caps){(void)caps;return malloc(n);}
void *heap_caps_calloc(size_t n,size_t s,int caps){(void)caps;return calloc(n,s);}
void heap_caps_free(void *p){free(p);}
const char *app_settings_fonts_dir(void){return "/absent";}
const char *app_settings_font_path(void){return "builtin";}

static uint8_t canvas[512*512];
static unsigned painted;
static int min_x,max_x,min_y,max_y;

int epd_width(void){return 512;}
int epd_height(void){return 512;}
void epd_draw_pixel(int x,int y,uint8_t c,uint8_t *fb){
    if(x<0||y<0||x>=512||y>=512)return;
    if(!(c>>4)) {
        if(x<min_x)min_x=x; if(x>max_x)max_x=x;
        if(y<min_y)min_y=y; if(y>max_y)max_y=y;
    }
    ++painted;
    fb[y*512+x]=c>>4;
}

// 单个复合字形必须画出父字形的轮廓，而不是只画子字形（例如只剩声调）。
// A composite must paint the parent's own outline, not just a child (for example the diacritic alone).
static int ink_box(const char *label,const char *text,int px,int *out_h,int *out_w){
    memset(canvas,15,sizeof(canvas));
    painted=0;min_x=min_y=1<<20;max_x=max_y=-1;
    int above=0,below=0;
    ttf_measure_line_px(px,text,&above,&below);
    ttf_draw_text_px(canvas,20,20+above,px,text,EPD_DRAW_ALIGN_LEFT,0,15);
    if(max_x<0){printf("%s: nothing painted\n",label);return 0;}
    *out_w=max_x-min_x+1;*out_h=max_y-min_y+1;
    return 1;
}

int main(void){
    if(ttf_font_open_builtin()!=ESP_OK){puts("builtin open failed");return 1;}
    ttf_set_antialias(true);

    // "á" = "a" + dấu sắc. Chỉ còn dấu sắc thì chiều cao vượt trội nhưng bề rộng teo lại.
    // "á" is "a" plus acute; keeping only the diacritic gives a tall, narrow box.
    int h_a,w_a,h_aacute,w_aacute;
    if(!ink_box("a","a",40,&h_a,&w_a))return 1;
    if(!ink_box("á","á",40,&h_aacute,&w_aacute))return 1;
    printf("a   box %dx%d\n",w_a,h_a);
    printf("á   box %dx%d\n",w_aacute,h_aacute);
    if(w_aacute*2 < w_a || h_aacute < h_a){
        printf("FAIL: á lost its base glyph (base %dx%d, composite %dx%d)\n",w_a,h_a,w_aacute,h_aacute);
        return 1;
    }

    // Cùng lý do với các glyph tổ hợp tiếng Việt khác: chữ cái phải chiếm phần lớn bề rộng.
    // Same reasoning for other Vietnamese composites: the base letter must carry most of the width.
    static const char *pairs[][2]={{"e","é"},{"o","ó"},{"u","ú"},{"A","Á"},{"O","Ở"}};
    for(unsigned i=0;i<sizeof(pairs)/sizeof(pairs[0]);++i){
        int hb,wb,hc,wc;
        if(!ink_box(pairs[i][0],pairs[i][0],40,&hb,&wb))return 1;
        if(!ink_box(pairs[i][1],pairs[i][1],40,&hc,&wc))return 1;
        if(wc*2 < wb){
            printf("FAIL: %s lost its base glyph (base %dx%d, composite %dx%d)\n",
                   pairs[i][1],wb,hb,wc,hc);
            return 1;
        }
    }

    // 正文字号与标题字号都要成立：字形缓存在两个尺寸上分别打包。
    // Hold at body and title sizes: the glyph cache packs each size separately.
    int hs,ws,hl,wl;
    if(!ink_box("small á","á",22,&hs,&ws))return 1;
    if(!ink_box("large á","á",60,&hl,&wl))return 1;
    if(ws*2 < w_a*22/40 || wl*2 < w_a*60/40){
        printf("FAIL: á wrong at other sizes (22px %dx%d, 60px %dx%d)\n",ws,hs,wl,hl);
        return 1;
    }

    puts("PASS: composite glyphs keep their parent outline at body and title sizes");
    return 0;
}
'''

with tempfile.TemporaryDirectory() as folder:
    work = Path(folder)
    for name, content in STUBS.items():
        (work / name).write_text(content)
    data = FONT.read_bytes()
    assets = '#include <stdint.h>\nconst uint8_t data[] asm("_binary_builtin_ttf_start")={'
    assets += ','.join(map(str, data)) + '};\n'
    assets += ('asm(".globl _binary_builtin_ttf_end\\n.set _binary_builtin_ttf_end, '
               f'_binary_builtin_ttf_start + {len(data)}");\n')
    (work / 'assets.c').write_text(assets)
    (work / 'test.c').write_text(HARNESS)
    flags = ['cc', '-std=gnu11', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function',
             '-DKIIKO_BOARD_METALIO47=1',
             '-I' + str(work), '-I' + str(ROOT / 'main/font'),
             '-I' + str(ROOT / 'main/book'), '-I' + str(ROOT / 'main'),
             '-I' + str(ROOT / 'tools/ui_gesture_stubs')]
    subprocess.run(flags + ['-c', str(ROOT / 'main/font/ttf_font.c'), '-o', str(work / 'ttf.o')],
                   check=True)
    subprocess.run(flags + [str(work / 'test.c'), str(work / 'assets.c'), str(work / 'ttf.o'),
                            '-lm', '-o', str(work / 'test')], check=True)
    subprocess.run([str(work / 'test')], check=True)
