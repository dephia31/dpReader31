#!/usr/bin/env python3
"""Check actual TTF ink conversion and real Vietnamese glyphs at reader sizes."""
from pathlib import Path
import subprocess
import tempfile
import sys
from home_cover_cache_host_test import function

root = Path(__file__).resolve().parents[1]
font = root / 'main/font/ttf_font.c'
header = root / 'main/font/ttf_font.h'
gamma = next(line for line in header.read_text().splitlines()
             if line.startswith('#define TTF_COVER_GAMMA '))
unit = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
static uint8_t s_cover[256];
static bool s_antialias = true;
'''
unit += gamma + '\n'
unit += function('ttf_cover_lut_init', font) + '\n'
unit += function('mix_ink', font) + '\n'
unit += r'''
int main(int argc, char **argv) {
    assert(argc == 2);
    ttf_cover_lut_init();
    unsigned samples = 0, corrected = 0;
    for (int fg = 0; fg < 16; ++fg) for (int bg = 0; bg < 16; ++bg) {
        unsigned previous = bg;
        for (int a = 0; a < 256; ++a) {
            unsigned actual = mix_ink(a, fg, bg);
            double ideal = (bg * (255 - s_cover[a]) + fg * s_cover[a]) / 255.0;
            assert(fabs(actual - ideal) <= 0.5);
            assert(actual + mix_ink(a, 15-fg, 15-bg) == 15);
            assert(actual >= (unsigned)(fg < bg ? fg : bg));
            assert(actual <= (unsigned)(fg > bg ? fg : bg));
            if (fg < bg) assert(actual <= previous);
            if (fg > bg) assert(actual >= previous);
            previous = actual;
        }
        assert(mix_ink(0, fg, bg) == bg && mix_ink(255, fg, bg) == fg);
    }
    FILE *file = fopen(argv[1], "rb"); assert(file);
    fseek(file, 0, SEEK_END); long length = ftell(file); rewind(file);
    unsigned char *data = malloc(length); assert(data);
    assert(fread(data, 1, length, file) == (size_t)length); fclose(file);
    stbtt_fontinfo info;
    assert(stbtt_InitFont(&info, data, stbtt_GetFontOffsetForIndex(data, 0)));
    const int cps[] = {'H','i','m','n',0x1ebf,0x1ec7,0x1eaf,0x1ed9,0x110,0x1ef9};
    for (int px = 18; px <= 90; ++px) {
        float scale = stbtt_ScaleForPixelHeight(&info, px);
        for (unsigned i = 0; i < sizeof(cps)/sizeof(cps[0]); ++i) {
            assert(stbtt_FindGlyphIndex(&info, cps[i]) != 0);
            int w,h,x,y;
            unsigned char *bitmap = stbtt_GetCodepointBitmap(&info, scale, scale, cps[i], &w,&h,&x,&y);
            assert(bitmap && w > 0 && h > 0);
            for (int k = 0; k < w*h; ++k) {
                unsigned a = bitmap[k];
                int old = 15 + s_cover[a] * -15 / 255;
                int ink = mix_ink(a, 0, 15);
                assert(ink <= old && old - ink <= 1);
                corrected += ink < old;
                ++samples;
            }
            stbtt_FreeBitmap(bitmap, NULL);
        }
    }
    free(data);
    assert(corrected > 0);
    s_antialias = false;
    for (int a = 0; a < 256; ++a) assert(mix_ink(a,0,15) == (a >= 128 ? 0 : 15));
    printf("PASS: 65536 ink combinations; real Vietnamese glyphs: %u/%u pixels corrected; binary unchanged\n", corrected, samples);
}
'''
with tempfile.TemporaryDirectory() as directory:
    tmp = Path(directory)
    (tmp / 'test.c').write_text(unit)
    subprocess.run(['cc', '-std=c11', '-O2', '-I'+str(root/'main/font'),
                    str(tmp/'test.c'), '-lm', '-o', str(tmp/'test')], check=True)
    subprocess.run([str(tmp/'test'), str(Path(sys.argv[1]) if len(sys.argv) > 1 else root/'main/assets/builtin.ttf')], check=True)
