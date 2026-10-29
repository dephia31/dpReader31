#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""中文：页脚外观枚举迁移与可见性。/ English: Footer style enum migration and visibility."""
from pathlib import Path
import re, subprocess, tempfile

ROOT = Path(__file__).resolve().parents[1]
SETTINGS = ROOT / 'main/settings.c'
HEADER = ROOT / 'main/settings.h'


def function(name: str, path: Path) -> str:
    text = path.read_text()
    scan = re.sub(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"',
                  lambda m: ' ' * len(m[0]), text)
    start = re.search(r'^(?:static )?(?:[A-Za-z_]\w*(?:\s+|\s*\*\s*))+' + name +
                      r'\([^;{}]*?\)\s*\{', scan, re.M)
    assert start, f'function not found: {name}'
    at, depth = start.end(), 1
    while depth:
        depth += (scan[at] == '{') - (scan[at] == '}')
        at += 1
    return text[start.start():at]


# Pull the real enum constants out of the header so the test cannot drift from them.
header = HEADER.read_text()
defs = dict(re.findall(r'#define (APP_BOOK_PROGRESS_STYLE_\w+)\s+(\d+)', header))
assert defs, 'progress style defines not found in settings.h'
define_block = '\n'.join(f'#define {k} {v}' for k, v in defs.items())

HARNESS = r'''
#include <stdio.h>
#include <stdint.h>

''' + define_block + r'''

static int failures;
#define CHECK(cond, ...) do { if (!(cond)) { printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); ++failures; } } while (0)

''' + function('book_progress_style_from_legacy', SETTINGS) + r'''

int main(void) {
    // 旧存档 0..2 依次对应 细/粗/极细，迁移后必须整体 +1。
    // Legacy 0..2 meant thin/thick/hairline, so migration shifts each by one.
    CHECK(book_progress_style_from_legacy(0) == APP_BOOK_PROGRESS_STYLE_THIN,
          "legacy 0 (thin) -> %u", book_progress_style_from_legacy(0));
    CHECK(book_progress_style_from_legacy(1) == APP_BOOK_PROGRESS_STYLE_THICK,
          "legacy 1 (thick) -> %u", book_progress_style_from_legacy(1));
    CHECK(book_progress_style_from_legacy(2) == APP_BOOK_PROGRESS_STYLE_HAIRLINE,
          "legacy 2 (hairline) -> %u", book_progress_style_from_legacy(2));

    // 未知的旧值回落到细条，绝不能被读成“隐藏”。
    // Unknown legacy values fall back to thin and must never read as hidden.
    for (unsigned v = 3; v < 256; ++v) {
        uint8_t got = book_progress_style_from_legacy((uint8_t)v);
        CHECK(got == APP_BOOK_PROGRESS_STYLE_THIN, "legacy %u -> %u (expected thin)", v, got);
    }

    // 隐藏档是 0，迁移函数绝不产出它；否则老用户开机就会丢页脚。
    // Hidden is 0 and migration never produces it, or existing users would lose the footer.
    for (unsigned v = 0; v < 256; ++v)
        CHECK(book_progress_style_from_legacy((uint8_t)v) != APP_BOOK_PROGRESS_STYLE_HIDDEN,
              "migration produced hidden from legacy %u", v);

    // 新枚举顺序固定：隐藏在最前，其余依次变粗再变细。
    // The new enum order is fixed: hidden first, then progressively thinner bars.
    CHECK(APP_BOOK_PROGRESS_STYLE_HIDDEN == 0, "hidden must be 0");
    CHECK(APP_BOOK_PROGRESS_STYLE_COUNT == 4, "count must be 4");

    if (failures) { printf("\n%d check(s) failed\n", failures); return 1; }
    puts("PASS: legacy footer styles shift to the new enum and never collapse to hidden");
    return 0;
}
'''

