import {installTours} from './tour-controller.js?v=mobile-20261021';
const embedded=new URLSearchParams(location.search).has('embed');document.body.className=embedded?'embedded':'standalone';
const canvas=document.getElementById('screen'),ctx=canvas.getContext('2d',{alpha:false}),image=ctx.createImageData(684,1216),loading=document.getElementById('loading'),notice=document.getElementById('notice');
let worker=new Worker(new URL('./worker.js?v=mobile-20261021',import.meta.url),{type:'module'}),personalWorker=worker;let ready=false,tickInFlight=false,inView=true,volatileRetry=false,pressed=false,pendingMove=null,raf=0,noticeTimer,lastState,requestId=0;const requests=new Map(),readyCallbacks=[];let bootResolve,bootReject;
function command(value){if(ready)worker.postMessage({type:'command',command:value});}
function announce(text){notice.textContent=text;clearTimeout(noticeTimer);noticeTimer=setTimeout(()=>notice.textContent='',8000);}
function receive(event){if(event.currentTarget!==worker)return;const {data}=event;
  if(data.type==='frame'){lastState=data;
    if(data.id==='tick')tickInFlight=false;
    if(data.pixels){const bytes=new Uint8Array(data.pixels),rgba=image.data,r=data.rect||{x:0,y:0,width:684,height:1216};
    for(let y=r.y;y<r.y+r.height;y++)for(let x=r.x;x<r.x+r.width;x++){const px=1215-y,b=bytes[x*608+(px>>1)],v=((px&1)?b>>4:b&15)*17,i=(y*684+x)*4;rgba[i]=rgba[i+1]=rgba[i+2]=v;rgba[i+3]=255;}
    ctx.putImageData(image,0,0,r.x,r.y,r.width,r.height);}
    for(const [key,value]of Object.entries({view:JSON.stringify(data.view),quick:String(data.quick),page:data.page,locked:String(data.locked)}))if(canvas.dataset[key]!==value)canvas.dataset[key]=value;const label=data.locked?'已锁屏':data.page.split(' ')[0];if(document.getElementById('state').textContent!==label)document.getElementById('state').textContent=label;
    document.querySelectorAll('.tabs button').forEach((button,i)=>{const labels=['首页','图书','文件管理','设置'];if(data.page.startsWith(labels[i])&&!data.locked)button.setAttribute('aria-current','page');else button.removeAttribute('aria-current');});
  if(data.id&&requests.has(data.id)){requests.get(data.id).resolve(data);requests.delete(data.id);}
  }else if(data.type==='progress'){window.dispatchEvent(new CustomEvent('kiikoread:boot-progress',{detail:data.text}));}
  else if(data.type==='ready'){tickInFlight=false;ready=true;window.dispatchEvent(new Event('kiikoread:boot-ready'));loading.hidden=true;document.getElementById('version').textContent=data.version;document.querySelectorAll('button').forEach(b=>b.disabled=false);bootResolve?.();bootResolve=null;readyCallbacks.splice(0).forEach(fn=>fn());}
  else if(data.type==='notice')announce(data.text);
  else if(data.type==='error'){if(data.code==='STORAGE_UNAVAILABLE'&&worker===personalWorker&&!volatileRetry){volatileRetry=true;worker.terminate();worker=personalWorker=new Worker(new URL('./worker.js?v=mobile-20261021',import.meta.url),{type:'module'});attach(worker);worker.postMessage({type:'init',volatile:true});announce('暂时无法读取历史记录，本次为临时体验，不会改动原记录。');return;}window.dispatchEvent(new CustomEvent('kiikoread:boot-error',{detail:data.text}));loading.hidden=false;loading.classList.add('error');loading.querySelector('strong').textContent='暂时没有启动成功';document.getElementById('loading-detail').textContent=data.text;document.getElementById('retry').hidden=false;bootReject?.(new Error(data.text));}
  else if(data.type==='reset')location.reload();
}
function failed(event){if(event?.currentTarget!==worker)return;window.dispatchEvent(new CustomEvent('kiikoread:boot-error',{detail:'模拟器连接失败，请重新加载。'}));loading.hidden=false;loading.classList.add('error');loading.querySelector('strong').textContent='模拟器未能加载';document.getElementById('loading-detail').textContent='请重新加载后再试。';document.getElementById('retry').hidden=false;bootReject?.(new Error('固件实例加载失败'));}
function attach(w){w.addEventListener('message',receive);w.addEventListener('error',failed);}
attach(worker);
document.querySelectorAll('[data-command]').forEach(button=>button.addEventListener('click',()=>command(button.dataset.command)));
function point(event){const b=canvas.getBoundingClientRect();return [Math.max(0,Math.min(683,Math.round((event.clientX-b.left)*684/b.width))),Math.max(0,Math.min(1215,Math.round((event.clientY-b.top)*1216/b.height)))];}
canvas.addEventListener('pointerdown',event=>{if(!ready)return;pressed=true;canvas.focus({preventScroll:true});canvas.setPointerCapture(event.pointerId);command('down '+point(event).join(' '));});
canvas.addEventListener('pointermove',event=>{if(!pressed)return;pendingMove=point(event);if(!raf)raf=requestAnimationFrame(()=>{raf=0;if(pendingMove){command('move '+pendingMove.join(' '));pendingMove=null;}});});
function release(event){if(!pressed)return;pressed=false;if(raf){cancelAnimationFrame(raf);raf=0;}if(pendingMove){command('move '+pendingMove.join(' '));pendingMove=null;}command('up '+point(event).join(' '));}
canvas.addEventListener('pointerup',release);canvas.addEventListener('pointercancel',release);
canvas.addEventListener('keydown',event=>{const commands={ArrowLeft:'key 0',ArrowRight:'key 2',' ':'key 1',Escape:'page 0'};if(commands[event.key]){event.preventDefault();command(commands[event.key]);}});

