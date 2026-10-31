#!/usr/bin/env python3
"""Exercise actual C transport and actual Lua plugin, using a controlled CRE API.
SPDX-License-Identifier: Apache-2.0
Requires lupa (Lua runtime) and SDK or system cJSON; no device/app integration claim.
"""
from pathlib import Path
import ctypes as C
import json
import os
import subprocess
import sys
import tempfile
from lupa import LuaRuntime
from notes_test_support import ROOT,cjson_flags
out=ROOT/'build/notes-roundtrip';out.mkdir(parents=True,exist_ok=True)
libpath=out/('exchange.dylib' if sys.platform=='darwin' else 'exchange.so')
subprocess.run(['cc','-std=c11','-D_DEFAULT_SOURCE','-D_DARWIN_C_SOURCE','-D_DEFAULT_SOURCE','-Wall','-Wextra','-Werror', '-dynamiclib' if sys.platform=='darwin' else '-shared','-fPIC','-I'+str(ROOT/'main/book'),str(ROOT/'main/book/book_annotations.c'),str(ROOT/'main/book/book_annotation_exchange.c'),*cjson_flags(),'-o',str(libpath)],check=True)
class Item(C.Structure):_fields_=[('start',C.c_uint32),('end',C.c_uint32),('note',C.c_char*128),('quote',C.c_char*256)]
class Notes(C.Structure):_fields_=[('text_hash',C.c_uint64),('path_hash',C.c_uint64),('chapter',C.c_uint32),('length',C.c_uint32),('count',C.c_uint32),('sequence',C.c_uint32),('items',Item*32)]
lib=C.CDLL(str(libpath))
lib.book_annotation_directory.argtypes=[C.c_char_p,C.c_char_p,C.c_size_t]
lib.book_annotation_directory.restype=C.c_bool
lib.book_annotations_load.argtypes=[C.POINTER(Notes),C.c_char_p,C.c_char_p,C.c_uint32,C.c_char_p,C.c_size_t]
lib.book_annotations_put.argtypes=[C.POINTER(Notes),C.c_char_p,C.c_size_t,C.c_size_t,C.c_char_p]
lib.book_annotations_save.argtypes=[C.POINTER(Notes),C.c_char_p]
lib.book_annotation_publish.argtypes=[C.POINTER(Notes),C.c_char_p,C.c_char_p]
lib.book_annotation_import.argtypes=[C.POINTER(Notes),C.c_char_p,C.c_char_p,C.POINTER(C.c_uint)]
for fn in ['book_annotations_load','book_annotations_put','book_annotations_save','book_annotation_publish','book_annotation_import']:getattr(lib,fn).restype=C.c_bool
lua=LuaRuntime(unpack_returned_tuples=True)
bridge=lua.execute((ROOT/'integrations/koreader/dpreader-sync.koplugin/bridge.lua').read_text())
# CRE stub returns real-looking opaque positions only through documented methods.
make_ui=lua.eval('''function(text)
 local d={};function d:isXPointerInDocument(p)return type(p)=="string" and p:match("^xp:%d+$")~=nil end
 function d:getTextFromXPointers(a,b)return text:sub(tonumber(a:sub(4))+1,tonumber(b:sub(4))) end
 function d:findAllText(query,case_insensitive,context,limit,regex,flags)
  assert(case_insensitive==false and limit==2 and regex==false and flags==1)
  local r={};local at=1;while #r<limit do local first,last=text:find(query,at,true);if not first then break end;r[#r+1]={start="xp:"..(first-1),["end"]="xp:"..last};at=first+1 end;return r
 end
 local a={annotations={}};function a:addItem(item)table.insert(self.annotations,item)end;function a:sortItems(items)table.sort(items,function(x,y)return x.pos0<y.pos0 end)end
 local toc={};function toc:getTocTitleByPage(p)return "Chapter One" end
 return {rolling=true,document=d,annotation=a,toc=toc}
end''')
def to_lua(x):
 if isinstance(x,dict):return lua.table_from({k:to_lua(v) for k,v in x.items()})
 if isinstance(x,list):return lua.table_from([to_lua(v) for v in x])
 return x
