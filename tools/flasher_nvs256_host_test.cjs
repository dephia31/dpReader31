/* Exercise the production click handler with simulated S31 flash. */
const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const {createHash, webcrypto} = require('node:crypto');
const appJs = fs.readFileSync('web/red-studio-app.js', 'utf8').replace(/^import[^\n]+\n/, '');
const html = fs.readFileSync('web/index.html', 'utf8');
function hash(kind, bytes) { return createHash(kind).update(bytes).digest('hex'); }
class Node {
  constructor(){this.checked=false;this.disabled=false;this.style={};this.events={};this.files=[];this.value='';this.classList={toggle(){}};this.options=[{value:''},{value:'1.4.1-vn'}];}
  addEventListener(k,fn){this.events[k]=fn;} focus(){}
}
async function run({confirmClean=true, corrupt=false, badMd5=false, wrongSize=false, love=false, archived=false, layout="new", fullInstall=false}={}) {
  const nodes=new Map([...html.matchAll(/\bid="([^"]+)"/g)].map(m=>[m[1],new Node()]));
  const get=id=>nodes.get(id), radios=['mode-full','mode-app','mode-custom'].map(get);
  const base = love ? 'releases/love-book' : 'releases';
  const folder = archived ? base + (love ? '/1.4.2-love-vn' : '/1.4.2-vn') : base;
  const feed = JSON.parse(fs.readFileSync(folder+'/metalio-s31-update.json','utf8'));
  get('firmware-edition').value=love?'love-book':'full';
  get('release-version').value=archived?feed.version:'';
  get('mode-full').checked=fullInstall;
  get('mode-app').checked=!fullInstall;
  const appName = archived ? 'web-firmware.bin' : 'dpReader_metalio47_firmware.bin';
  const parts = Object.fromEntries([appName,'partitions.bin','storage-empty.bin','bootloader.bin']
    .map(name => [name,fs.readFileSync(folder+'/'+name)]));
  const written=new Map(), writes=[], notices=[];
  let confirms=0;
  const loader={IS_STUB:true,chip:{CHIP_NAME:'ESP32-S31'},detectFlashSize:async()=>wrongSize?'8MB':'16MB',
    readFlash:async()=>{throw Error("Unexpected streamed flash read");},
    writeFlash:async({fileArray,eraseAll})=>{assert.equal(eraseAll,false);writes.push(Array.from(fileArray,x=>x.address));for(const x of fileArray)written.set(x.address,Buffer.from(x.data));},
    flashMd5sum:async(address,size)=>{
      if (!writes.length) {
        assert.equal(archived || fullInstall,false,'Explicit clean install skips partition probing');
        assert.equal(address,0x8000);
        assert.equal(size,0x1000);
        return {new:'98fd72a61b5952a549dd2a6f48b7c6fa',legacy:'9f56f6078cbd6aac217fb84f364246de',
                crosspoint:'c2c89587cec31dfd42c20bf837b3eb29'}[layout] || '0'.repeat(32);
      }
      assert.equal(size,written.get(address).length);
      return badMd5?'0'.repeat(32):hash('md5',written.get(address).subarray(0,size));
    }};
  const context=vm.createContext({document:{getElementById:get,querySelectorAll:s=>s==='.radio-card'?[]:radios},navigator:{serial:{}},
    console:{...console,warn(){},error(){}},alert:m=>notices.push(m),confirm:()=>{++confirms;return confirmClean;},
    crypto:webcrypto,TextDecoder,Uint8Array,DataView,Date,setTimeout,loader,feed,
    fetch:async url=>{
      if(url.includes('.json'))return {ok:true,json:async()=>feed};
      const name=url.split('/').pop().split('?')[0];
      const bytes=parts[name];assert(bytes,name);
      return {ok:true,arrayBuffer:async()=>Uint8Array.from(corrupt&&name==='storage-empty.bin'?Buffer.alloc(bytes.length):bytes).buffer};
    }});
  vm.runInContext(appJs,context);
  await new Promise(resolve=>setImmediate(resolve));
  vm.runInContext('esploader=loader; currentReleaseInfo=feed; editionReady=true;',context);
  get('risk-accepted').checked=true;
  await get('flash-btn').events.click();
  return {writes,confirms,notices,fullSize:written.get(0)?.length};
}
(async()=>{
  for (const love of [false,true]) {
    const update = await run({love});
    assert.equal(update.confirms,0);
    assert.deepEqual(update.writes,[[0x10000,0xd000]]);
    assert.ok(update.notices.some(message=>message.includes('thành công')));
    for (const options of [{fullInstall:true,layout:'legacy'},{fullInstall:true,layout:'crosspoint'},{archived:true},{fullInstall:true,layout:'unknown'}]) {
      const result = await run({love,...options});
      assert.equal(result.confirms,1);
      assert.deepEqual(result.writes,[[0]]);
      assert.equal(result.fullSize,0x1000000);
      assert.ok(result.notices.some(message=>message.includes('thành công')));
    }
  }
  for (const options of [{fullInstall:true,confirmClean:false},{fullInstall:true,corrupt:true},{layout:'legacy'},{layout:'crosspoint'},{layout:'unknown'},{wrongSize:true}])
    assert.deepEqual((await run(options)).writes,[]);
  const corruptedWrite = await run({badMd5:true});
  assert.ok(corruptedWrite.notices.some(message=>message.includes('Kiểm tra lại sau khi ghi thất bại')));
  console.log('PASS Web Flash: NVS256 app-only, 16 MiB clean install for legacy/CrossPoint, historical 1.4.2, cancel, integrity and device geometry');
})().catch(error=>{console.error(error);process.exitCode=1;});
