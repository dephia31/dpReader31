/* SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 执行实际网页脚本，验证目录操作与三类上传。/ Exercise the real page script for browsing, editing, and three uploads.
 */
const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const html = fs.readFileSync('components/read_pico_transfer/upload.html', 'utf8');
const source = html.match(/<script>([\s\S]*?)<\/script>/)[1];
const flush = () => new Promise(resolve => setImmediate(resolve));

class Element {
  constructor() {
    this.children = []; this.dataset = {}; this.value = ''; this.files = []; this.history = []; this.attributes = {}; this.disabled=false;
    this.classList = {toggle() {}};
  }
  set textContent(text) { this.text = String(text); this.children = []; this.history.push(this.text); }
  get textContent() { return this.text || ''; }
  set innerHTML(_) { throw Error('Do not parse file names as HTML'); }
  append(...children) { this.children.push(...children); }
  replaceChildren(...children) { this.children = children; }
  showModal() { this.open = true; }
  close() { this.open = false; }
  setAttribute(key,value) { this.attributes[key]=String(value); }
  getAttribute(key) { return this.attributes[key] ?? null; }
  focus() { this.focused=true; }
  getContext() { return {setTransform() {}, fillRect() {}, drawImage() {}, fillText() {}}; }
  getBoundingClientRect() { return {width: 260}; }
  setPointerCapture() {}
  toBlob(resolve) { assert.equal(this.width, 684); assert.equal(this.height, 1216); resolve({size: 120000}); }
}

async function setup({responses = [], confirmations = [true], mode = 'ap', mkdirStatus=200, directoryStatus=200} = {}) {
  const nodes = new Map();
  const sections = ['book', 'font', 'picture', 'file'].map(kind => {
    const section = new Element();
    section.dataset.kind = kind;
    const input = new Element(), result = new Element(), send = new Element();
    section.querySelector = selector => selector === 'input' ? input : selector === '.result' ? result : send;
    section.input = input; section.result = result; section.send = send;
    return section;
  });
  const document = {
    getElementById(id) { if (!nodes.has(id)) nodes.set(id, new Element()); return nodes.get(id); },
    createElement() { return new Element(); },
    querySelectorAll(selector) { return selector === '[role="tab"]' ? tabs : selector === '.upload' ? sections : [...nodes.values(), ...sections.flatMap(s => [s.input, s.send])]; },
  };
  for(const match of html.matchAll(/\bid="([^"]+)"/g))document.getElementById(match[1]);
  const tabs=Array.from(html.matchAll(/<button type="button" role="tab" id="([^"]+)" aria-controls="([^"]+)" aria-selected="([^"]+)" tabindex="([^"]+)">/g),match=>{
    const tab=document.getElementById(match[1]);tab.setAttribute('aria-controls',match[2]);tab.setAttribute('aria-selected',match[3]);tab.tabIndex=Number(match[4]);
    document.getElementById(match[2]).hidden=match[3]!=='true';return tab;
  });
  const state = {gets: [], mutations: [], sent: [], signature: '慢慢阅读', signatures: [], clock: 0, timers: new Map()};
  const fetch = async (url, options = {}) => {
    if(url === '/sync-config')return {status:200,json:async()=>({kosync_url:'https://sync.example.test',kosync_user:'',kosync_auto:0,opds_url:'https://books.example.test/opds',opds_user:''})};
    if(url === '/signature') { if(options.method === 'POST') {state.signature=options.body;state.signatures.push(options.body);}return {status:200,json:async()=>({signature:state.signature,ok:true})}; }
    if (url === '/info') return {status: 200, json: async () => ({
      mode, is_flash: false, free_bytes: 100000000, file_limit: 0,
    })};
    if (url.startsWith('/files?')) {
      state.gets.push(url);
      const path = new URL('http://pico' + url).searchParams.get('path') || '';
      if(directoryStatus!==200&&path.startsWith('dict'))return {status:directoryStatus,json:async()=>({error:'Không phải thư mục'})};
      const items = path ? [{name: '封面.jpg', directory: false, size: 1200000}]
        : [{name: 'books', directory: true, size: 0}, {name: 'readme.txt', directory: false, size: 12}];
      return {status: 200, json: async () => ({path, total: items.length, pages: 1, items})};
    }
    if (url === '/files' && options.method === 'POST') {
      state.mutations.push(JSON.parse(options.body));
      return {status:JSON.parse(options.body).action==='mkdir'?mkdirStatus:200, json: async () => ({ok: true})};
    }
    throw Error('Unexpected fetch: ' + url);
  };
  class XHR {
    constructor() { this.upload = {}; }
    open(method, url) { assert.equal(method, 'PUT'); this.url = url; }
    send(file) {
      state.sent.push({url: this.url, file});
      state.clock+=1000;
      this.upload.onprogress({loaded: file.size/2, total: file.size});
      state.clock+=1000;
      this.upload.onprogress({loaded: file.size, total: file.size});
      const response = responses.shift() || {status: 200, body: {ok: true}};
      queueMicrotask(() => {
        state.clock+=1000;
        for(const paint of state.timers.values())paint();
        if(response.failure){this['on'+response.failure]();return;}
        this.status = response.status; this.responseText = response.raw ?? JSON.stringify(response.body);
        this.onload();
      });
    }
  }
  class ImageMock {
    naturalWidth = 1200;
    naturalHeight = 1600;
    set src(value) { this.url = value; queueMicrotask(() => this.onload()); }
  }
  const context = vm.createContext({document, fetch, XMLHttpRequest: XHR, TextEncoder, URL,
    Image: ImageMock, performance: {now: () => state.clock},
    setInterval: paint => {const id=state.timers.size+1;state.timers.set(id,paint);return id},
    clearInterval: id => state.timers.delete(id),
    confirm: () => confirmations.length ? confirmations.shift() : true});
  context.URL = {createObjectURL: () => 'blob:test', revokeObjectURL() {}};
  vm.runInContext(source, context);
  await flush(); await flush();
  return {nodes, sections, state, context, tabs};
}