def to_py(x):
 if hasattr(x,'items'):
  items=list(x.items())
  if items and all(isinstance(k,int) for k,v in items):return [to_py(x[i]) for i in range(1,len(items)+1)]
  return {k:to_py(v) for k,v in items}
 return x
with tempfile.TemporaryDirectory() as temp:
 root=Path(temp);os.chdir(root);book=root/'book.epub';book.write_bytes(b'fixture book')
 side=root/'dpReader_note/book.epub';side.mkdir(parents=True);store=side/'snapshots';store.mkdir()
 path=side/'dpReader.annotations.json';text=b'apple banana carrot';a=Notes()
 assert lib.book_annotations_load(C.byref(a),str(store).encode(),b'',0,text,len(text))
 assert lib.book_annotations_put(C.byref(a),text,0,5,b'From firmware')
 assert lib.book_annotations_save(C.byref(a),str(store).encode())
 assert lib.book_annotation_publish(C.byref(a),book.name.encode(),text)
 assert bridge.exchangePath(str(book))==str(path)
 doc=json.loads(path.read_text());doc['chapters']=['Chapter One'];path.write_text(json.dumps(doc))
 ui=make_ui(text.decode())
 # Native KOReader note exported alongside firmware highlight.
 ui.annotation.annotations[1]=to_lua({'page':'xp:13','pos0':'xp:13','pos1':'xp:19','text':'carrot','note':'From KOReader','drawer':'underscore','chapter':'Chapter One','datetime':'2026-10-31 12:00:00'})
 def sync_ko():
  doc=to_lua(json.loads(path.read_text()));stats=bridge.sync(ui,doc)
  encoded=to_py(doc)
  if isinstance(encoded['annotations'],dict) and not encoded['annotations']:encoded['annotations']=[]
  path.write_text(json.dumps(encoded));return stats
 def sync_fw():
  before=(a.count,bytes(a.items))
  unresolved=C.c_uint();assert lib.book_annotation_import(C.byref(a),book.name.encode(),text,C.byref(unresolved))
  if before!=(a.count,bytes(a.items)):assert lib.book_annotations_save(C.byref(a),str(store).encode())
  assert lib.book_annotation_publish(C.byref(a),book.name.encode(),text)
  return unresolved.value
 stats=sync_ko();assert stats.imported==1 and stats.exported==1
 assert sync_fw()==0 and a.count==2
 def apple():return next(v for k,v in ui.annotation.annotations.items() if v.text=='apple')
 apple().note='Edited on KOReader';sync_ko();assert sync_fw()==0
 assert next(i.note for i in a.items[:a.count] if i.start==0)==b'Edited on KOReader'
 assert lib.book_annotations_put(C.byref(a),text,0,5,b'Edited on firmware')
 assert lib.book_annotations_save(C.byref(a),str(store).encode());assert lib.book_annotation_publish(C.byref(a),book.name.encode(),text)
 sync_ko();assert apple().note=='Edited on firmware'
 # Remove managed KOReader carrot; it must disappear on firmware, not reappear.
 for i,v in list(ui.annotation.annotations.items()):
  if v.text=='carrot':lua.eval('table.remove')(ui.annotation.annotations,i)
 sync_ko();sync_fw();assert a.count==1 and a.items[0].start==0
 # Concurrent independent edits are retained as a conflict.
 apple().note='Local concurrent edit'
 assert lib.book_annotations_put(C.byref(a),text,0,5,b'Remote concurrent edit')
 assert lib.book_annotations_save(C.byref(a),str(store).encode());assert lib.book_annotation_publish(C.byref(a),book.name.encode(),text)
 stats=sync_ko();assert stats.conflicts==1 and apple().note=='Local concurrent edit'
 assert sync_fw()>0
 after=json.loads(path.read_text());assert sum(e['text']=='apple' for e in after['annotations'])==1
 # Resolve explicitly, then delete from firmware and verify deletion on KOReader.
 after['annotations'][0].pop('conflict',None)
 for e in after['annotations']:
  if e['text']=='apple':e.pop('conflict',None);e['note']='Local concurrent edit'
 path.write_text(json.dumps(after));sync_ko();sync_fw()
 a.count=0;assert lib.book_annotations_save(C.byref(a),str(store).encode());assert lib.book_annotation_publish(C.byref(a),book.name.encode(),text)
 sync_ko();assert len(list(ui.annotation.annotations.items()))==0
 # An unresolved oversized note remains byte-for-byte in JSON.
 record={'id':'foreign-long','text':'banana','note':'x'*200,'deleted':False,'source':'koreader','kiikoread':{'chapter':0}}
 doc=json.loads(path.read_text());doc['annotations'].append(record);path.write_text(json.dumps(doc))
 unresolved=C.c_uint();assert lib.book_annotation_import(C.byref(a),book.name.encode(),text,C.byref(unresolved));assert unresolved.value==1
 assert lib.book_annotation_publish(C.byref(a),book.name.encode(),text)
 assert next(e for e in json.loads(path.read_text())['annotations'] if e['id']=='foreign-long')==record
 # Corrupt primary recovers from previous valid JSON without deleting it.
 path.write_text('{broken');assert lib.book_annotation_import(C.byref(a),book.name.encode(),text,C.byref(unresolved))
 # Central paths separate same-name books and reject traversal.
 buf=C.create_string_buffer(512)
 assert lib.book_annotation_directory(b'/sdcard/books/book.epub',buf,512)
 assert buf.value==b'dpReader_note/books/book.epub'
 assert not lib.book_annotation_directory(b'/sdcard/../book.epub',buf,512)
 assert lib.book_annotation_directory(b'/sdcard/other/book.epub',buf,512)
 assert buf.value==b'dpReader_note/other/book.epub'
 # Import legacy JSON only before a central document exists; keep original intact.
 legacy=root/'legacy.sdr';legacy.mkdir()
 legacy_file=legacy/'kiikoread.annotations.json'
 legacy_doc={'schema':'kiikoread-koreader-annotations','version':1,'book':'legacy.epub','annotations':[]}
 legacy_file.write_text(json.dumps(legacy_doc));original=legacy_file.read_bytes()
 empty=Notes();assert lib.book_annotations_load(C.byref(empty),str(store).encode(),b'',3,text,len(text))
 assert lib.book_annotation_import(C.byref(empty),b'legacy.epub',text,C.byref(unresolved))
 assert lib.book_annotation_publish(C.byref(empty),b'legacy.epub',text)
 assert (root/'dpReader_note/legacy.epub/dpReader.annotations.json').exists()
 assert legacy_file.read_bytes()==original
 legacy_doc['annotations']=[record];legacy_file.write_text(json.dumps(legacy_doc))
 assert lib.book_annotation_import(C.byref(empty),b'legacy.epub',text,C.byref(unresolved)) and unresolved.value==0
 # Actual plugin path resolution mirrors nested folders and prefers central JSON.
 nested_book=root/'books/demo.epub';nested_book.parent.mkdir(exist_ok=True);nested_book.write_bytes(b'demo')
 nested_json=root/'dpReader_note/books/demo.epub/dpReader.annotations.json'
 nested_json.parent.mkdir(parents=True);nested_json.write_text(json.dumps(legacy_doc))
 assert bridge.exchangePath(str(nested_book))==str(nested_json)
 nested_json.rename(str(nested_json)+'.old')
 assert bridge.exchangePath(str(nested_book))==str(nested_json)
 assert bridge.exchangePath(str(root/'unopened.epub'))==str(root/'unopened.sdr/kiikoread.annotations.json')
 # Duplicate quote is never imported into an arbitrary CRE position.
 dup=to_lua({'schema':'kiikoread-koreader-annotations','version':1,'annotations':[{'id':'duplicate','text':'apple','note':'test','drawer':'underscore','deleted':False}]})
 stats=bridge.sync(make_ui('apple apple'),dup);assert stats.unresolved==1 and stats.imported==0
print('C ↔ actual Lua bridge create/edit/delete/conflict/ambiguous/recovery: PASS')
