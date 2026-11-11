/* Exercise the production click handler with simulated S31 flash. */
const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const {createHash, webcrypto} = require('node:crypto');
const appJs = fs.readFileSync('web/red-studio-app.js', 'utf8').replace(/^import[^\n]+\n/, '');
const html = fs.readFileSync('web/index.html', 'utf8');
function hash(kind, bytes) { return createHash(kind).update(bytes).digest('hex'); }
function identity(bytes) { return {size:bytes.length, sha256:hash('sha256',bytes), md5:hash('md5',bytes)}; }
class Node {
  constructor(){this.checked=false;this.disabled=false;this.style={};this.events={};this.files=[];this.value='';this.classList={toggle(){}};this.options=[{value:''},{value:'1.4.1-vn'}];}
  addEventListener(k,fn){this.events[k]=fn;} focus(){}
}
async function run({confirmClean=true, corrupt=false, badMd5=false, wrongSize=false, love=false}={}) {
  const nodes=new Map([...html.matchAll(/\bid="([^"]+)"/g)].map(m=>[m[1],new Node()]));
  const get=id=>nodes.get(id), radios=['mode-full','mode-app','mode-custom'].map(get);
  const version=love?'1.4.2-love-vn':'1.4.2-vn';
  get('firmware-edition').value=love?'love-book':'full';
  // A stale radio choice from history must never change automatic layout behavior.
  get('mode-app').checked=true;
  const folder = love ? 'releases/love-book' : 'releases';
  const app = fs.readFileSync(folder+'/web-firmware.bin');
  const parts = Object.fromEntries(['partitions.bin','storage-empty.bin','bootloader.bin']
    .map(name => [name,fs.readFileSync(folder+'/'+name)]));
  const feed={version,usb_only:true,...identity(app),usb_parts:Object.fromEntries(Object.entries(parts).map(([n,b])=>[n,identity(b)]))};
  const written=new Map(), writes=[], notices=[];
  let confirms=0;
  const loader={IS_STUB:true,chip:{CHIP_NAME:'ESP32-S31'},detectFlashSize:async()=>wrongSize?'8MB':'16MB',
    readFlash:async()=>{throw Error("Unexpected streamed flash read");},
    writeFlash:async({fileArray,eraseAll})=>{assert.equal(eraseAll,false);writes.push(Array.from(fileArray,x=>x.address));for(const x of fileArray)written.set(x.address,Buffer.from(x.data));},
    flashMd5sum:async(address,size)=>{
      assert.ok(writes.length,'1.4.2 clean install must skip partition probing');
      assert.equal(address,0);
      assert.equal(size,0x1000000);
      return badMd5?'0'.repeat(32):hash('md5',written.get(address).subarray(0,size));
    }};
  const context=vm.createContext({document:{getElementById:get,querySelectorAll:s=>s==='.radio-card'?[]:radios},navigator:{serial:{}},
    console:{...console,warn(){},error(){}},alert:m=>notices.push(m),confirm:()=>{++confirms;return confirmClean;},
    crypto:webcrypto,TextDecoder,Uint8Array,DataView,Date,setTimeout,loader,feed,
    fetch:async url=>{
      if(url.includes('.json'))return {ok:true,json:async()=>feed};
      const name=url.split('/').pop().split('?')[0];
      const bytes=name==='web-firmware.bin'?app:parts[name];assert(bytes,name);
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
    const result = await run({love});
    assert.equal(result.confirms,1);
    assert.deepEqual(result.writes,[[0]]);
    assert.equal(result.fullSize,0x1000000);
    assert.ok(result.notices.some(message=>message.includes('thành công')));
  }
  for (const options of [{confirmClean:false},{corrupt:true},{wrongSize:true}])
    assert.deepEqual((await run(options)).writes,[]);
  const corruptedWrite = await run({badMd5:true});
  assert.deepEqual(corruptedWrite.writes,[[0]]);
  assert.ok(corruptedWrite.notices.some(message=>message.includes('Kiểm tra lại sau khi ghi thất bại')));
  console.log('PASS Web Flash: 16 MiB single write at 0x0 for Full and Love Book, cancel, invalid parts and flash checksum');
})().catch(error=>{console.error(error);process.exitCode=1;});
