"""Validate the production one-shot NTP client at wire/socket boundaries."""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
code=r'''
#define _DEFAULT_SOURCE
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <arpa/inet.h>
typedef int esp_err_t;
enum {ESP_OK,ESP_ERR_INVALID_ARG,ESP_ERR_NO_MEM,ESP_ERR_TIMEOUT,ESP_ERR_INVALID_RESPONSE,ESP_FAIL,ESP_ERR_INVALID_STATE};
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
enum {NORMAL,TIMEOUT,DNSERR,MALFORMED,OOM,CONNECTERR,SETOPTERR,SENDERR,FALLBACK,CLOCKERR};
static int scenario,lookups,frees,opens,closes,sends,active,clock_sets;
static uint8_t cookie[8];static uint32_t saved_utc;static int64_t micro_clock;
static struct sockaddr_in peer;static struct addrinfo address;
static int fake_getaddrinfo(const char *host,const char *service,const struct addrinfo *hints,struct addrinfo **out){
 assert(host&&!strcmp(service,"123")&&hints->ai_family==AF_INET&&hints->ai_socktype==SOCK_DGRAM);++lookups;
 *out=NULL;if(scenario==DNSERR)return EAI_AGAIN;
 peer.sin_family=AF_INET;peer.sin_port=htons(123);peer.sin_addr.s_addr=htonl(0x7f000001);
 address.ai_addr=(struct sockaddr*)&peer;address.ai_addrlen=sizeof(peer);*out=&address;return 0;
}
static void fake_freeaddrinfo(struct addrinfo *p){assert(p==&address);++frees;}
static int fake_socket(int family,int type,int protocol){assert(!active&&family==AF_INET&&type==SOCK_DGRAM&&protocol==IPPROTO_UDP);if(scenario==OOM){errno=ENOMEM;return -1;}++opens;active=1;return 32;}
static int fake_connect(int fd,const struct sockaddr *p,socklen_t n){assert(fd==32&&active&&p==(struct sockaddr*)&peer&&n==sizeof(peer));return scenario==CONNECTERR?-1:0;}
static int fake_setsockopt(int fd,int level,int name,const void *p,socklen_t n){const struct timeval *t=p;assert(fd==32&&active&&level==SOL_SOCKET&&name==SO_RCVTIMEO&&n==sizeof(*t)&&t->tv_sec==0&&t->tv_usec==100000);return scenario==SETOPTERR?-1:0;}
static void fake_random(void *p,size_t n){assert(n==8);memset(p,0xa5,n);}
static ssize_t fake_send(int fd,const void *p,size_t n,int flags){const uint8_t *b=p;assert(fd==32&&active&&n==48&&!flags&&b[0]==0x23);memcpy(cookie,b+40,8);++sends;return scenario==SENDERR?-1:(ssize_t)n;}
static void wire_u32(uint8_t *p,uint32_t v){p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v;}
static ssize_t fake_recv(int fd,void *p,size_t n,int flags){assert(fd==32&&active&&n>=48&&!flags);uint8_t *b=p;micro_clock+=100000;
 if(scenario==TIMEOUT||(scenario==FALLBACK&&lookups==1)){errno=EAGAIN;return -1;}
 memset(b,0,48);b[0]=0x24;b[1]=2;memcpy(b+24,cookie,8);wire_u32(b+40,1791633600U+2208988800U);
 if(scenario==MALFORMED)b[24]^=1;
 return 48;
}
static int64_t fake_timer(void){return micro_clock;}
static int fake_close(int fd){assert(fd==32&&active);active=0;++closes;return 0;}
static int fake_settimeofday(const struct timeval *tv,const void *tz){assert(!tz&&tv->tv_usec>=0&&tv->tv_usec<1000000);if(scenario==CLOCKERR)return -1;assert(!active);saved_utc=tv->tv_sec;++clock_sets;return 0;}
#define getaddrinfo fake_getaddrinfo
#define freeaddrinfo fake_freeaddrinfo
#define socket fake_socket
#define connect fake_connect
#define setsockopt fake_setsockopt
#define send fake_send
#define recv fake_recv
#define close fake_close
#define settimeofday fake_settimeofday
#define esp_fill_random fake_random
#define esp_timer_get_time fake_timer
'''
header=(root/'components/read_pico_transfer/transfer_ntp_time.h').read_text()
code+=re.sub(r'^#(?:include|pragma)[^\n]*\n','',header,flags=re.M)
code+=re.sub(r'^#include[^\n]*\n','',(root/'components/read_pico_transfer/transfer_ntp_time.c').read_text(),flags=re.M)
code+=r'''
static bool cancel_now(void){return true;}
static bool cancel_during_receive(void){return micro_clock>=200000;}
static void reset(int kind){scenario=kind;lookups=frees=opens=closes=sends=active=clock_sets=0;micro_clock=0;saved_utc=0;}
int main(void){
 uint32_t utc=0;uint8_t packet[48]={0x24},nonce[8]={1};packet[1]=2;memcpy(packet+24,nonce,8);wire_u32(packet+40,1791633600U+2208988800U);
 assert(transfer_ntp_reply(packet,48,nonce,&utc)&&utc==1791633600U);
 assert(!transfer_ntp_reply(packet,47,nonce,&utc));packet[24]^=1;assert(!transfer_ntp_reply(packet,48,nonce,&utc));packet[24]^=1;
 packet[0]=0xe4;assert(!transfer_ntp_reply(packet,48,nonce,&utc));packet[0]=0x23;assert(!transfer_ntp_reply(packet,48,nonce,&utc));packet[0]=0x24;
 packet[1]=0;assert(!transfer_ntp_reply(packet,48,nonce,&utc));packet[1]=16;assert(!transfer_ntp_reply(packet,48,nonce,&utc));packet[1]=2;
 wire_u32(packet+40,2147483647U+2208988800U);assert(transfer_ntp_reply(packet,48,nonce,&utc)&&utc==2147483647U);
 wire_u32(packet+40,0);assert(!transfer_ntp_reply(packet,48,nonce,&utc));
 reset(NORMAL);assert(transfer_sync_time_ntp(&utc)==ESP_OK&&utc==1791633600U&&saved_utc==utc&&closes==1&&frees==1&&sends==1&&clock_sets==1);
 reset(FALLBACK);assert(transfer_sync_time_ntp(&utc)==ESP_OK&&closes==2&&frees==2&&sends==4);
 reset(TIMEOUT);assert(transfer_sync_time_ntp(&utc)==ESP_ERR_TIMEOUT&&utc==0&&closes==3&&frees==3&&sends==9&&!clock_sets);
 reset(DNSERR);assert(transfer_sync_time_ntp(&utc)==ESP_ERR_TIMEOUT&&lookups==3&&!opens&&!frees);
 reset(MALFORMED);assert(transfer_sync_time_ntp(&utc)==ESP_ERR_INVALID_RESPONSE&&utc==0&&closes==3&&!clock_sets);
 reset(OOM);assert(transfer_sync_time_ntp(&utc)==ESP_ERR_NO_MEM&&!opens&&frees==1);
 reset(CONNECTERR);assert(transfer_sync_time_ntp(&utc)==ESP_ERR_TIMEOUT&&opens==3&&closes==3&&frees==3);
 reset(SETOPTERR);assert(transfer_sync_time_ntp(&utc)==ESP_FAIL&&closes==1&&frees==1);
 reset(SENDERR);assert(transfer_sync_time_ntp(&utc)==ESP_ERR_TIMEOUT&&closes==3&&frees==3&&!clock_sets);
 reset(CLOCKERR);assert(transfer_sync_time_ntp(&utc)==ESP_FAIL&&utc==0&&closes==1&&!clock_sets);
 reset(TIMEOUT);assert(transfer_sync_time_ntp_cancelable(&utc,cancel_now)==ESP_ERR_INVALID_STATE&&!opens&&!clock_sets);
 reset(TIMEOUT);assert(transfer_sync_time_ntp_cancelable(&utc,cancel_during_receive)==ESP_ERR_INVALID_STATE&&micro_clock==200000&&closes==1&&!clock_sets);
 assert(transfer_sync_time_ntp(NULL)==ESP_ERR_INVALID_ARG);
 puts("PASS: actual NTP IPv4/socket requests, matching origin/LI/mode/stratum/length/2036 era, fallback/timeout/DNS/send/socket/clock failures and resource cleanup");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    src=Path(tmp)/'test.c';src.write_text(code);exe=Path(tmp)/'test'
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined',str(src),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
