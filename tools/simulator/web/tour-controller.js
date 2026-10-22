import {tours,byTopic} from './guides.js';
// 操作演示使用另一台临时固件实例；关闭后返回用户原来的实例。/ Tours run in a separate temporary firmware instance.
export function installTours(api){
  const panel=document.getElementById('guide'),marker=document.getElementById('guide-marker'),next=document.getElementById('guide-next'),prev=document.getElementById('guide-prev'),play=document.getElementById('guide-play');
  let tour,index=0,playing=false,timer,busy=false,epoch=0;
  const wait=ms=>new Promise(resolve=>setTimeout(resolve,ms));
  const send=(command,token)=>token===epoch?api.command(command):Promise.reject(new Error('演示已切换'));
  const move=async(x,y,xx,yy,token=epoch)=>{await send(`down ${x} ${y}`,token);for(let i=1;i<=4;i++){await wait(65);await send(`move ${Math.round(x+(xx-x)*i/4)} ${Math.round(y+(yy-y)*i/4)}`,token);}await send(`up ${xx} ${yy}`,token);await wait(400);await send('tick',token);};
  async function pointFor(target,token=epoch){
    if(token!==epoch)throw new Error("演示已切换");
    if(Array.isArray(target))return target;
    if(!target)return null;
    const [type,key]=target.split(':'),n=Number(key);
    if(type==='reader')return api.state().targets.reader[n];
    if(type==='setting'){
      for(let i=0;i<7;i++){
        const y=api.state().targets.settings[n];
        if(y>=190&&y<=1040)return[360,y];
        if(y>1040)await move(350,960,350,440,token);else await move(350,420,350,940,token);
      }
      throw new Error('没有定位到该设置项');
    }
    return null;
  }
  async function perform(item,token=epoch){
    const action=item.action;
    if(typeof action==='string')await send(action,token);
    else if(action?.swipe)await move(...action.swipe,token);
    else if(action?.hold){const[x,y]=action.hold;await send(`down ${x} ${y}`,token);await wait(600);await send(`move ${x} ${y}`,token);await send(`up ${x} ${y}`,token);}
    else if(action&&'setting'in action){const p=await pointFor(`setting:${action.setting}`,token);await send(`tap ${p[0]} ${p[1]}`,token);}
    else if(action&&'reader'in action){const p=await pointFor(`reader:${action.reader}`,token);await send(`tap ${p[0]} ${p[1]}`,token);}
    for(let i=0;api.state().busy&&i<30;i++){await wait(60);await send("tick",token);}
    if(token!==epoch)throw new Error("演示已切换");
    if(item.expect){
      const state=api.state();
      for(const[key,value]of Object.entries(item.expect)){
        const actual=key==='page'?state.page:key==='quick'?state.quick:state.view[key];
        if(key==='page'?!actual.startsWith(value):actual!==value)throw new Error('当前界面与预期步骤不一致，已暂停演示。');
      }
    }
  }
  function playback(){clearTimeout(timer);play.textContent=playing?'暂停':'自动播放';if(playing&&!busy&&index<tour.steps.length-1)timer=setTimeout(()=>advance(),1800);}
  async function show(){
    const token=epoch;busy=true;next.disabled=prev.disabled=true;marker.hidden=true;
    const item=tour.steps[index];document.getElementById('guide-step').textContent=`${index+1} / ${tour.steps.length} · ${item.text}`;
    try{
      const point=await pointFor(item.target,token);if(token!==epoch)return;
      if(point){marker.style.left=(point[0]/684*100)+'%';marker.style.top=(point[1]/1216*100)+'%';marker.hidden=false;}
      next.textContent=index===tour.steps.length-1?'返回自由体验':'下一步';prev.disabled=index===0;next.disabled=false;
    }catch(error){if(token!==epoch)return;api.notice(error.message);playing=false;}
    finally{if(token===epoch){busy=false;playback();}}
  }
  async function advance(){
    if(busy||!tour)return;
    if(index===tour.steps.length-1){close();return;}
    const token=epoch;busy=true;next.disabled=prev.disabled=true;clearTimeout(timer);
    try{await perform(tour.steps[index],token);if(token!==epoch)return;index++;if(index===tour.steps.length-1)playing=false;await show();}
    catch(error){if(token!==epoch)return;playing=false;busy=false;marker.hidden=true;api.notice(error.message);next.disabled=false;prev.disabled=index===0;playback();}
  }
  async function start(topic,backTo=0,autoplay=true){
    const selected=tours[byTopic[topic]||topic];if(!selected)return false;
    const token=++epoch;clearTimeout(timer);tour=selected;index=0;playing=autoplay;busy=true;marker.hidden=true;
    panel.hidden=false;document.body.classList.add('demonstrating');document.getElementById('guide-title').textContent=selected.title;
    document.getElementById('guide-step').textContent='正在准备独立演示…';
    try{await api.begin(()=>token===epoch);if(token!==epoch)return false;for(let i=0;i<backTo;i++){await pointFor(tour.steps[i].target,token);await perform(tour.steps[i],token);}index=backTo;await show();panel.scrollIntoView({behavior:'smooth',block:'nearest'});return true;}
    catch(error){if(token!==epoch)return false;playing=false;busy=false;api.notice('演示暂未启动：'+error.message);return false;}
  }
  function close(){++epoch;clearTimeout(timer);playing=false;tour=null;panel.hidden=marker.hidden=true;document.body.classList.remove('demonstrating');api.end();}
  next.onclick=()=>{playing=false;advance();};prev.onclick=()=>{if(!busy&&index>0){const key=Object.keys(tours).find(k=>tours[k]===tour);start(key,index-1,false);}};
  play.onclick=()=>{playing=!playing;playback();};document.getElementById('guide-close').onclick=close;
  window.addEventListener('message',event=>{if(event.origin!==location.origin||event.source!==parent)return;if(event.data?.type==='kiikoread:guide'&&typeof event.data.id==='string')start(event.data.id);});
  const topic=new URLSearchParams(location.search).get('guide');if(topic)api.onReady(()=>start(topic));
  return {start,active:()=>!!tour};
}
