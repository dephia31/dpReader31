/* SPDX-License-Identifier: Apache-2.0
 * 中文：执行实际快照校验和串口适配器，覆盖旧缓存、错误版本、损坏、历史版本和失败前不写设备。
 * English: Run actual snapshot verification and serial adapter against stale caches, wrong versions, corruption, archives and pre-write failures.
 */
const fs = require('node:fs'), vm = require('node:vm'), assert = require('node:assert/strict');
const {webcrypto, createHash} = require('node:crypto');
const sha = bytes => createHash('sha256').update(bytes).digest('hex');
const path = require('node:path');
const assets = path.join(__dirname, 'fixtures/flasher');
const original = JSON.parse(fs.readFileSync(path.join(assets, 'manifest.json')));
const bytes = new Map();
const manifest = JSON.parse(JSON.stringify(original));
for (const part of manifest.builds[0].parts) {
  const data = Buffer.alloc(part.offset === 0x10000 ? 256 : 64, part.offset & 255);
  if (part.offset === 0x10000) {
    data[0] = 0xe9; data.writeUInt32LE(0xabcd5432, 32);
    data.write(manifest.version, 48); data.write('Read_Pico', 80);
  }
  bytes.set(part.path, data); part.size = data.length; part.sha256 = sha(data);
}
const script = fs.readFileSync(path.join(assets, 'flasher-integrity.js'), 'utf8');
let fault, calls = [], live = new Set();
class CheckedURL extends URL {
  static createObjectURL(blob) {const url = URL.createObjectURL(blob); live.add(url); return url;}
  static revokeObjectURL(url) {assert(live.delete(url)); URL.revokeObjectURL(url);}
}
const context = vm.createContext({window: {}, URL: CheckedURL, crypto: webcrypto, Blob, DataView, TextDecoder, Uint8Array,
  fetch: async (url, options) => {
    assert.equal(options.cache, 'no-store'); const u = new URL(url);
    assert(/^[a-f0-9]{64}$/.test(u.searchParams.get('sha256')));
    calls.push(u); let name = u.pathname.split('/').pop();
    if (name.startsWith('Pico-update-')) name = 'firmware.bin';
    let payload = Buffer.from(bytes.get(name));
    if (name === 'firmware.bin' && fault) payload = fault(payload);
    return new Response(payload);
  }});
vm.runInContext(script, context);
const prepare = (m = manifest, archives) => context.window.KiikoreadFlasher.prepare(m,
  archives ? 'https://kiikoread.com/releases/0.3.3-rc86/manifest.json' : 'https://kiikoread.com/manifest.json', archives);