with tempfile.TemporaryDirectory() as folder:
    work = Path(folder)
    src = work / 'test.c'
    src.write_text(HARNESS)
    binary = work / 'test'
    subprocess.run(['cc', '-std=gnu11', '-Wall', '-Wextra', '-Werror',
                    str(src), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)

# 中文：直接执行真实页脚绘制，遍历所有开关和条宽，检查文字、对齐及正文边界。
# English: Execute the real footer for every toggle/style combination, checking labels, alignment and body bounds.
BOOK = ROOT / 'main/apps/app_book.c'
source = BOOK.read_text()
footer_defs = '\n'.join(re.findall(r'^#define READER_FOOTER_.*', source, re.M))
outer = function('draw_reader_outer', BOOK)
assert 'draw_reader_footer(fb, page);' in outer
assert 's_reader_fullscreen' not in source
assert 'toggle_reader_fullscreen' not in source
assert 'draw_reader_header' not in source
assert '(EpdRect){32, 75, 64, 64}' not in function('action_at', BOOK)
assert 'app_settings_reader_immersive' not in source
render = footer_defs + '\n' + r'''
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#define UI_LOCK_WIDTH 684
#define UI_LOCK_HEIGHT 1216
#define UI_BAR_TOP 1096
#define UI_BAR_H 96
#define UI_GRAY_WHITE 255
#define READER_PANEL_BACKGROUND_PREVIEW 9
#define UI_GRAY_LIGHT 200
#define UI_GRAY_BLACK 0
#define EPD_DRAW_ALIGN_CENTER 0
#define EPD_DRAW_ALIGN_LEFT 1
#define EPD_DRAW_ALIGN_RIGHT 2
typedef struct {int x,y,width,height;} EpdRect;
static int style,mask,texts,bars,page_y,pct_y,fast_mode;
static bool s_save_failed;
static int s_reader_panel,clears,stale_ink;
static unsigned page_count=101,progress_pct=61;
static EpdRect track_seen,done_seen;
static int page_w=100,pct_w=60;
static int app_settings_book_progress_style(void){return style;}
static bool app_settings_book_footer_page(void){return mask&1;}
static bool app_settings_book_footer_chapter(void){return mask&2;}
static bool app_settings_book_footer_pct(void){return mask&4;}
static int app_settings_reader_turn_effect(void){return fast_mode?2:0;}
static int ui_text_fixed_width_px(int px,const char *s){(void)px;return s&&strchr(s,'%')?pct_w:page_w;}
static unsigned percent(size_t page){(void)page;return progress_pct;}
static unsigned book_layout_page_count(void){return page_count;}
static void reader_footer_chapter_name(char *s,size_t n){snprintf(s,n,"Tên chương");}
static void fit_fixed_text(char *s,int px,int w){(void)s;(void)px;(void)w;}
static void ui_fill_rect_fast(EpdRect r,int color,uint8_t *fb){
 (void)fb;if(r.width==1){assert(color==UI_GRAY_BLACK);assert(r.y>=READER_FOOTER_TOP&&r.y+r.height<=UI_LOCK_HEIGHT-2);++bars;++stale_ink;return;}
 assert(r.x==36&&r.y==READER_FOOTER_TOP&&r.width==612&&r.height==READER_FOOTER_HEIGHT&&color==UI_GRAY_WHITE);
 stale_ink=0;++clears;
}
static void ui_reader_background_paint(uint8_t *fb,EpdRect r,bool blend){(void)fb;(void)r;assert(blend);}
static void ui_fill_round_rect(uint8_t *fb,EpdRect r,int radius,int color){
 (void)fb;(void)radius;assert(!fast_mode||color!=UI_GRAY_LIGHT);
 if(color==UI_GRAY_LIGHT)track_seen=r;else if(color==UI_GRAY_BLACK)done_seen=r;assert(r.width>=0&&r.x>=36&&r.x+r.width<=648);
 assert(r.y>=READER_FOOTER_TOP&&r.y+r.height<=UI_LOCK_HEIGHT-2);++bars;++stale_ink;
}
static void ui_text_fixed_vc(uint8_t *fb,int x,int y,int px,const char *s,int align,bool invert){
 (void)fb;(void)px;(void)invert;++texts;++stale_ink;
 if(align==EPD_DRAW_ALIGN_LEFT){assert(x==36);assert(!strcmp(s,page_count==101?"21 / 101":"21 / 1000000"));page_y=y;}
 else if(align==EPD_DRAW_ALIGN_RIGHT){assert(x==648);pct_y=y;}
 else assert(x==342&&!strcmp(s,"Tên chương"));
}
'''
render += define_block + '\n'
for name in ('reader_area', 'progress_rect', 'reader_footer_visible',
             'reader_progress_bar_height', 'reader_footer_chapter_rect', 'reader_footer_meta_rect',
             'reader_footer_bar_rect', 'draw_reader_footer'):
    render += function(name, BOOK) + '\n'
key_enum = re.search(r'typedef enum \{\s*APP_READER_KEY_PREV,[\s\S]*?\} app_reader_key_action_t;', HEADER.read_text())[0]
render += key_enum + '\nstatic uint8_t s_reader_keys[3],s_reader_hold_action,s_volume_keys[2];\n'
for name in ('app_settings_reader_key_action', 'app_settings_reader_hold_action',
             'app_settings_volume_key_action', 'app_settings_reader_key_action_allowed'):
    render += function(name, SETTINGS) + '\n'
render += r'''
int main(void){
 assert(READER_FOOTER_LINE_PX==40&&READER_FOOTER_META_PX==40);
 s_reader_keys[0]=s_reader_hold_action=s_volume_keys[0]=APP_READER_KEY_FULLSCREEN;
 assert(app_settings_reader_key_action(0)==APP_READER_KEY_TOOLS);
 assert(app_settings_reader_hold_action()==APP_READER_KEY_TOOLS);
 assert(app_settings_volume_key_action(0)==APP_READER_KEY_TOOLS);
 for(unsigned key=0;key<4;++key){
  assert(!app_settings_reader_key_action_allowed(key,APP_READER_KEY_FULLSCREEN));
  assert(app_settings_reader_key_action_allowed(key,APP_READER_KEY_TOOLS));
 }
 for(style=0;style<4;++style)for(mask=0;mask<8;++mask){
  texts=bars=page_y=pct_y=clears=0;stale_ink=999;draw_reader_footer(NULL,20);
  assert(clears==1&&stale_ink==texts+bars);
  assert(texts==!!(mask&1)+!!(mask&2)+!!(mask&4));assert((bars>0)==(style!=0));
  if((mask&5)==5)assert(page_y==pct_y);
  EpdRect body=reader_area(),chapter=reader_footer_chapter_rect(),area=progress_rect();
  assert(body.y==72&&body.y+body.height==UI_LOCK_HEIGHT-96);
  assert(area.y+area.height==UI_LOCK_HEIGHT-2&&area.height==94);
  assert(body.y+body.height<=chapter.y);assert(area.y<=chapter.y&&area.y+area.height>=page_y);
 }
 mask=7;style=2;page_count=1000000;draw_reader_footer(NULL,20);
 EpdRect fixed=reader_footer_bar_rect(page_w,pct_w,true,true);
 assert(READER_FOOTER_CHAPTER_TOP==4);
 // 页码/百分比都显示时，进度条与两个标签的间距相等。
 // With both labels on, the track keeps equal gaps to the page and percentage text.
 assert(fixed.x-(36+page_w)==648-pct_w-(fixed.x+fixed.width));
 for(style=1;style<4;++style)for(mask=0;mask<8;++mask)for(progress_pct=0;progress_pct<=100;++progress_pct){
  track_seen=done_seen=(EpdRect){0};draw_reader_footer(NULL,20);
  // 轨迹只随标签开关变化，不随页码位数或百分比数值移动。
  // The track depends on label toggles only, never on digit count or percentage value.
  EpdRect actual=reader_footer_bar_rect(page_w,pct_w,mask&1,mask&4);
  assert(track_seen.x==actual.x&&track_seen.width==actual.width&&track_seen.height<reader_progress_bar_height());
  if(progress_pct){assert(done_seen.x==actual.x&&done_seen.width==actual.width*(int)progress_pct/100&&done_seen.height==reader_progress_bar_height());}
  else assert(done_seen.width==0);
 }
 // 快读把整帧压成黑白：未读段必须画成点阵黑点，不能只依赖会被吞掉的浅灰。
 // Fast turns collapse the frame to BW; the unread track must be black dots instead of pale gray that disappears.
 fast_mode=1;style=2;mask=7;progress_pct=61;track_seen=done_seen=(EpdRect){0};bars=0;
 draw_reader_footer(NULL,20);
 assert(track_seen.width==0&&bars>0&&done_seen.width>0);
 fast_mode=0;
 // 只开一侧标签时，轨迹仍居中于剩余空间。
 // With one label, the track stays centred in the remaining span.
 for(mask=1;mask<8;mask+=2){
  EpdRect one=reader_footer_bar_rect(page_w,pct_w,mask&1,mask&4);
  int left=36+((mask&1)?page_w:0),right=648-((mask&4)?pct_w:0);
  assert(one.x-left==right-(one.x+one.width));
 }

 puts("PASS: footer labels/toggles/styles with stale-cache clearing, shared row, long page counts and body/refresh bounds");
}
'''
with tempfile.TemporaryDirectory() as folder:
    work = Path(folder)
    src = work / 'footer.c'
    src.write_text('#include <stdio.h>\n#include <stdint.h>\n' + render)
    binary = work / 'footer'
    subprocess.run(['cc', '-std=gnu11', '-Wall', '-Wextra', '-Werror', '-DKIIKO_BOARD_METALIO47=1',
                    str(src), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
