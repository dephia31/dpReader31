#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""中文：全高系统子页键盘真实像素与触摸，不越出1216画布。/ English: Actual system-child keyboard pixels/hits stay within the1216 canvas."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
unit=(root/'tools/keyboard_refresh_host_test.c').read_text().split('int main(void)')[0]
unit=unit.replace('#include "../main/ui/ui_keyboard.c"','#include "'+str(root/'main/ui/ui_keyboard.c')+'"').replace('#define HEIGHT 1102','#define HEIGHT 1216').replace('560','UI_SYSTEM_KEYBOARD_TOP')
unit+=r'''
int main(void){
 assert(UI_SYSTEM_KEYBOARD_TOP==560);
 fb=malloc(BYTES);reference=malloc(BYTES);previous=malloc(BYTES);assert(fb&&reference&&previous);
 for(int ascii=0;ascii<2;++ascii){
  begin(ascii);
  for(int panel=0;panel<4;++panel){s_panel=panel;fresh(fb);repaint(false);}
 }
 begin(true);assert(ui_keyboard_press(26,742,UI_SYSTEM_KEYBOARD_TOP,0));repaint(false);
 assert(ui_keyboard_release());key(26,742);assert(!strcmp(value,"q"));
 begin(false);key(120,742);key(270,820);key(420,898);
 ui_keyboard_end();ui_ime_release();free(fb);free(reference);free(previous);
 puts("PASS: system560..1070 keyboard all layouts, actual press/release/character routing and incremental/full pixel equality above persistent navigation");
}
'''
with tempfile.TemporaryDirectory() as directory:
 p=Path(directory);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'tools/ui_gesture_stubs'),'-I'+str(root/'tools/html_text_stubs'),'-I'+str(root/'main/ui'),'-I'+str(root/'main/font'),'-I'+str(root/'components/read_pico_search/include'),str(p/'test.c'),str(root/'main/ui/ui_ime.c'),str(root/'main/ui/ui_text_edit.c'),str(root/'main/ui/ui_text_input.c'),str(root/'components/read_pico_search/read_pico_search.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
