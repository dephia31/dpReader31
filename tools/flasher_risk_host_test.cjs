/* SPDX-License-Identifier: Apache-2.0
 * Exercise the production flasher without a USB device or firmware downloads.
 */
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const html=fs.readFileSync('web/index.html','utf8');
assert.match(html, /id="mode-app" checked/);
assert.doesNotMatch(html, /id="mode-full" checked/);
assert.doesNotMatch(html.match(/<section class="risk-agreement"[\s\S]*?<\/section>/)[0], /\bhidden\b/);
class Node {
 constructor(){this.checked=false;this.disabled=false;this.style={};this.events={};this.files=[];this.value='';this.classList={toggle(){}};this.options=[{value:''},{value:'1.2.4-vn'}];}
 addEventListener(k,fn){this.events[k]=fn} focus(){this.focused=true}
}
const nodes=new Map([...html.matchAll(/\bid="([^"]+)"/g)].map(m=>[m[1],new Node()]));
const get=id=>nodes.get(id);get('mode-full').checked=true;get('baud-rate').value='460800';
const radios=['mode-full','mode-app','mode-custom'].map(get),cards=radios.map(n=>({classList:{toggle(){}},querySelector:()=>n}));
let downloads=0,writes=0;const messages=[];
const ctx=vm.createContext({document:{getElementById:get,querySelectorAll:s=>s==='.radio-card'?cards:radios},navigator:{serial:{}},console:{...console,warn(){}},alert:s=>messages.push(s),confirm:()=>true,fetch:async()=>{downloads++;throw Error('No network in host test')},setTimeout,Uint8Array,Date});
vm.runInContext(fs.readFileSync('web/red-studio-app.js','utf8').replace(/^import[^\n]+\n/,''),ctx);
const run=s=>vm.runInContext(s,ctx);
(async()=>{
 await new Promise(resolve=>setImmediate(resolve));
 assert.equal(get('flash-btn').disabled,true);
 get('risk-accepted').checked=true;get('risk-accepted').events.change();assert.equal(get('flash-btn').disabled,true,'consent alone cannot enable writes');
 run('esploader = {writeFlash:async()=>{}}');run('updateFlashReadiness()');assert.equal(get('flash-btn').disabled,true);
 run('editionReady = true; updateFlashReadiness()');assert.equal(get('flash-btn').disabled,false);
 get('risk-accepted').checked=false;get('risk-accepted').events.change();assert.equal(get('flash-btn').disabled,true);
 const before=downloads;await get('flash-btn').events.click();assert.equal(downloads,before,'unchecked handler must not download or write');assert.equal(get('risk-accepted').focused,true);
 get('risk-accepted').checked=true;get('mode-custom').checked=true;get('mode-full').checked=false;get('mode-custom').events.change();assert.equal(get('risk-accepted').checked,false,'target change invalidates consent');
 get('release-version').value='1.2.4-vn';assert.equal(run('selectedReleaseFolder()'),'releases/1.2.4-vn');get('risk-accepted').checked=true;get('release-version').events.change();assert.equal(get('risk-accepted').checked,false,'version change invalidates consent');
 get('risk-accepted').checked=true;get('custom-file').events.change();assert.equal(get('risk-accepted').checked,false,'file change invalidates consent');
 // An archived app-only write must be refused on an NVS256 device.
 get('mode-custom').checked=false;get('mode-app').checked=true;
 run('esploader={chip:{CHIP_NAME:"ESP32-S31"},readFlash:async()=>new Uint8Array(4096),writeFlash:async()=>{throw Error("unexpected write")}}');
 assert.equal(run('classifyS31Partitions(new Uint8Array(4096))'),'unknown');
 get('release-version').value='';
 assert.equal(run('automaticLayoutSelected()'),true);
 console.log('flasher risk gate: PASS (consent, target/file changes, automatic release, unknown partition rejection)');
})().catch(e=>{console.error(e);process.exitCode=1});
