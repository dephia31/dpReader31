"""Compile the released firmware UI/reader for an isolated browser instance."""
from pathlib import Path
import ast, json, os, re, runpy, shutil, subprocess, zipfile
r=Path(__file__).resolve().parents[1];web=r/'web';out=r/'build/web';out.mkdir(parents=True,exist_ok=True)
stage=out/'source';stage.mkdir(exist_ok=True)
runpy.run_path(str(r/'src/prepare_host.py'))
tree=ast.parse((r/'build.py').read_text())
sources=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='sources' for t in n.targets))
fw=Path(os.environ.get('PICO_SIM_FIRMWARE',r/'firmware'));inc=[web/'shim',r/'shim',fw/'main']+[fw/'main'/d for d in ['app','apps','ui','font','book','factory']]+list((fw/'components').glob('*/include'))+list((fw/'components').iterdir())+[web/'vendor',web/'vendor/zlib']
emcc=Path(os.environ.get('EMCC',r/'build/toolchains/emsdk/upstream/emscripten/emcc'))
flags=['-std=gnu11','-O2','-Wno-format','-Wno-unused-value','-Wno-incompatible-pointer-types','-Wno-implicit-const-int-float-conversion','-include',str(r/'shim/host.h')]+[str(x) for p in inc for x in ['-I',p]]
objects=[]
for name in sources:
    src=fw/'components/read_pico_search/read_pico_search.c' if name=='SEARCH' else fw/'main'/name
    if name=='apps/app_book.c':src=r/'src/book_preview.c'
    if not src.exists():continue
    text=src.read_text()
    text=re.sub(r'extern const uint8_t (\w+)\[\] asm\("_binary_[^"]+"\);',r'extern const uint8_t *\1;',text)
    if name=='apps/app_device_settings.c':
        text+='\nint sim_settings_view(void){return s_page;}\nint sim_settings_target(int i){const int rows[]={180+settings_profile_height()/2,SETTINGS_DISPLAY_Y+SETTINGS_ROW_H/2,SETTINGS_DISPLAY_Y+SETTINGS_ROW_H*5/2,SETTINGS_DISPLAY_Y+SETTINGS_ROW_H*9/2,SETTINGS_PERSONAL_Y+SETTINGS_ROW_H/2,SETTINGS_PERSONAL_Y+SETTINGS_ROW_H*3/2,SETTINGS_PERSONAL_Y+SETTINGS_ROW_H*5/2,SETTINGS_PERSONAL_Y+SETTINGS_ROW_H*7/2,SETTINGS_WIRELESS_Y+SETTINGS_ROW_H*5/2,SETTINGS_WIRELESS_Y+SETTINGS_ROW_H*3/2,SETTINGS_MAINTENANCE_Y+SETTINGS_ROW_H*3/2};return i>=0&&i<11?rows[i]-s_main_scroll:-1;}\n'
    if name=='apps/app_dashboard.c':text+='\nint sim_home_view(void){return s_view;}\n'
    if name=='apps/app_book.c':text+='\nint sim_book_busy(void){return s_scan_pending;}\nint sim_book_view(void){return s_view;}\nint sim_book_panel(void){return s_reader_panel;}\nint sim_reader_target(int i){if(i<0||i>5)return -1;EpdRect r=reader_font_card(i);return ((r.x+r.width/2)<<16)|(r.y+r.height/2);}\n'
    target=stage/(name.replace('/','_')+'.c');target.write_text(text)
    obj=stage/(target.stem+'.o');subprocess.run([str(emcc),*flags,'-c',str(target),'-o',str(obj)],check=True);objects.append(str(obj))
for src in [r/'src/platform.c',r/'src/services.c',r/'src/engine.c',web/'src/cover.c',web/'src/assets.c',web/'src/crypto.c',web/'vendor/sha256.c']:
    obj=stage/(src.stem+'.o');extra=['-DSIM_PLATFORM=1'] if src.name in ['platform.c','assets.c'] else []
    subprocess.run([str(emcc),*flags,*extra,'-c',str(src),'-o',str(obj)],check=True);objects.append(str(obj))