async function verify(snapshot) {
  const m = await (await fetch(snapshot.url)).json();
  assert.equal(m.version, manifest.version);
  for (const part of m.builds[0].parts) {
    assert(part.path.startsWith('blob:'));
    const data = Buffer.from(await (await fetch(part.path)).arrayBuffer());
    assert.equal(data.length, part.size); assert.equal(sha(data), part.sha256);
  }
}
(async () => {
  const first = await prepare(); await verify(first);
  assert.equal(live.size, 5); assert(calls.some(u => u.pathname.endsWith(`Pico-update-${manifest.version}.bin`)));
  assert(!calls.some(u => u.pathname.endsWith('/firmware.bin')));
  const second = await prepare(); await verify(first); await verify(second);
  first.dispose(); first.dispose(); assert.equal(live.size, 5); second.dispose(); assert.equal(live.size, 0);
  fault = data => {data.fill(0, 48, 80); data.write('0.3.3-rc95', 48); return data;};
  await assert.rejects(prepare(), /校验失败/); assert.equal(live.size, 0);
  const wrong = JSON.parse(JSON.stringify(manifest)), old = fault(Buffer.from(bytes.get('firmware.bin')));
  wrong.builds[0].parts.find(p => p.offset === 0x10000).sha256 = sha(old);
  await assert.rejects(prepare(wrong), /内部版本/); assert.equal(live.size, 0);
  fault = data => data.subarray(0, data.length - 1);
  await assert.rejects(prepare(), /校验失败/); assert.equal(live.size, 0);
  fault = undefined;
  const missing = JSON.parse(JSON.stringify(manifest)); delete missing.builds[0].parts[0].sha256;
  const priorCalls = calls.length;
  await assert.rejects(prepare(missing), /完整性/); assert.equal(calls.length, priorCalls);
  const archive = JSON.parse(JSON.stringify(manifest)), info = {};
  archive.version = '0.3.3-rc86';
  const app = Buffer.from(bytes.get('firmware.bin')); app.fill(0, 48, 80); app.write(archive.version, 48);
  bytes.set('firmware.bin', app);
  for (const part of archive.builds[0].parts) {
    info[part.path] = {size: bytes.get(part.path).length, sha256: sha(bytes.get(part.path))};
    const name = part.path; delete part.size; delete part.sha256;
    part.path = part.offset === 0x10000 ? '../../Pico-update-0.3.3-rc86.bin' : name;
  }
  const archived = await prepare(archive, info); const am = await (await fetch(archived.url)).json();
  assert.equal(am.version, '0.3.3-rc86');
  assert.equal(Buffer.from(await (await fetch(am.builds[0].parts.find(p => p.offset === 0x10000).path)).arrayBuffer()).subarray(48, 60).toString(), '0.3.3-rc86\0\0');
  archived.dispose(); assert.equal(live.size, 0);

  // 实际适配器必须先请求用户串口，再校验，成功后才打开并交给安装器。
  // The actual adapter requests the user's port first, verifies next, and only then opens it for the installer.
  let adapter = fs.readFileSync(path.join(assets, 'install-button.js'), 'utf8').split(';class t extends')[0];
  adapter = adapter.replace(/import\("[^"\n]+"\)/g, 'Promise.resolve({i:{}})') + ';globalThis.invoke=e;';
  let events = [], alerts = [], listener, disposed = 0, opens = 0;
  const port = {open: async () => {opens++; events.push('open');}, close: async () => events.push('close')};
  const button = {textContent: '连接墨水屏，刷入 rc97'};
  const element = {querySelector: () => button, prepareManifest: async () => {
    events.push('verify'); return {url: 'blob:verified', dispose: () => disposed++};
  }};
  const document = {createElement: () => ({addEventListener: (name, cb) => {assert.equal(name, 'closed'); listener = cb;}}),
    body: {appendChild: dialog => {assert.equal(dialog.manifestPath, 'blob:verified'); events.push('dialog');}}};
  const adapterContext = vm.createContext({navigator: {serial: {requestPort: async () => {events.push('request'); return port;}}},
    document, alert: error => alerts.push(error), Promise});
  vm.runInContext(adapter, adapterContext);
  await adapterContext.invoke(element);
  assert.deepEqual(events, ['request', 'verify', 'open', 'dialog']); assert.equal(button.textContent, '连接墨水屏，刷入 rc97');
  listener(); assert.equal(disposed, 1);
  element.prepareManifest = async () => {throw new Error('内部版本不一致');};
  const before = opens; await adapterContext.invoke(element);
  assert.equal(opens, before); assert.equal(alerts.pop(), '内部版本不一致'); assert(!element._preparing);
  console.log('PASS: cache bypass, four hashes, embedded app version, corruption rejection, independent snapshots, archive compatibility and verification before port opening');
})().catch(error => {console.error(error); process.exitCode = 1;});


// 中文：执行当前网页真实文件选择分支，确认自选应用也切换启动槽，其他偏移不写otadata。
// English: Execute the current web file-selection branch; custom apps switch boot slots while other offsets leave otadata alone.
(async () => {
  const webSource = fs.readFileSync(path.join(__dirname, '../web/app.js'), 'utf8');
  const begin = webSource.indexOf('    if (radioApp.checked) {', webSource.indexOf('let clearOtadata = false;'));
  const end = webSource.indexOf('    const startTime = Date.now();', begin);
  assert(begin >= 0 && end > begin);
  const branch = webSource.slice(begin, end);
  const AsyncFunction = Object.getPrototypeOf(async function () {}).constructor;
  const select = new AsyncFunction('mode', 'customOffset', 'folder', `
    const radioApp = {checked: mode === 'app'}, radioFull = {checked: mode === 'full'}, radioCustom = {checked: mode === 'custom'};
    const payload = new Uint8Array([0xe9, 0, 0, 0]);
    const customFileInput = {files: [{name: 'rc-app.bin', arrayBuffer: async () => payload.buffer}]};
    const customOffsetInput = {value: customOffset}, progressStatus = {}, terminal = {writeLine() {}};
    const fetched = [];
    const timestamp = 1, fetchBinary = async (...urls) => {fetched.push(urls); return payload;}, selectedReleaseFolder = () => folder || "releases";
    let binaryData = null, offset = 0, fileName = '', clearOtadata = false;
    ${branch}
    fileArray.fetched = fetched; return fileArray;
  `);
  for (const mode of ['app', 'custom']) {
    const parts = await select(mode, '0x10000');
    assert.deepEqual(parts.map(p => p.address), [0x10000, 0xd000]);
    assert.equal(parts[1].data.length, 0x2000);
    assert(parts[1].data.every(byte => byte === 0xff));
  }
  assert.deepEqual((await select('full', '0x0')).map(p => p.address), [0]);
  assert.deepEqual((await select('custom', '0x510000')).map(p => p.address), [0x510000]);
  const archived = await select('app', '0x10000', 'releases/1.1.6-vn');
  assert(archived.fetched[0].every(url => url.includes('/releases/1.1.6-vn/')));
  console.log('PASS: web custom/app ota_0 writes include exact otadata reset; full and alternate offsets leave selection alone');
})().catch(error => {console.error(error); process.exitCode = 1;});

{
  const source=fs.readFileSync(path.join(__dirname,'../web/app.js'),'utf8');
  const helper=source.slice(source.indexOf('function selectedReleaseFolder()'),source.indexOf('const releaseVersionSelect'));
  for(const value of ['', '1.1.6-vn','../../private','1.1.7-vn']) {
    const context={document:{getElementById:()=>({value})}};
    vm.runInNewContext(helper,context);
    assert.equal(context.selectedReleaseFolder(),value==='1.1.6-vn'?'releases/1.1.6-vn':'releases');
  }
  console.log('PASS: official version chooser only selects the previous archive or the current release');
}
