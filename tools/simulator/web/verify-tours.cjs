const {chromium}=require('playwright');
const fs=require('node:fs'),path=require('node:path');
(async()=>{const {tours}=await import('./guides.js');const b=await chromium.launch({...(process.env.CHROME_PATH?{executablePath:process.env.CHROME_PATH}:{}),headless:true});const results=[];
try{const p=await b.newPage({viewport:{width:1100,height:1100}});for(const [key,tour] of Object.entries(tours)){
  const record={key,steps:tour.steps.length,passed:false};try{
    await p.goto(`${process.env.SIMULATOR_URL||'http://127.0.0.1:8880/'}?embed=1&guide=${key}`,{waitUntil:'load'});
    await p.locator('#guide-next').waitFor({state:'visible',timeout:20000});await p.waitForFunction(()=>!document.querySelector('#guide-next').disabled,{},{timeout:20000});
    if(await p.locator('#guide-play').innerText()==='暂停')await p.locator('#guide-play').click();
    for(let i=0;i<tour.steps.length-1;i++){
      await p.locator('#guide-next').click();
      await p.waitForFunction(index=>document.querySelector('#guide-step').textContent.startsWith(`${index+1} /`)&&!document.querySelector('#guide-next').disabled,i+1,{timeout:15000});
    }
    record.passed=true;if(['reading-size','pin-enable','home-component','shelf-style'].includes(key))await p.screenshot({path:path.join(__dirname,`../build/web/tour-${key}.png`)});
  }catch(error){record.error=error.message.split('\n')[0];record.notice=await p.locator('#notice').innerText().catch(()=>null);record.step=await p.locator('#guide-step').innerText().catch(()=>null);record.view=await p.locator('#screen').getAttribute('data-view').catch(()=>null);await p.screenshot({path:path.join(__dirname,`../build/web/tour-failed-${key}.png`)});}
  results.push(record);console.log(JSON.stringify(record));
}fs.writeFileSync(path.join(__dirname,'../build/web/tour-verification.json'),JSON.stringify(results,null,2));if(results.some(x=>!x.passed))process.exitCode=1;}finally{await b.close();}})().catch(e=>{console.error(e);process.exit(1)});
