// 中文：压缩资源并行加载；不变画面不传像素。/ Load compressed resources in parallel and omit unchanged pixels.
let core,ready=false,saved,demonstration=false,persistent=false,lastPixels;
const version='0.3.3-rc97',started=performance.now();
function progress(text){postMessage({type:'progress',text,elapsed:Math.round(performance.now()-started)});}
async function download(file,label,expectedSize){
 const controller=new AbortController();let timeout=setTimeout(()=>controller.abort(),25000);
 try{const response=await fetch(new URL(file,import.meta.url),{signal:controller.signal,cache:'force-cache'});if(!response.ok)throw new Error(label+'下载失败（'+response.status+'）');
  const total=expectedSize||Number(response.headers.get('content-length'))||0,parts=[];let size=0,lastProgress=0;
  if(!response.body?.getReader)return await response.arrayBuffer();
  const reader=response.body.getReader();while(true){const {done,value}=await reader.read();if(done)break;clearTimeout(timeout);timeout=setTimeout(()=>controller.abort(),25000);parts.push(value);size+=value.length;if(performance.now()-lastProgress>100||size===total){lastProgress=performance.now();progress(label+(total?' · '+Math.min(100,Math.round(size/total*100))+'%':' · 已接收 '+Math.round(size/1024)+' KB'));}}
  const bytes=new Uint8Array(size);let offset=0;for(const part of parts){bytes.set(part,offset);offset+=part.length;}return bytes.buffer;
 }catch(error){if(error.name==='AbortError')throw new Error(label+'下载等待过久，请检查网络后重试。');throw error;}finally{clearTimeout(timeout);}
}
function frame(id){
 const start=core._sim_frame(),pixels=core.HEAPU8.subarray(start,start+1216*684/2);let rect=null;
 if(!lastPixels){lastPixels=new Uint8Array(pixels.length);rect={x:0,y:0,width:684,height:1216};}
 else{let x0=684,x1=-1,y0=1216,y1=-1;for(let i=0;i<pixels.length;i++)if(pixels[i]!==lastPixels[i]){const x=(i/608)|0,y=1214-(i%608)*2;x0=Math.min(x0,x);x1=Math.max(x1,x);y0=Math.min(y0,y);y1=Math.max(y1,y+1);}if(x1>=0)rect={x:x0,y:y0,width:x1-x0+1,height:y1-y0+1};}
 let packed;if(rect){lastPixels.set(pixels);packed=pixels.slice().buffer;}
 postMessage({type:'frame',id,rect,targets:{settings:Array.from({length:11},(_,i)=>core._sim_settings_target(i)),reader:Array.from({length:6},(_,i)=>{const v=core._sim_reader_target(i);return [v>>>16,v&65535];})},pixels:packed,busy:!!core._sim_book_busy(),page:core.UTF8ToString(core._sim_page()),view:{settings:core._sim_settings_view(),book:core._sim_book_view(),panel:core._sim_book_panel(),home:core._sim_home_view()},locked:!!core._sim_locked(),quick:!!core._sim_quick_open(),version},packed?[packed]:[]);
}
function save(){if(!persistent)return;clearTimeout(saved);saved=setTimeout(()=>core.FS.syncfs(false,error=>{if(error)postMessage({type:'notice',text:'浏览器未允许保存，下次打开将重新体验。'});}),400);}
async function initialize(options={}){
 demonstration=!!options.demo;progress('正在下载固件与阅读资源…');
 const runtime=await fetch(new URL('./runtime.json?v=mobile-20261021',import.meta.url),{cache:'no-cache'}).then(r=>{if(!r.ok)throw new Error('资源清单未能加载');return r.json();});
 const [factory,wasm,data]=await Promise.all([import(new URL(runtime.module,import.meta.url).href),download(runtime.wasm,'固件程序',runtime.wasmSize),download(runtime.data,'字体与示例书',runtime.dataSize)]);
 progress('资源已就绪，正在启动阅读引擎…');
 core=await factory.default({wasmBinary:wasm,getPreloadedPackage:()=>data,locateFile:name=>new URL(name==='pico.wasm'?runtime.wasm:name==='pico.data'?runtime.data:name,import.meta.url).href,print:()=>{},printErr:message=>console.warn(message)});
 core.FS.mkdirTree('/Data/nvs');core.FS.mkdirTree('/Data/flash');core.FS.mkdirTree('/Data/card/fonts');core.FS.mkdirTree('/Data/card/pictures');
 if(!demonstration&&!options.volatile){
  progress('正在恢复这台浏览器的阅读记录…');let timeout;
  try{core.FS.mount(core.IDBFS,{},'/Data/nvs');await Promise.race([new Promise((resolve,reject)=>core.FS.syncfs(true,error=>error?reject(error):resolve())),new Promise((_,reject)=>{timeout=setTimeout(()=>reject(new Error('storage-timeout')),2500);})]);persistent=true;}
  catch(error){const failure=new Error('浏览器暂时无法读取记录，将进入临时体验。');failure.code='STORAGE_UNAVAILABLE';throw failure;}
  finally{clearTimeout(timeout);}
 }
 progress('正在绘制首页…');core._sim_assets_init();core._sim_seed();core._sim_init();frame();ready=true;postMessage({type:'ready',version,elapsed:Math.round(performance.now()-started),persistent});
}
onmessage=async ({data})=>{
  try{
    if(data.type==='init'){await initialize(data);return;}
    if(!ready)return;
    if(data.type==='command'){
      if(typeof data.command!=='string'||data.command.length>1024)return;
      core.ccall('sim_command',null,['string'],[data.command]);frame(data.id);
      if(data.command!=='tick'&&!/^(down|move) /.test(data.command))save();
    }else if(data.type==='file'){
      const name=data.name.replace(/[\/\\\x00-\x1f]/g,'_');
      if(!name||data.bytes.byteLength>20*1024*1024)throw new Error('文件过大，请选择 20 MB 以内的文件。');
      const font=/\.(ttf|otf)$/i.test(name),book=/\.(txt|epub)$/i.test(name);
      if(!font&&!book)throw new Error('支持 EPUB、TXT、TTF 和 OTF 文件。');
      const directory=font?'fonts':'books';
      if(core._sim_locked())throw new Error('请先解锁模拟器。');
      let unique=name,suffix=1;
      while(core.FS.analyzePath(`/Data/card/${directory}/${unique}`).exists)unique=name.replace(/(\.[^.]+)$/,` (${suffix++})$1`);
      core.FS.writeFile(`/Data/card/${directory}/${unique}`,new Uint8Array(data.bytes));
      core.ccall('sim_command',null,['string'],[font?`font /sdcard/fonts/${unique}`:'rescan']);frame();save();
      postMessage({type:'notice',text:`已导入 ${unique}，文件仅在本次浏览器会话内使用。`});
    }else if(data.type==='reset'){
      core._sim_shutdown();
      function remove(path){for(const item of core.FS.readdir(path)){if(item==='.'||item==='..')continue;const p=`${path}/${item}`;if(core.FS.isDir(core.FS.stat(p).mode)){remove(p);core.FS.rmdir(p);}else core.FS.unlink(p);}}
      remove('/Data/nvs');await new Promise(resolve=>core.FS.syncfs(false,()=>resolve()));postMessage({type:'reset'});
    }
  }catch(error){postMessage({type:ready?'notice':'error',code:error.code,text:error.message||'模拟器启动失败，请重试。'});}
};
