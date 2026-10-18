/* SPDX-License-Identifier: Apache-2.0
 * 验证真实缓存/提交失败/空闲与阻塞边界，不进行硬件唤醒。/ Test real cache, failed persistence and idle/blocking boundaries, without wake hardware.
 */
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "../main/anti_fade.c"
static unsigned opens,read_count,sets,commits,closes;
static int fail_open,fail_get,fail_set,fail_commit;
static uint8_t persisted=5,staged;
int nvs_open(const char* space,int mode,nvs_handle_t* h){assert(!strcmp(space,"metalio_hw"));(void)mode;if(fail_open)return -1;++opens;*h=1;return 0;}
int nvs_get_u8(nvs_handle_t h,const char* key,uint8_t* v){assert(h==1&&!strcmp(key,"antifade_min"));++read_count;if(fail_get)return -1;*v=persisted;return 0;}
int nvs_set_u8(nvs_handle_t h,const char* key,uint8_t v){assert(h==1&&!strcmp(key,"antifade_min"));++sets;if(fail_set)return -1;staged=v;return 0;}
int nvs_commit(nvs_handle_t h){assert(h==1);++commits;if(fail_commit)return -1;persisted=staged;return 0;}
void nvs_close(nvs_handle_t h){assert(h==1);++closes;}
static void reset(void){opens=read_count=sets=commits=closes=0;fail_open=fail_get=fail_set=fail_commit=0;persisted=5;s_loaded=s_clock_ready=false;s_minutes=5;s_last_refresh_ms=0;}
int main(void){
 const unsigned values[]={0,1,3,5,10,30};
 for(unsigned i=0;i<6;++i){reset();persisted=values[i];for(int j=0;j<10000;++j)assert(anti_fade_minutes()==values[i]);assert(opens==1&&read_count==1&&closes==1);}
 for(unsigned bad=0;bad<3;++bad){reset();fail_open=bad==0;fail_get=bad==1;persisted=bad==2?255:5;assert(anti_fade_minutes()==5);unsigned prior=opens;for(int j=0;j<100;++j)assert(anti_fade_minutes()==5);assert(prior==opens&&opens==closes);}
 for(unsigned f=0;f<3;++f){reset();assert(anti_fade_minutes()==5);fail_open=f==0;fail_set=f==1;fail_commit=f==2;anti_fade_set_minutes(3);assert(anti_fade_minutes()==5&&persisted==5&&opens==closes);}
 reset();anti_fade_set_minutes(3);assert(persisted==3&&anti_fade_minutes()==3&&commits==1);anti_fade_set_minutes(3);assert(commits==1);anti_fade_set_minutes(2);assert(persisted==3);
 reset();assert(!anti_fade_due(1000,1000,false));assert(!anti_fade_due(300999,1000,false));assert(anti_fade_due(301000,1000,false));assert(!anti_fade_due(301001,1000,false));assert(anti_fade_due(601000,1000,false));assert(read_count==1);
 {reset();assert(!anti_fade_due(0,0,false));assert(!anti_fade_due(300000,0,true));assert(!anti_fade_due(599999,0,false));assert(anti_fade_due(600000,0,false));}
 reset();assert(!anti_fade_due(0,0,false));assert(!anti_fade_due(300000,299000,false));assert(!anti_fade_due(598999,299000,false));assert(anti_fade_due(599000,299000,false));
 reset();assert(!anti_fade_due(900000,900000,false));assert(!anti_fade_due(1,0,false));assert(!anti_fade_due(300000,0,false));assert(anti_fade_due(300001,0,false));
 reset();anti_fade_set_minutes(0);for(int i=0;i<100;++i)assert(!anti_fade_due((int64_t)i*600000,0,false));assert(read_count==1);
 puts("PASS: cached one-time NVS, valid/legacy/missing settings, transactional failure, complete idle interval, blocked/wake/clock rollback, off and no hot-loop NVS");
}
