/* Exercise the production click handler with simulated S31 flash. */
const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const {createHash, webcrypto} = require('node:crypto');
const appJs = fs.readFileSync('web/red-studio-app.js', 'utf8').replace(/^import[^\n]+\n/, '');
const html = fs.readFileSync('web/index.html', 'utf8');
function hash(kind, bytes) { return createHash(kind).update(bytes).digest('hex'); }
function identity(bytes) { return {size:bytes.length, sha256:hash('sha256',bytes), md5:hash('md5',bytes)}; }
function table(layout) {
  const bytes = Buffer.alloc(4096, 255);
  if (layout === 'blank') return bytes;
  const legacy = layout === 'legacy';
  const rows = [
    ['nvs',1,2,legacy?0x9000:0xfb0000,legacy?0x4000:0x40000],
    ['otadata',1,0,0xd000,0x2000], ['phy_init',1,1,0xf000,0x1000],
    ['ota_0',0,16,0x10000,0x500000], ['ota_1',0,17,0x510000,0x500000],
    ['storage',1,129,0xa10000,legacy?0x5e0000:0x5a0000], ['coredump',1,3,0xff0000,0x10000]
  ];
  if (layout === 'rc') rows.push(['nvs_old',1,2,0x9000,0x4000]);
  if (layout === 'unknown') rows[0][3]=0x19000;
  rows.forEach(([name,kind,sub,off,size],i)=>{
    const at=i*32; bytes.fill(0,at,at+32); bytes.writeUInt16LE(0x50aa,at);
    bytes[at+2]=kind; bytes[at+3]=sub; bytes.writeUInt32LE(off,at+4);
    bytes.writeUInt32LE(size,at+8); bytes.write(name,at+12);
  });
  return bytes;
}
class Node {
  constructor(){this.checked=false;this.disabled=false;this.style={};this.events={};this.files=[];this.value='';this.classList={toggle(){}};this.options=[{value:''},{value:'1.4.1-vn'}];}
  addEventListener(k,fn){this.events[k]=fn;} focus(){}
}
async function run(layout, {confirmClean=true, corrupt=false, badMd5=false, wrongSize=false, love=false, probeTimeout=false, forceClean=false}={}) {
  const nodes=new Map([...html.matchAll(/\bid="([^"]+)"/g)].map(m=>[m[1],new Node()]));
  const get=id=>nodes.get(id), radios=['mode-full','mode-app','mode-custom'].map(get);
  const version=love?'1.4.2-love-vn':'1.4.2-vn';
  get('firmware-edition').value=love?'love-book':'full';
  // A stale radio choice from history must never change automatic layout behavior.
  get('mode-app').checked=true;
  let app=Buffer.alloc(512);app[0]=0xe9;app[12]=32;app.writeUInt32LE(0xabcd5432,32);
  app.write(version,48);app.write('Kiikoread_Metalio_S31',80);
  const boot=Buffer.alloc(256);boot[0]=0xe9;boot[12]=32;
  let parts={'partitions.bin':table('new').subarray(0,0xc00),'storage-empty.bin':Buffer.alloc(0x5a0000,255),'bootloader.bin':boot};
  if (forceClean || layout === 'crosspoint') {
    const folder = love ? 'releases/love-book' : 'releases';
    app = fs.readFileSync(folder+'/web-firmware.bin');
    parts = Object.fromEntries(Object.keys(parts).map(name => [name,fs.readFileSync(folder+'/'+name)]));
  }
  get('clean-install').checked = forceClean;
  const feed={version,usb_only:true,...identity(app),usb_parts:Object.fromEntries(Object.entries(parts).map(([n,b])=>[n,identity(b)]))};
  const written=new Map(), writes=[], notices=[];
  let confirms=0;
  const loader={IS_STUB:true,chip:{CHIP_NAME:'ESP32-S31'},detectFlashSize:async()=>wrongSize?'8MB':'16MB',
    readFlash:async()=>{throw Error("Unexpected streamed flash read");},
    writeFlash:async({fileArray,eraseAll})=>{assert.equal(eraseAll,false);writes.push(Array.from(fileArray,x=>x.address));for(const x of fileArray)written.set(x.address,Buffer.from(x.data));},
    flashMd5sum:async(address,size)=>{
      if (!writes.length) {
        assert.equal(forceClean,false,'Explicit clean install must skip partition probing');
        assert.equal(address,0x8000);assert.equal(size,4096);
        if (probeTimeout) throw Error('No serial data received.');
        return {crosspoint:'c2c89587cec31dfd42c20bf837b3eb29',new:'98fd72a61b5952a549dd2a6f48b7c6fa',legacy:'9f56f6078cbd6aac217fb84f364246de',rc:'c8d9169209a600d053b83659b33e0bb2',blank:'6ae59e64850377ee5470c854761551ea'}[layout]||'f'.repeat(32);
      }
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
  assert.deepEqual((await run('new',{love:true})).writes,[[0x10000,0xd000]]);
  assert.deepEqual((await run('legacy')).writes,[[0x10000,0xd000,0x9000,0xa10000,0xfb0000],[0x8000]]);
  assert.deepEqual((await run('blank')).writes,[[0x10000,0xd000,0x9000,0xa10000,0xfb0000,0x2000],[0x8000]]);
  const crosspoint = await run('crosspoint');
  assert.equal(crosspoint.confirms,1);
  for (const result of [crosspoint, await run('unknown',{forceClean:true,love:true})]) {
    assert.deepEqual(result.writes,[[0]]);
    assert.equal(result.fullSize,0x1000000);
    assert.ok(result.notices.some(message=>message.includes('thành công')));
  }
  assert.deepEqual((await run('rc')).writes,[[0x10000,0xd000,0x9000,0xa10000,0xfb0000],[0x8000]]);
  for(const [layout,options] of [['unknown',{forceClean:true,wrongSize:true}],['unknown',{forceClean:true,confirmClean:false}],['crosspoint',{confirmClean:false}],['legacy',{confirmClean:false}],['legacy',{corrupt:true}],['unknown',{}],['legacy',{wrongSize:true}],['legacy',{probeTimeout:true}]])
    assert.equal((await run(layout,options)).writes.length,0);
  assert.equal((await run('legacy',{badMd5:true})).writes.length,1,'Failed payload check must stop before publishing new partitions');
  console.log('PASS Web Flash: legacy/blank/RC/CrossPoint migration, preserve NVS256, Love Book, cancellation, corruption, wrong layout/size, partition table last');
})().catch(error=>{console.error(error);process.exitCode=1;});
