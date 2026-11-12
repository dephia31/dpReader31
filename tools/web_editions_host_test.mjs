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
    const data={version:archived||(love?'1.4.5-love-vn':'1.4.5-vn'),usb_parts:!archived||/^1\.4\.[34]/.test(archived)?{}:undefined,usb_only:false,notes:'Test',size:4710000,sha256:'0'.repeat(64),previous_version:love?'1.4.4-love-vn':'1.4.4-vn'};
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
assert.equal(get('release-version').options[1].value,'1.4.4-love-vn');
assert.equal(get('release-version').options[2].value,'1.4.3-love-vn');
assert.equal(get('release-installer-download').href,'./releases/love-book/dpReader-metalio47-1.4.5-love-vn-USB.zip');
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
