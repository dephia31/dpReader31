#!/usr/bin/env python3
"""Run parent-menu hit testing, scroll limits and nested Back state."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1];source=root/'main/apps/app_device_settings.c'
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
static int s_settings_group=-1,s_settings_root_scroll,s_main_scroll,scale=100;
static unsigned app_settings_system_font_size(void){return scale;}
static unsigned ui_text_system_percent(void){return app_settings_system_font_size();}
static int settings_viewport_end(void){return 1088;}
#define SETTINGS_WIRELESS_ROWS 7
#define SETTINGS_DISPLAY_ROWS 8
#define SETTINGS_PERSONAL_ROWS 4
#define SETTINGS_CONTROLS_ROWS 1
#define SETTINGS_MAINTENANCE_ROWS 7
#define SETTINGS_ROW_H settings_main_row_height()
#define SETTINGS_PARENT_Y (180+settings_profile_height()+28)
#define SETTINGS_PARENT_ROW_H 136
'''
for n in ('settings_stacked','settings_main_row_height','settings_profile_height','settings_menu_scroll_max','settings_parent_open','settings_parent_back'):
 unit+=function(n,source)+'\n'
unit+=r'''
int main(void){
 for(scale=100;scale<=200;scale+=10){
  s_settings_group=-1;assert(settings_menu_scroll_max()==0);
  assert(!settings_parent_open(SETTINGS_PARENT_Y-1));
  assert(!settings_parent_open(SETTINGS_PARENT_Y+5*SETTINGS_PARENT_ROW_H));
  assert(!settings_parent_back());
  for(int group=0;group<5;++group){
   int top=SETTINGS_PARENT_Y+group*SETTINGS_PARENT_ROW_H;
   assert(!settings_parent_open(top+SETTINGS_PARENT_ROW_H-1));
   s_main_scroll=37;assert(settings_parent_open(top+60));
   assert(s_settings_group==group&&s_main_scroll==0&&s_settings_root_scroll==37);
   int counts[]={7,8,4,1,7};int limit=settings_menu_scroll_max();
   assert(limit>=0&&240+counts[group]*SETTINGS_ROW_H-limit<=1088);
   // Detail pages retain their current parent; returning restores its scroll.
   s_main_scroll=limit;
   assert(s_settings_group==group);
   assert(settings_parent_back());assert(s_settings_group==-1&&s_main_scroll==37);
   assert(!settings_parent_back());
  }
 }
 puts("PASS parent menus: five groups,card gaps/bounds,11 text scales,last-child reachability,parent Back/root scroll restoration");
}
'''
with tempfile.TemporaryDirectory() as directory:
 p=Path(directory);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