document.getElementById('reset').onclick=()=>{if(ready&&confirm('重置这台浏览器里的体验设置、阅读进度和密码？'))worker.postMessage({type:'reset'});};
document.getElementById('import').onchange=async event=>{for(const file of event.target.files){if(file.size>20*1024*1024){announce('请选择 20 MB 以内的文件。');continue;}const bytes=await file.arrayBuffer();worker.postMessage({type:'file',name:file.name,bytes},[bytes]);}event.target.value='';};
document.getElementById('export').onclick=()=>{if(!ready)return;canvas.toBlob(blob=>{if(!blob)return;const url=URL.createObjectURL(blob),a=document.createElement('a');a.href=url;a.download='kiikoread-rc97-684x1216.png';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);});};
if(globalThis.IntersectionObserver)new IntersectionObserver(entries=>{inView=entries[0].isIntersecting;}).observe(canvas);
setInterval(()=>{if(ready&&!pressed&&!document.hidden&&inView&&!tickInFlight){tickInFlight=true;worker.postMessage({type:'command',command:'tick',id:'tick'});}},350);
document.addEventListener('visibilitychange',()=>{if(document.hidden&&worker===personalWorker)command('suspend');});
window.addEventListener('pagehide',()=>{if(worker===personalWorker)command('suspend');});
document.querySelectorAll('button').forEach(b=>{if(b.id!=='retry')b.disabled=true;});worker.postMessage({type:'init'});

function request(value){if(!ready)return Promise.reject(new Error('模拟器尚未就绪'));const id=++requestId;return new Promise((resolve,reject)=>{const timer=setTimeout(()=>{requests.delete(id);reject(new Error('操作未完成，请重试演示'));},10000);requests.set(id,{resolve:data=>{clearTimeout(timer);resolve(data);}});worker.postMessage({type:'command',command:value,id});});}
const guide=installTours({command:value=>worker!==personalWorker?request(value):Promise.reject(new Error("演示已结束")),state:()=>lastState,notice:announce,onReady:fn=>ready?fn():readyCallbacks.push(fn),begin:async isCurrent=>{if(worker===personalWorker&&ready)await request("suspend");if(!isCurrent())return;tickInFlight=false;bootResolve?.();bootResolve=bootReject=null;if(worker!==personalWorker)worker.terminate();ready=false;worker=new Worker(new URL('./worker.js?v=mobile-20261021',import.meta.url),{type:'module'});attach(worker);return new Promise((resolve,reject)=>{bootResolve=resolve;bootReject=reject;worker.postMessage({type:'init',demo:true});});},end:()=>{tickInFlight=false;bootResolve?.();bootResolve=bootReject=null;if(worker!==personalWorker)worker.terminate();worker=personalWorker;ready=true;loading.hidden=true;command('refresh');}});
document.getElementById('ask-guide').onclick=()=>{if(embedded){parent.postMessage({type:'kiikoread:ask-manual'},location.origin);}else{document.querySelector('.manual-launch')?.click();}};
if(!embedded){const style=document.createElement('link');style.rel='stylesheet';style.href='../assets/manual-helper.css';document.head.append(style);const search=document.createElement('script');search.src='../assets/manual-search.js';search.onload=()=>{const helper=document.createElement('script');helper.src='../assets/manual-helper.js';helper.dataset.index='../manual/manual-index.json';helper.dataset.manual='../manual/';document.head.append(helper);};document.head.append(search);}

document.addEventListener('kiikoread:start-guide',event=>{if(typeof event.detail?.id==='string')guide.start(event.detail.id);});
if(embedded){let heightRaf;new ResizeObserver(()=>{cancelAnimationFrame(heightRaf);heightRaf=requestAnimationFrame(()=>parent.postMessage({type:'kiikoread:simulator-height',height:Math.ceil(document.querySelector('.experience').getBoundingClientRect().height)+2},location.origin));}).observe(document.querySelector('.experience'));}
