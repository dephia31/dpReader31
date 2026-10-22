const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
(async()=>{const browser=await chromium.launch({...(process.env.CHROME_PATH?{executablePath:process.env.CHROME_PATH}:{}),headless:true});
try{const page=await browser.newPage();await page.goto((process.env.SIMULATOR_URL||'http://127.0.0.1:8880/')+'version.json');
const result=await page.evaluate(async()=>{
 let w,seq=0,pending=new Map();
 async function start(){w=new Worker('./worker.js',{type:'module'});await new Promise((resolve,reject)=>{w.onmessage=e=>{if(e.data.type==='ready')resolve();if(e.data.type==='error')reject(new Error(e.data.text));if(e.data.type==='frame'&&e.data.id){pending.get(e.data.id)?.(e.data);pending.delete(e.data.id);}};w.postMessage({type:'init'});});}
 const wait=ms=>new Promise(r=>setTimeout(r,ms));
 function cmd(command){const id=++seq;return new Promise(resolve=>{pending.set(id,resolve);w.postMessage({type:'command',command,id});});}
 async function db(){return new Promise((resolve,reject)=>{const q=indexedDB.open('/Data/nvs');q.onerror=reject;q.onsuccess=()=>{const d=q.result,t=d.transaction('FILE_DATA','readonly'),s=t.objectStore('FILE_DATA'),a=s.getAll(),k=s.getAllKeys();t.oncomplete=()=>{const out=Object.fromEntries(k.result.map((key,i)=>[key,Array.from(a.result[i].contents||[])]));d.close();resolve(out);};};});}
 const pin=async()=>{let last;for(const [x,y]of [[158,544],[342,544],[526,544],[158,703]])last=await cmd(`tap ${x} ${y}`);return last;};
 await start();await cmd('open /sdcard/books/海边的信.txt');for(let i=0;i<3;i++)await cmd('key 2');await cmd('key 1');await cmd('tap 205 1140');await cmd('page 3');
 let state=await cmd('tick');for(let i=0;state.targets.settings[6]>1040&&i<5;i++){await cmd('down 350 950');await cmd('move 350 450');await cmd('up 350 450');await wait(400);state=await cmd('tick');}
 state=await cmd(`tap 300 ${state.targets.settings[6]}`);if(state.view.settings!==21)throw new Error('PIN entry mismatch');await cmd('tap 300 440');await pin();await pin();await cmd('page 0');await wait(850);
 const before=await db();await cmd('power');state=await cmd('power');if(!state.locked)throw new Error('PIN must lock');if(!(await cmd('page 1')).locked)throw new Error('Navigation bypassed lock');for(let i=0;i<4;i++)state=await cmd('tap 342 1021');if(!state.locked)throw new Error('Wrong PIN accepted');if((await pin()).locked)throw new Error('Correct PIN failed');await wait(700);w.terminate();
 await start();await cmd('power');if(!(await cmd('power')).locked)throw new Error('PIN not restored');if((await pin()).locked)throw new Error('Stored PIN cannot unlock');await cmd('page 0');await wait(700);const after=await db();w.terminate();
 const paths=Object.keys(before).filter(p=>/\/(rp_books|rp_marks|pico_lock)\//.test(p));return {before:Object.fromEntries(paths.map(p=>[p,before[p]])),after:Object.fromEntries(paths.map(p=>[p,after[p]]))};
});assert(Object.keys(result.before).some(k=>k.includes('/pico_lock/credential')));assert(Object.keys(result.before).some(k=>k.includes('/rp_marks/')));assert.deepEqual(result.after,result.before);const report={passed:true,checks:['Browser IDBFS saves progress and bookmark bytes','Four digit PIN rejects wrong input and navigation bypass','Worker restart restores hashed PIN and exact progress/bookmark bytes']};fs.writeFileSync(path.join(__dirname,'../build/web/persistence-verification.json'),JSON.stringify(report,null,2));console.log(report);
}finally{await browser.close();}})().catch(e=>{console.error(e);process.exit(1)});