(async () => {
  const grouped=await setup();
  assert.equal(grouped.tabs.length,7);
  const selected=()=>grouped.tabs.filter(tab=>tab.getAttribute('aria-selected')==='true');
  const visible=()=>grouped.tabs.filter(tab=>!grouped.nodes.get(tab.getAttribute('aria-controls')).hidden);
  assert.equal(selected()[0],grouped.tabs[0]);assert.equal(visible().length,1);
  grouped.sections[0].input.files=[{name:'draft.epub',size:10}];
  grouped.nodes.get('kosyncUser').value='Bản nháp chưa lưu';
  for(const tab of grouped.tabs){tab.onclick();assert.equal(selected()[0],tab);assert.equal(visible()[0],tab);assert.equal(visible().length,1);}
  grouped.tabs[6].onkeydown({key:'ArrowRight',preventDefault(){}});assert.equal(selected()[0],grouped.tabs[0]);
  grouped.tabs[0].onkeydown({key:'End',preventDefault(){}});assert.equal(selected()[0],grouped.tabs[6]);
  grouped.tabs[6].onkeydown({key:'Home',preventDefault(){}});assert.equal(selected()[0],grouped.tabs[0]);
  assert.equal(grouped.sections[0].input.files[0].name,'draft.epub');assert.equal(grouped.nodes.get('kosyncUser').value,'Bản nháp chưa lưu');
  vm.runInContext('lock(true)',grouped.context);grouped.tabs[3].onclick();assert.equal(selected()[0],grouped.tabs[0]);
  vm.runInContext('lock(false)',grouped.context);grouped.tabs[3].onclick();assert.equal(selected()[0],grouped.tabs[3]);

  assert.match(html, /Duyệt thẻ nhớ TF/);
  assert.doesNotMatch(html, /图片对比|编辑书架名称/);
  let t = await setup();
  assert.match(t.nodes.get('storage').textContent, /Thẻ nhớ TF sẵn sàng/);
  assert.equal(t.nodes.get('entries').children.length, 2);
  const folder = t.nodes.get('entries').children[0];
  folder.children[0].children[0].onclick();
  await flush(); await flush();
  assert.equal(t.nodes.get('path').textContent, '/books');
  assert.match(t.state.gets.at(-1), /path=books/);
  const item = t.nodes.get('entries').children[0];
  const rename = item.children[1].children[0];
  rename.onclick();
  t.nodes.get('editorName').value = '新封面.jpg';
  await t.nodes.get('editorConfirm').onclick();
  assert.deepEqual(t.state.mutations[0], {action: 'rename', path: 'books/封面.jpg', target: 'books/新封面.jpg'});
  t.nodes.get('newFolder').onclick();
  t.nodes.get('editorName').value = '新目录';
  await t.nodes.get('editorConfirm').onclick();
  assert.deepEqual(t.state.mutations[1], {action: 'mkdir', path: 'books', target: 'books/新目录'});
  item.children[1].children[1].onclick();
  t.nodes.get('editorName').value = '封面副本.jpg';
  await t.nodes.get('editorConfirm').onclick();
  assert.deepEqual(t.state.mutations[2], {action: 'copy', path: 'books/封面.jpg', target: 'books/封面副本.jpg'});
  item.children[1].children[2].onclick();
  await t.nodes.get('editorConfirm').onclick();
  assert.deepEqual(t.state.mutations[3], {action: 'delete', path: 'books/封面.jpg'});
  for (const [kind, name, endpoint] of [
    ['book', '海边的信.epub', '/upload'],
    ['font', '思源宋体.otf', '/font-upload'],
    ['picture', '封面.jpeg', '/image-upload'],
  ]) {
    const section = t.sections.find(s => s.dataset.kind === kind);
    section.input.files = [{name, size: 1200000}];
    await section.send.onclick();
    assert.match(t.state.sent.at(-1).url, new RegExp('^' + endpoint));
    assert.match(section.result.textContent, /Đã lưu/);
    assert.match(section.result.textContent, /400 KB\/s · 3.0 giây/);
    const sent=section.result.history.findIndex(text=>text.startsWith('Đang chờ thiết bị lưu'));
    const saved=section.result.history.findIndex(text=>text.startsWith('Đã lưu'));
    assert(sent>=0&&saved>sent);
    assert.match(section.result.history[sent], /600 KB\/s · 2.0 giây/);
    assert(section.result.history.some(text=>text.startsWith('Đang chờ thiết bị lưu')&&text.endsWith('400 KB/s · 3.0 giây')));
    assert.equal(t.state.timers.size,0);
  }
  for(const response of [{failure:'error'},{failure:'abort'},{status:200,raw:'invalid JSON'}]){
    t=await setup({responses:[response]});
    const section=t.sections.find(s=>s.dataset.kind==='font');
    section.input.files=[{name:'test.ttf',size:1200000}];
    await section.send.onclick();
    assert.equal(t.state.timers.size,0);
    assert(!section.result.textContent.startsWith('Đã lưu'));
    assert(!section.send.disabled&&!section.input.disabled);
  }
  t = await setup({responses: [{status: 409, body: {error: '同名文件已存在'}}, {status: 200, body: {ok: true}}]});
  t.sections[2].input.files = [{name: '同名.png', size: 100}];
  await t.sections[2].send.onclick();
  assert.match(t.state.sent[1].url, /overwrite=1/);
  // 热点/已有 WiFi 共用入口，目录目标与多文件串行上传保持一致。
  // AP/STA share the same entry, directory target and serial multi-file uploads.
  for (const mode of ['ap', 'sta']) {
    t = await setup({mode});
    assert.equal(t.nodes.get('signatureText').value,'慢慢阅读');
    t.nodes.get('signatureText').value='我的 Pico';await t.nodes.get('signatureSave').onclick();assert.equal(t.state.signature,'我的 Pico');
    t.nodes.get('signatureText').value='';await t.nodes.get('signatureSave').onclick();assert.equal(t.state.signature,'');
    t.nodes.get('signatureText').value='字'.repeat(32);await t.nodes.get('signatureSave').onclick();assert.equal(t.state.signatures.length,2);

    const generic = t.sections.find(s => s.dataset.kind === 'file');
    generic.input.files = [{name: 'README', size: 0}, {name: '升级 包%+#.bin', size: 32}, {name: '.env', size: 17}];
    await generic.send.onclick();
    assert.equal(t.state.sent.length, 3);
    assert.deepEqual(t.state.sent.map(s => new URL('http://pico' + s.url).searchParams.get('path')), ['README', '升级 包%+#.bin', '.env']);
    assert.match(generic.result.textContent, /Đã lưu 3 tệp vào \/$/);
    t.nodes.get('entries').children[0].children[0].children[0].onclick();
    await flush(); await flush();
    generic.input.files = [{name: '文件 没有扩展名', size: 100}];
    await generic.send.onclick();
    assert.equal(new URL('http://pico' + t.state.sent.at(-1).url).searchParams.get('path'), 'books/文件 没有扩展名');
    assert.match(generic.result.textContent, /Đã lưu \/books\/文件 没有扩展名/);
  }
  for(const response of [{failure:'error'},{failure:'abort'},{status:200,raw:'invalid JSON'}]){
    t=await setup({responses:[response]});
    const section=t.sections.find(s=>s.dataset.kind==='font');
    section.input.files=[{name:'test.ttf',size:1200000}];
    await section.send.onclick();
    assert.equal(t.state.timers.size,0);
    assert(!section.result.textContent.startsWith('Đã lưu'));
    assert(!section.send.disabled&&!section.input.disabled);
  }
  t = await setup({responses: [{status: 409, body: {error: '同名文件已存在'}}, {status: 200, body: {ok: true}}]});
  const generic = t.sections.find(s => s.dataset.kind === 'file');
  generic.input.files = [{name: '升级.bin', size: 32}];
  await generic.send.onclick();
  assert.match(t.state.sent[1].url, /^\/file-upload\?path=.+&overwrite=1$/);
  t = await setup({mode: 'sta'});
  assert.equal(t.nodes.get('entries').children.length, 2);
  t.nodes.get('entries').children[1].children[1].children[1].onclick();
  t.nodes.get('editorName').value = 'readme 副本.txt';
  await t.nodes.get('editorConfirm').onclick();
  assert.deepEqual(t.state.mutations[0], {action: 'copy', path: 'readme.txt', target: 'readme 副本.txt'});
  t.nodes.get('wallpaperFile').files = [{name: 'photo.heic', size: 3000000}];
  t.nodes.get('wallpaperFile').onchange();
  await flush();
  assert.equal(t.nodes.get('wallpaperSend').disabled, false);
  t.nodes.get('cropZoom').value = '1.5';
  t.nodes.get('cropZoom').oninput();
  const beforeDrag = vm.runInContext('[cropX,cropY]', t.context);
  t.nodes.get('cropPreview').onpointerdown({pointerId: 1, clientX: 100, clientY: 100});
  t.nodes.get('cropPreview').onpointermove({pointerId: 1, clientX: 120, clientY: 90});
  t.nodes.get('cropPreview').onpointerup();
  const afterDrag = vm.runInContext('[cropX,cropY]', t.context);
  assert.notDeepEqual(afterDrag, beforeDrag);
  await t.nodes.get('wallpaperSend').onclick();
  assert.match(t.state.sent.at(-1).url, /^\/image-upload\?name=dpreader-wallpaper-.+&wallpaper=1$/);
  assert.match(t.nodes.get('wallpaperResult').textContent, /Đã đặt làm hình nền màn hình khóa/);
  // Whole StarDict folder, compressed parts, optional synonyms, metadata last.
  const dict=await setup();
  dict.nodes.get('dictionaryFolder').files=['sample.ifo','sample.idx.gz','sample.dict.dz','sample.syn','README.md'].map(name=>({name,size:20,webkitRelativePath:'Anh-Viet/'+name}));
  await dict.nodes.get('dictionarySend').onclick();
  assert.deepEqual(dict.state.mutations.map(x=>x.target),['dict','dict/Anh-Viet']);
  assert.equal(dict.state.sent.length,4);
  assert.ok(dict.state.sent.every(x=>decodeURIComponent(x.url).startsWith('/file-upload?path=dict/Anh-Viet/')));
  assert.ok(dict.state.sent.at(-1).url.endsWith('sample.ifo'));
  assert.match(dict.nodes.get('dictionaryResult').textContent,/Đã lưu 4 tệp/);
  assert.ok(!dict.nodes.get('dictionarySend').disabled);
  await dict.nodes.get('dictionaryBrowse').onclick();
  assert.equal(vm.runInContext('path',dict.context),'dict');
  assert.equal(dict.nodes.get('tab-files').getAttribute('aria-selected'),'true');
  // Mobile fallback, preflight missing files/path traversal/deep nesting/space.
  const flat=await setup();flat.nodes.get('dictionaryName').value='Tiếng Việt';
  flat.nodes.get('dictionaryFiles').files=['vi.ifo','vi.idx','vi.dict'].map(name=>({name,size:10}));
  await flat.nodes.get('dictionarySend').onclick();assert.equal(flat.state.sent.length,3);
  for(const bad of [
    [{name:'x.ifo',size:1,webkitRelativePath:'English/x.ifo'}],
    ['x.ifo','x.idx','x.dict'].map(name=>({name,size:1,webkitRelativePath:'../'+name})),
    ['x.ifo','x.idx','x.dict'].map(name=>({name,size:1,webkitRelativePath:'deep/sub/'+name})),
    ['x.ifo','x.idx','x.dict'].map(name=>({name,size:50000000,webkitRelativePath:'English/'+name}))
  ]){const fail=await setup();fail.nodes.get('dictionaryFolder').files=bad;await fail.nodes.get('dictionarySend').onclick();assert.equal(fail.state.sent.length,0);assert.equal(fail.state.mutations.length,0);}
  const retry=await setup({responses:[{status:409,body:{error:'exists'}},{status:200,body:{ok:true}}]});
  retry.nodes.get('dictionaryFolder').files=['x.ifo','x.idx','x.dict'].map(name=>({name,size:10,webkitRelativePath:'English/'+name}));
  await retry.nodes.get('dictionarySend').onclick();assert.ok(retry.state.sent[1].url.endsWith('&overwrite=1'));assert.match(retry.nodes.get('dictionaryResult').textContent,/Đã lưu 3 tệp/);
  const interrupted=await setup({responses:[{status:507,body:{error:'Thẻ đầy'}}]});
  interrupted.nodes.get('dictionaryFolder').files=['x.ifo','x.idx','x.dict'].map(name=>({name,size:10,webkitRelativePath:'English/'+name}));
  await interrupted.nodes.get('dictionarySend').onclick();assert.equal(interrupted.state.sent.length,1);assert.ok(!interrupted.state.sent[0].url.endsWith('.ifo'));assert.ok(!interrupted.nodes.get('dictionarySend').disabled);assert.match(interrupted.nodes.get('dictionaryResult').textContent,/0\/3/);
  for(const directoryStatus of [200,400]){
   const existing=await setup({mkdirStatus:409,directoryStatus});
   existing.nodes.get('dictionaryFolder').files=['x.ifo','x.idx','x.dict'].map(name=>({name,size:10,webkitRelativePath:'English/'+name}));
   await existing.nodes.get('dictionarySend').onclick();assert.equal(existing.state.sent.length,directoryStatus===200?3:0);
  }
  console.log('transfer web interaction tests passed');
})().catch(error => { console.error(error); process.exitCode = 1; });
