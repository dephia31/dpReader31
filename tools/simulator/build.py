from pathlib import Path
import subprocess, json, runpy, os
r=Path(__file__).resolve().parent;f=Path(os.environ.get('PICO_SIM_FIRMWARE',r/'firmware'));out=r/'build';out.mkdir(exist_ok=True)
sources=['SEARCH','apps/app_image.c','apps/app_dashboard.c','apps/app_book.c','apps/book_toc.c','apps/app_files.c','apps/file_tree.c','apps/app_device_settings.c','app/app_registry.c','settings.c','lock_pin_flow.c','lock_pin.c','ui/datamatrix.c','ui/ui_reader_background.c','ui/ui_kit.c','ui/ui_nav.c','ui/ui_gesture.c','ui/ui_quick_menu.c','ui/ui_quick_menu_input.c','ui/ui_click_feedback.c','ui/ui_pinpad.c','ui/ui_acrylic.c','ui/ui_glass.c','ui/ui_wallpaper.c','ui/ui_image_dither.c','ui/ui_text_edit.c','ui/ui_text_input.c','ui/ui_keyboard.c','ui/ui_ime.c','font/ttf_font.c','font/ui_font.c','font/ui_hanzi.c','font/app_font_context.c','book/book_store.c','book/book_progress.c','book/book_history_backup.c','book/book_title.c','book/book_ticket.c','book/book_lock_collage.c','book/book_source.c','book/book_index_cache.c','book/book_txt.c','book/gbk.c','book/book_epub.c','book/book_image_header.c','book/book_cover_auto.c','book/zip_reader.c','book/html_text.c','book/book_layout.c','book/book_cover_jobs.c','book/book_lock_bookmark.c','book/book_excerpt.c','ui/ui_avatar.c','ui/ui_home_clock.c','ui/ui_home_widget.c']
incs=[r/'shim',f/'main']+[f/'main'/d for d in ['app','apps','ui','font','book','factory']]+list((f/'components').glob('*/include'))
flags=['-std=gnu11','-O1','-g','-fno-common','-Wno-format','-Wno-unused-value','-Wno-deprecated-declarations','-Wno-incompatible-pointer-types','-include',str(r/'shim/host.h')]+[str(x) for p in incs for x in ['-I',p]]
# Use the released shelf layout without experimental overrides.
runpy.run_path(str(r/'src/prepare_host.py'))
failed=[];objs=[]
for name in sources:
 source=(f/'components/read_pico_search/read_pico_search.c') if name=='SEARCH' else f/'main'/name
 if name=='apps/app_book.c': source=r/'src/book_preview.c'
 if not source.exists(): continue
 obj=out/(name.replace('/','_')+'.o')
 result=subprocess.run(['cc',*flags,'-c',str(source),'-o',str(obj)],capture_output=True,text=True)
 if result.returncode: failed.append((name,result.stderr))
 else: objs.append(str(obj))
(out/'compile-errors.txt').write_text('\n'.join(name+'\n'+err for name,err in failed))
(out/'objects.json').write_text(json.dumps(objs));(out/'flags.json').write_text(json.dumps(flags))
print('Compiled',len(objs),'failed',len(failed))
for name,err in failed:
 lines=err.splitlines(); first=next((x for x in lines if 'error:' in x),'');print(name,first)
if not failed and (r/'src/engine.c').exists():
 pass # Release Home is compiled directly.
 for name in ['platform.c','engine.c','services.c','cover.m','pin.c']:
  source=r/'src'/name
  if source.exists():
   obj=out/(name+'.o');extra=['-DSIM_PLATFORM=1'] if name in ['platform.c','cover.m'] else [];subprocess.run(['cc',*flags,*extra,'-c',str(source),'-o',str(obj)],check=True);objs.append(str(obj))
 assets=[('builtin_ttf','builtin.ttf'),('ui_hanzi_bin','ui-hanzi.bin'),('lock_4bpp_bin','lock_4bpp.bin'),('loading_4bpp_bin','loading_4bpp.bin'),('reading_md','reading.md'),('display_test_png','display-test.png')]
 asm=['.section __DATA,__const']
 for label,file in assets:
  asm += [f'.globl _binary_{label}_start',f'.globl _binary_{label}_end',f'_binary_{label}_start:',f'.incbin "{f/"main/assets"/file}"',f'_binary_{label}_end:']
 (r/'src/assets.S').write_text('\n'.join(asm)+'\n')
 obj=out/'assets.o';subprocess.run(['cc','-c',str(r/'src/assets.S'),'-o',str(obj)],check=True);objs.append(str(obj))
 subprocess.run(['cc',*objs,'-lz','-lpthread','-framework','Cocoa','-framework','ImageIO','-framework','CoreGraphics','-framework','Security','-o',str(out/'pico-engine')],check=True)
