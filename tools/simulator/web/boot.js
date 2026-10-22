// 中文：先显示可恢复的加载状态，再下载界面模块。/ Show recoverable startup feedback before loading modules.
(()=>{const layer=document.getElementById('loading'),detail=document.getElementById('loading-detail'),retry=document.getElementById('retry');let timer,complete=false;
const fail=text=>{if(complete)return;clearTimeout(timer);layer.hidden=false;layer.classList.add('error');layer.querySelector('strong').textContent='加载暂时没有完成';detail.textContent=text;retry.hidden=false;retry.disabled=false;};
retry.onclick=()=>{const url=new URL(location.href);url.searchParams.set('retry',Date.now());location.replace(url);};
function watch(){clearTimeout(timer);timer=setTimeout(()=>fail('连接等待过久，请检查网络后重试。'),30000);}
window.addEventListener('kiikoread:boot-progress',e=>{if(complete)return;detail.textContent=e.detail;watch();});
window.addEventListener('kiikoread:boot-ready',()=>{complete=true;clearTimeout(timer);layer.hidden=true;});
window.addEventListener('kiikoread:boot-error',e=>fail(e.detail));
if(!globalThis.WebAssembly||!globalThis.Worker){fail('当前浏览器不支持模拟器，请更新 Safari 后重试。');return;}
watch();import('./simulator.js?v=mobile-20261021').catch(()=>fail('界面资源下载失败，请点重新加载。'));
})();
