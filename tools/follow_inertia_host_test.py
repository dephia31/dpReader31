#!/usr/bin/env python3
"""中文：真实列表惯性的边界、时序及打断测试。/ English: Real list inertia bounds, timing and interruption tests."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
CODE = r'''
#include <assert.h>
#include <stdio.h>
#include "ui_follow_scroll.h"
static ui_follow_scroll_t s;
static cst836u_touch_t touch;
static app_ctx_t ctx;
static int offset;
static bool handled;
static bool feed(int y,int count,bool press,int ms){
 touch=(cst836u_touch_t){.x=300,.y=y,.count=count,.touched=count>0};
 ctx=(app_ctx_t){.touch=&touch,.pressed=press,.released=count==0,.now_ms=ctx.now_ms+ms};
 return ui_follow_scroll_feed(&s,&ctx,160,1096,&offset,5000,&handled);
}
static void launch(void){
 ui_follow_scroll_reset(&s);offset=1000;ctx.now_ms=1000;
 assert(!feed(800,1,true,10));assert(feed(760,1,false,20));assert(feed(700,1,false,20));
 feed(700,0,false,10);assert(s.coasting&&s.velocity>0&&handled);
}
static int coast_result(int dt){
 ui_follow_scroll_reset(&s);offset=1000;ctx.now_ms=1000;
 s.coasting=s.settle_pending=true;s.velocity=1800;s.coast_ms=s.last_ms=s.coast_start_ms=1000;
 int prior=offset,velocity=s.velocity;
 while(s.coasting){feed(700,0,false,dt);assert(offset>=prior&&offset<=5000&&s.velocity<=velocity);prior=offset;velocity=s.velocity;}
 assert(ctx.now_ms<1900&&!ui_follow_scroll_take_settle(&s,&ctx));ctx.now_ms=s.settle_ms;
 assert(ui_follow_scroll_take_settle(&s,&ctx)&&!ui_follow_scroll_take_settle(&s,&ctx));return offset;
}
int main(void){
 int result=coast_result(1);int steps[]={8,16,33,66};
 for(unsigned i=0;i<sizeof(steps)/sizeof(steps[0]);++i)assert(abs(coast_result(steps[i])-result)<=8);
 launch();int before=offset;assert(feed(700,0,false,10)&&offset>before);
 assert(!feed(600,1,true,10)&&!s.coasting&&s.tracking&&handled);
 assert(!feed(600,0,false,10)&&handled&&!s.tracking);
 launch();assert(!feed(600,1,true,10));before=offset;assert(feed(580,1,false,20)&&offset==before+20);
 launch();assert(!feed(100,1,true,10)&&!s.coasting&&!handled&&!s.tracking);
 for(int kind=0;kind<5;++kind){launch();before=offset;
  if(kind==0){touch.count=2;touch.touched=true;}
  if(kind==1)ctx.touch=NULL;
  if(kind==2)ctx.consumed=true;
  if(kind==3)ctx.now_ms=s.last_ms-1;
  if(kind==4)ctx.now_ms=-1;
  ui_follow_scroll_feed(&s,&ctx,160,1096,&offset,5000,&handled);
  assert(!s.coasting&&s.swallow&&handled&&offset==before);
 }
 launch();before=offset;feed(700,0,false,121);assert(!s.coasting&&offset==before);
 for(int direction=-1;direction<=1;direction+=2){launch();offset=direction<0?1:4999;s.velocity=direction*2400;
  feed(700,0,false,20);assert(offset==(direction<0?0:5000)&&!s.coasting);
 }
 launch();ui_follow_scroll_reset(&s);assert(!s.coasting&&!s.settle_pending&&!s.tracking);
 ui_follow_scroll_reset(&s);offset=1000;feed(800,1,true,10);feed(780,1,false,200);feed(780,0,false,10);assert(!s.coasting);
 launch();feed(680,1,true,10);feed(660,1,false,10);feed(660,0,false,100);assert(!s.coasting);
 ui_follow_scroll_reset(&s);offset=1000;feed(800,1,true,10);feed(740,1,false,20);feed(780,1,false,20);feed(780,0,false,10);assert(s.coasting&&s.velocity<0);
 puts("PASS: inertia decay independent of cadence, monotonic bounds, single delayed settle, immediate stop taps/resumed drags/navigation, stale/slow/held input, reversal, faults and page reset");
}
'''
with tempfile.TemporaryDirectory(prefix="kiikoread-inertia-") as folder:
    c = Path(folder) / "test.c"
    binary = Path(folder) / "test"
    c.write_text(CODE)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-g", "-fsanitize=address,undefined",
                    "-I" + str(ROOT / "tools/ui_gesture_stubs"), "-I" + str(ROOT / "main/ui"), str(c), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
