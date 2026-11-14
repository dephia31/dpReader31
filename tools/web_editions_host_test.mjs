import fs from 'node:fs/promises';
import vm from 'node:vm';
import assert from 'node:assert/strict';
import {webcrypto} from 'node:crypto';

const root = new URL('../', import.meta.url);
const code = (await fs.readFile(new URL('web/red-studio-app.js', root), 'utf8')).replace(/^import .*\n/, '');
function element(id='') {
  return {id, value:'', checked:false, disabled:false, hidden:false, style:{}, textContent:'', files:[], options:[], handlers:{},
    addEventListener(type, fn){this.handlers[type]=fn;}, focus(){}, classList:{toggle(){}},
    remove(i){this.options.splice(i,1);}, appendChild(x){this.options.push(x);}, replaceChildren(...x){this.options=x;},
  };
}
const ids = new Map();
const get = id => {if(!ids.has(id))ids.set(id,element(id));return ids.get(id);};
get('firmware-edition').value='full';
get('release-version').options=[{value:'',textContent:'Mới nhất'},{value:'1.3.1-vn',textContent:'Trước'},{value:'1.3.0-vn',textContent:'Cũ'}];
const requested=[];
let loveAvailable=true;
const context=vm.createContext({console, navigator:{}, document:{getElementById:get,querySelectorAll:()=>[],createElement:()=>element()},
  fetch:async url=>{
    requested.push(url);
    const love=url.includes('/love-book/');
    const archived=url.match(/\/(\d+\.\d+\.\d+(?:-love)?-vn)\//)?.[1];
    const data={version:archived||(love?'1.4.6-love-vn':'1.4.6-vn'),usb_parts:!archived||/^1\.4\.[34]/.test(archived)?{}:undefined,usb_only:false,notes:'Test',size:4710000,sha256:'0'.repeat(64),previous_version:love?'1.4.5-love-vn':'1.4.5-vn'};
    return {ok:!love||loveAvailable,json:async()=>data};
  },crypto:webcrypto,DataView,Date,Map,Array,ESPLoader:class{},Transport:class{},setTimeout,Uint8Array,TextDecoder});
vm.runInContext(code, context);
await vm.runInContext('fetchLatestRelease()',context);
assert.equal(vm.runInContext('editionReady',context),true);
assert.equal(vm.runInContext('selectedReleaseFolder()',context),'releases');
get('firmware-edition').value='love-book';get('firmware-edition').handlers.change();
await vm.runInContext('fetchLatestRelease()',context);
assert.equal(vm.runInContext('selectedReleaseFolder()',context),'releases/love-book');
assert.equal(get('release-tf-download').href,'./releases/love-book/dpReader_metalio47_firmware.bin');
assert.equal(vm.runInContext('editionReady',context),true);
assert.equal(get('release-version').options[1].value,'1.4.5-love-vn');
assert.equal(get('release-version').options[2].value,'1.4.4-love-vn');
assert.equal(get('release-installer-download').href,'./releases/love-book/dpReader-metalio47-1.4.6-love-vn-USB.zip');
assert.equal(get('flash-modes').hidden,false);
get('release-version').value='1.4.2-love-vn';get('release-version').handlers.change();
assert.equal(get('flash-modes').hidden,true);
get('release-version').value='1.3.2-love-vn';get('release-version').handlers.change();
assert.equal(vm.runInContext('selectedReleaseFolder()',context),'releases/love-book/1.3.2-love-vn');
get('firmware-edition').value='full';get('firmware-edition').handlers.change();
await vm.runInContext('fetchLatestRelease()',context);
assert.equal(get('release-version').options[2].value,'1.3.0-vn');
get('release-version').value='1.3.1-vn';get('release-version').handlers.change();
assert.equal(vm.runInContext('selectedReleaseFolder()',context),'releases/1.3.1-vn');
get('firmware-edition').value='love-book';get('firmware-edition').handlers.change();
loveAvailable=false;requested.length=0;
await vm.runInContext('fetchLatestRelease()',context);
assert.equal(vm.runInContext('editionReady',context),false);
assert.equal(get('flash-btn').disabled,true);
assert.equal(get('release-tf-download').hidden,true);
assert.equal(get('release-usb-download').hidden,true);
assert.equal(get('release-installer-download').hidden,true);
assert.ok(requested.every(url=>url.includes('/love-book/')));
assert.equal(get('latest-version-name').textContent,'Chưa phát hành');
console.log('PASS Web Flash: Full/Love Book independent URLs, app/full links, per-edition history, unavailable Love Book blocks flash/downloads without Full fallback');

const app=new Uint8Array(512);app[0]=0xe9;const dataView=new DataView(app.buffer);dataView.setUint16(12,32,true);dataView.setUint32(32,0xabcd5432,true);
app.set(new TextEncoder().encode('1.3.3-love-vn'),48);app.set(new TextEncoder().encode('Kiikoread_Metalio_S31'),80);
const sha=Array.from(new Uint8Array(await webcrypto.subtle.digest('SHA-256',app)),b=>b.toString(16).padStart(2,'0')).join('');
context.image=app.buffer;context.release={size:app.length,version:'1.3.3-love-vn',sha256:sha};
await vm.runInContext('verifySelectedFirmware(image,0,release,"love-book")',context);
await assert.rejects(vm.runInContext('verifySelectedFirmware(image,0,release,"full")',context));
app[300]^=1;await assert.rejects(vm.runInContext('verifySelectedFirmware(image,0,release,"love-book")',context));
app[300]^=1;const merged=new Uint8Array(65536+app.length);merged.set(app,65536);context.image=merged.buffer;
await vm.runInContext('verifySelectedFirmware(image,65536,release,"love-book")',context);
console.log('PASS Web Flash binary gate: app/full app offset, chip/project/embedded edition/version and SHA-256; wrong edition and damaged image rejected before writes');

context.release.version="1.3.3RC4-love-vn";await assert.rejects(vm.runInContext('verifySelectedFirmware(image,65536,release,"love-book")',context));console.log("PASS RC firmware rejected by Web Flash");

// Use the release partition table to exercise the separate local full-image flow.
const table = await fs.readFile(new URL('releases/partitions.bin', root));
const bootImage = await fs.readFile(new URL('releases/bootloader.bin', root));
const applicationImage = await fs.readFile(new URL('releases/dpReader_metalio47_firmware.bin', root));
const localImage = new Uint8Array(0x10000 + applicationImage.length).fill(255);
localImage.set(table, 0x8000);
localImage.set(applicationImage, 0x10000);
localImage.set(bootImage, 0x2000);
context.customImage = localImage;
await vm.runInContext('validateCustomFullImage(customImage)', context);
// Headers alone must never qualify for an erase-all installation.
for (const length of [0x10000 + 288, localImage.length - 1]) {
  context.customImage = localImage.slice(0, length);
  await assert.rejects(vm.runInContext('validateCustomFullImage(customImage)', context));
}
for (const offset of [0x2020, 0x10120, localImage.length - 1]) {
  context.customImage = localImage.slice();
  context.customImage[offset] ^= 1;
  await assert.rejects(vm.runInContext('validateCustomFullImage(customImage)', context));
}
const writes = [];
const alerts = [];
let agreed = false;
context.confirm = () => agreed;
context.alert = message => alerts.push(message);
context.console = { ...console, error() {} };
context.mockLoader = {
  chip: { CHIP_NAME: 'ESP32-S31' }, IS_STUB: true,
  detectFlashSize: async () => '16MB',
  writeFlash: async options => writes.push(options),
};
vm.runInContext('esploader = mockLoader; resetIntoApp = async () => true; releasePortAfterFlash = async () => {};', context);
get('mode-custom').checked = true;
const flashLocal = async data => {
  get('custom-file').files = [{name: 'local.bin', size: data.length, arrayBuffer: async () => data.buffer}];
  get('risk-accepted').checked = true;
  await get('flash-btn').handlers.click();
};
await flashLocal(localImage);
assert.equal(writes.length, 0);
agreed = true;
await flashLocal(app);
assert.match(alerts.at(-1), /ảnh cài toàn phần/);
assert.equal(writes.length, 0);
await flashLocal(localImage.slice(0, 0x10000 + 288));
assert.equal(writes.length, 0);
await flashLocal(localImage);
assert.equal(writes.length, 1);
assert.equal(writes[0].eraseAll, true);
assert.equal(writes[0].fileArray.length, 1);
assert.equal(writes[0].fileArray[0].address, 0);
console.log('PASS local .bin: no release manifest required, confirm before erase, reject app-only, erase and write at 0x0');