card=out/'data/card/books';card.mkdir(parents=True,exist_ok=True)
text='海边的信\n\n这是一份为 kiikoread 在线体验编写的阅读示例。你可以翻页、添加书签，或者打开工具栏调整字号和间距。\n\n'
paragraphs=['清晨的海面很安静。风从远处走来，掠过窗台，也翻动了桌上的一页书。','阅读不必匆忙。让目光沿着文字慢慢前行，在句子之间停留，也是一种收获。','合上书时，记得给今天留一点空白。等下一次打开，我们就从这一页继续。']
(card/'海边的信.txt').write_text(text+'\n\n'.join(paragraphs[i%3] for i in range(160)),encoding='utf-8')
for title in ['山间来信','慢慢读']:
    with zipfile.ZipFile(card/(title+'.epub'),'w') as z:
        z.writestr('mimetype','application/epub+zip',compress_type=zipfile.ZIP_STORED)
        z.writestr('META-INF/container.xml','<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles><rootfile full-path="content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>')
        z.writestr('content.opf',f'<package version="2.0" xmlns="http://www.idpf.org/2007/opf" unique-identifier="id"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:title>{title}</dc:title><dc:creator>kiikoread</dc:creator><dc:identifier id="id">demo-{title}</dc:identifier><dc:language>zh-CN</dc:language></metadata><manifest><item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/>'+''.join(f'<item id="c{i}" href="c{i}.xhtml" media-type="application/xhtml+xml"/>' for i in range(3))+'</manifest><spine toc="ncx">'+''.join(f'<itemref idref="c{i}"/>' for i in range(3))+'</spine></package>')
        z.writestr('toc.ncx','<ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1"><head/><docTitle><text>'+title+'</text></docTitle><navMap>'+''.join(f'<navPoint id="n{i}" playOrder="{i+1}"><navLabel><text>第{i+1}章</text></navLabel><content src="c{i}.xhtml"/></navPoint>' for i in range(3))+'</navMap></ncx>')
        for i in range(3):z.writestr(f'c{i}.xhtml','<html xmlns="http://www.w3.org/1999/xhtml"><head><title>'+title+'</title></head><body><h1>第'+str(i+1)+'章</h1>'+''.join('<p>'+paragraphs[j%3]+'</p>' for j in range(50))+'</body></html>')
for name in ['adler32','crc32','inflate','inftrees','inffast','zutil','deflate','trees','compress','uncompr']:
    src=web/'vendor/zlib'/(name+'.c');obj=stage/('zlib_'+name+'.o');subprocess.run([str(emcc),'-O2','-I',str(web/'vendor/zlib'),'-c',str(src),'-o',str(obj)],check=True);objects.append(str(obj))
dist=web/'dist';dist.mkdir(exist_ok=True)
command=[str(emcc),*objects,'-O2','--no-entry','-sMODULARIZE=1','-sEXPORT_ES6=1','-sEXPORT_NAME=createPico','-sALLOW_MEMORY_GROWTH=1','-sINITIAL_MEMORY=67108864','-sSTACK_SIZE=4194304','-sENVIRONMENT=web,worker','-sFORCE_FILESYSTEM=1','-sEXPORTED_FUNCTIONS=["_sim_init","_sim_command","_sim_shutdown","_sim_frame","_sim_page","_sim_locked","_sim_quick_open","_sim_assets_init","_sim_seed","_sim_settings_view","_sim_book_view","_sim_book_busy","_sim_book_panel","_sim_home_view","_sim_settings_target","_sim_reader_target","_malloc","_free"]','-sEXPORTED_RUNTIME_METHODS=["FS","ccall","UTF8ToString","HEAPU8","IDBFS"]','-lidbfs.js','-Wl,--wrap=time','--preload-file',str(out/'data')+'@/Data','--preload-file',str(fw/'main/assets')+'@/assets','-o',str(dist/'pico.js')]
subprocess.run(command,check=True)
manifest=json.loads((r/'source-manifest.json').read_text());(dist/'version.json').write_text(json.dumps({k:manifest[k] for k in ['version','release_commit']},indent=2))
for name in ['index.html','simulator.css','simulator.js','worker.js','guides.js','tour-controller.js','NOTICE.txt']:shutil.copy2(web/name,dist/name)
subprocess.run([os.environ.get('NODE',str(r/'build/toolchains/emsdk/node/24.19.0_64bit/bin/node')),'--input-type=module','-e',"import {tours} from './guides.js';import fs from 'node:fs';fs.writeFileSync('./dist/guides.json',JSON.stringify(Object.fromEntries(Object.entries(tours).flatMap(([key,t])=>t.ids.map(id=>[id,{tour:key,title:t.title,scope:t.scope||'步骤'}]))),null,2));"],cwd=web,check=True)
print('Browser firmware ready:',dist)

runpy.run_path(str(web/'pack_runtime.py'))['pack'](dist)
