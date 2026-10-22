/* Host hardware boundary. Firmware rendering and book logic are compiled unchanged. */
#define SIM_PLATFORM 1
#include "host.h"
#include "nvs.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "epdiy.h"
#include <pthread.h>
#include <sys/time.h>
#include <stdarg.h>
#include <errno.h>
#include <math.h>
#include <string.h>
#include <time.h>

static const char *data_root(void){const char *p=getenv("PICO_SIM_DATA");return p?p:"Data";}
const char *sim_path(const char *p){
 static _Thread_local char paths[8][4096];static _Thread_local unsigned next;
 if(!p)return NULL;
 if(!strncmp(p,"/sdcard",7)&&(!p[7]||p[7]=='/')){char *out=paths[next++%8];snprintf(out,4096,"%s/card%s",data_root(),p+7);return out;}
 if(!strncmp(p,"/flash",6)&&(!p[6]||p[6]=='/')){char *out=paths[next++%8];snprintf(out,4096,"%s/flash%s",data_root(),p+6);return out;}
 return p;
}
FILE *sim_fopen(const char *p,const char *m){return fopen(sim_path(p),m);}
DIR *sim_opendir(const char *p){return opendir(sim_path(p));}
int sim_stat(const char *p,struct stat *s){return stat(sim_path(p),s);}
int sim_mkdir(const char *p,mode_t m){return mkdir(sim_path(p),m);}
int sim_unlink(const char *p){return unlink(sim_path(p));}
int sim_rename(const char *p,const char *q){return rename(sim_path(p),sim_path(q));}
int sim_access(const char*p,int mode){return access(sim_path(p),mode);}
int sim_open(const char*p,int flags,...){mode_t mode=0600;if(flags&O_CREAT){va_list a;va_start(a,flags);mode=va_arg(a,int);va_end(a);}return open(sim_path(p),flags,mode);}
const char *esp_err_to_name(int e){return e==0?"ESP_OK":"SIM_ERROR";}
int64_t esp_timer_get_time(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return(int64_t)t.tv_sec*1000000+t.tv_nsec/1000;}
TickType_t xTaskGetTickCount(void){return esp_timer_get_time()/1000;}
struct sim_sem{pthread_mutex_t lock;pthread_cond_t ready;unsigned count;};
static SemaphoreHandle_t sem_make(unsigned n){SemaphoreHandle_t s=calloc(1,sizeof(*s));pthread_mutex_init(&s->lock,NULL);pthread_cond_init(&s->ready,NULL);s->count=n;return s;}
SemaphoreHandle_t xSemaphoreCreateMutex(void){return sem_make(1);}
SemaphoreHandle_t xSemaphoreCreateBinary(void){return sem_make(0);}
int xSemaphoreTake(SemaphoreHandle_t s,unsigned wait){if(!s)return 0;pthread_mutex_lock(&s->lock);while(!s->count){if(!wait){pthread_mutex_unlock(&s->lock);return 0;}pthread_cond_wait(&s->ready,&s->lock);}--s->count;pthread_mutex_unlock(&s->lock);return 1;}
int xSemaphoreGive(SemaphoreHandle_t s){if(!s)return 0;pthread_mutex_lock(&s->lock);s->count=1;pthread_cond_signal(&s->ready);pthread_mutex_unlock(&s->lock);return 1;}
void vSemaphoreDelete(SemaphoreHandle_t s){if(s){pthread_cond_destroy(&s->ready);pthread_mutex_destroy(&s->lock);free(s);}}
// 主机/浏览器使用有界合作调度，后台只做文件工作，字体仍由UI执行。/ Bounded cooperative host scheduler; workers only perform IO and UI retains font ownership.
struct sim_task{void(*fn)(void*);void *arg;bool pending;};
static struct sim_task tasks[16];
static TaskHandle_t this_task;
void sim_pump_tasks(void){for(unsigned i=0;i<16;i++)if(tasks[i].pending){this_task=&tasks[i];tasks[i].pending=false;tasks[i].fn(tasks[i].arg);this_task=NULL;tasks[i].fn=NULL;}}
int xTaskCreatePinnedToCore(void(*fn)(void*),const char*n,unsigned z,void*a,unsigned p,TaskHandle_t*out,int c){(void)n;(void)z;(void)p;(void)c;for(unsigned i=0;i<16;i++)if(!tasks[i].fn){tasks[i]=(struct sim_task){fn,a,true};if(out)*out=&tasks[i];return pdPASS;}return pdFALSE;}
int xTaskCreate(void(*fn)(void*),const char*n,unsigned z,void*a,unsigned p,TaskHandle_t*out){return xTaskCreatePinnedToCore(fn,n,z,a,p,out,0);}
void vTaskDelete(TaskHandle_t t){if(t)t->pending=false;}
void vTaskDelay(unsigned ms){(void)ms;}
void xTaskNotifyGive(TaskHandle_t t){if(t)t->pending=true;}
unsigned ulTaskNotifyTake(int clear,unsigned wait){(void)clear;(void)wait;return 1;}
unsigned uxTaskGetStackHighWaterMark(TaskHandle_t t){(void)t;return 16384;}

static char namespaces[64][32];static unsigned ns_count;
static bool safe_key(const char *p){if(!p||!*p)return false;for(;*p;p++)if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='_'||*p=='-'))return false;return true;}
static void nv_path(char *out,size_t z,nvs_handle_t h,const char *key){snprintf(out,z,"%s/nvs/%s/%s",data_root(),namespaces[h-1],key);}
int nvs_open(const char *ns,int mode,nvs_handle_t *out){if(!safe_key(ns))return ESP_ERR_INVALID_ARG;char p[4096];snprintf(p,sizeof(p),"%s/nvs",data_root());mkdir(p,0700);snprintf(p,sizeof(p),"%s/nvs/%s",data_root(),ns);if(mode==NVS_READONLY&&access(p,F_OK))return ESP_ERR_NVS_NOT_FOUND;if(mode==NVS_READWRITE&&mkdir(p,0700)&&errno!=EEXIST)return ESP_FAIL;unsigned i;for(i=0;i<ns_count;i++)if(!strcmp(ns,namespaces[i]))break;if(i==ns_count){if(ns_count==64)return ESP_FAIL;snprintf(namespaces[ns_count++],32,"%s",ns);}*out=i+1;return 0;}
void nvs_close(nvs_handle_t h){(void)h;}
static int nv_get(nvs_handle_t h,const char*k,void*out,size_t*n,nvs_type_t type){if(h<1||h>(int)ns_count||!safe_key(k))return ESP_ERR_INVALID_ARG;char p[4096];nv_path(p,sizeof(p),h,k);FILE*f=fopen(p,"rb");if(!f)return ESP_ERR_NVS_NOT_FOUND;uint32_t stored,z;if(fread(&stored,4,1,f)!=1||fread(&z,4,1,f)!=1||z>16*1024*1024){fclose(f);return ESP_FAIL;}if(type!=NVS_TYPE_ANY&&stored!=(uint32_t)type){fclose(f);return ESP_ERR_NVS_TYPE_MISMATCH;}if(!out){*n=z;fclose(f);return 0;}if(*n<z){*n=z;fclose(f);return ESP_ERR_NVS_INVALID_LENGTH;}int ok=fread(out,1,z,f)==z;*n=z;fclose(f);return ok?0:ESP_FAIL;}
static int nv_set(nvs_handle_t h,const char*k,const void*v,size_t n,nvs_type_t type){if(h<1||h>(int)ns_count||!safe_key(k))return ESP_ERR_INVALID_ARG;char p[4096],tmp[4096];nv_path(p,sizeof(p),h,k);snprintf(tmp,sizeof(tmp),"%s.tmp",p);FILE*f=fopen(tmp,"wb");if(!f)return ESP_FAIL;uint32_t t=type,z=n;bool ok=fwrite(&t,4,1,f)==1&&fwrite(&z,4,1,f)==1&&fwrite(v,1,n,f)==n;ok=fclose(f)==0&&ok;if(ok&&rename(tmp,p)==0)return 0;unlink(tmp);return ESP_FAIL;}
int nvs_get_blob(nvs_handle_t h,const char*k,void*p,size_t*n){return nv_get(h,k,p,n,NVS_TYPE_BLOB);}
int nvs_set_blob(nvs_handle_t h,const char*k,const void*p,size_t n){return nv_set(h,k,p,n,NVS_TYPE_BLOB);}
int nvs_get_str(nvs_handle_t h,const char*k,char*p,size_t*n){return nv_get(h,k,p,n,NVS_TYPE_STR);}
int nvs_set_str(nvs_handle_t h,const char*k,const char*p){return nv_set(h,k,p,strlen(p)+1,NVS_TYPE_STR);}
#define NV_NUMBER(kind,ctype,TYPE) int nvs_get_##kind(nvs_handle_t h,const char*k,ctype*p){size_t n=sizeof(*p);return nv_get(h,k,p,&n,TYPE);} int nvs_set_##kind(nvs_handle_t h,const char*k,ctype p){return nv_set(h,k,&p,sizeof(p),TYPE);}
NV_NUMBER(u8,uint8_t,NVS_TYPE_U8) NV_NUMBER(u16,uint16_t,NVS_TYPE_U16) NV_NUMBER(u32,uint32_t,NVS_TYPE_U32) NV_NUMBER(i32,int32_t,NVS_TYPE_I32)
int nvs_commit(nvs_handle_t h){(void)h;return 0;}
int nvs_erase_key(nvs_handle_t h,const char*k){char p[4096];nv_path(p,sizeof(p),h,k);return unlink(p)?ESP_ERR_NVS_NOT_FOUND:0;}
struct nvs_iterator_stub{DIR*dir;int h;nvs_type_t type;nvs_entry_info_t info;};
static int iterator_step(nvs_iterator_t *it){struct dirent*entry;while((entry=readdir((*it)->dir))){if(entry->d_name[0]=='.'||strchr(entry->d_name,'.'))continue;char p[4096];nv_path(p,sizeof(p),(*it)->h,entry->d_name);FILE*f=fopen(p,"rb");uint32_t type;if(!f)continue;bool ok=fread(&type,4,1,f)==1;fclose(f);if(!ok||((*it)->type!=NVS_TYPE_ANY&&type!=(*it)->type))continue;snprintf((*it)->info.namespace_name,16,"%s",namespaces[(*it)->h-1]);snprintf((*it)->info.key,16,"%s",entry->d_name);(*it)->info.type=type;return 0;}nvs_release_iterator(*it);*it=NULL;return ESP_ERR_NVS_NOT_FOUND;}
int nvs_entry_find_in_handle(nvs_handle_t h,nvs_type_t type,nvs_iterator_t*out){char p[4096];snprintf(p,sizeof(p),"%s/nvs/%s",data_root(),namespaces[h-1]);DIR*d=opendir(p);if(!d){*out=NULL;return ESP_ERR_NVS_NOT_FOUND;}*out=calloc(1,sizeof(**out));(*out)->dir=d;(*out)->h=h;(*out)->type=type;return iterator_step(out);}
int nvs_entry_find(const char*part,const char*ns,nvs_type_t t,nvs_iterator_t*out){(void)part;int h;int e=nvs_open(ns,0,&h);return e?e:nvs_entry_find_in_handle(h,t,out);}
int nvs_entry_next(nvs_iterator_t*out){return iterator_step(out);}
int nvs_entry_info(nvs_iterator_t it,nvs_entry_info_t*out){*out=it->info;return 0;}
void nvs_release_iterator(nvs_iterator_t it){if(it){closedir(it->dir);free(it);}}
int nvs_erase_all(nvs_handle_t h){nvs_iterator_t it=NULL;if(nvs_entry_find_in_handle(h,NVS_TYPE_ANY,&it))return 0;do{nvs_erase_key(h,it->info.key);}while(!nvs_entry_next(&it));return 0;}

int epd_width(void){return 1216;}int epd_height(void){return 684;}
int epd_rotated_display_width(void){return 684;}int epd_rotated_display_height(void){return 1216;}
static enum EpdRotation rotation=EPD_ROT_PORTRAIT;
enum EpdRotation epd_get_rotation(void){return rotation;}
void epd_set_rotation(enum EpdRotation r){rotation=r;}
static void physical(int *x,int *y){int a=*x,b=*y;if(rotation==EPD_ROT_PORTRAIT){*x=1215-b;*y=a;}else if(rotation==EPD_ROT_INVERTED_PORTRAIT){*x=b;*y=683-a;}else if(rotation==EPD_ROT_INVERTED_LANDSCAPE){*x=1215-a;*y=683-b;}}
void epd_draw_pixel(int x,int y,uint8_t c,uint8_t*fb){physical(&x,&y);if(x<0||x>=1216||y<0||y>=684)return;uint8_t*p=fb+y*608+x/2;*p=x%2?(*p&15)|(c&240):(*p&240)|(c>>4);}
uint8_t epd_get_pixel(int x,int y,int w,int h,const uint8_t*fb){if(x<0||x>=w||y<0||y>=h)return 255;uint8_t v=fb[y*w/2+x/2];return(x%2?v>>4:v&15)<<4;}
void epd_fill_rect(EpdRect r,uint8_t c,uint8_t*f){for(int y=r.y;y<r.y+r.height;y++)for(int x=r.x;x<r.x+r.width;x++)epd_draw_pixel(x,y,c,f);}
void epd_draw_hline(int x,int y,int n,uint8_t c,uint8_t*f){epd_fill_rect((EpdRect){x,y,n,1},c,f);}void epd_draw_vline(int x,int y,int n,uint8_t c,uint8_t*f){epd_fill_rect((EpdRect){x,y,1,n},c,f);}
void epd_draw_line(int x0,int y0,int x1,int y1,uint8_t c,uint8_t*f){int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1,err=dx+dy;for(;;){epd_draw_pixel(x0,y0,c,f);if(x0==x1&&y0==y1)break;int e=2*err;if(e>=dy){err+=dy;x0+=sx;}if(e<=dx){err+=dx;y0+=sy;}}}
void epd_draw_rect(EpdRect r,uint8_t c,uint8_t*f){epd_draw_hline(r.x,r.y,r.width,c,f);epd_draw_hline(r.x,r.y+r.height-1,r.width,c,f);epd_draw_vline(r.x,r.y,r.height,c,f);epd_draw_vline(r.x+r.width-1,r.y,r.height,c,f);}
void epd_draw_circle(int x,int y,int r,uint8_t c,uint8_t*f){int a=0,b=r,e=3-2*r;while(a<=b){int xx[]={a,-a,a,-a,b,-b,b,-b},yy[]={b,b,-b,-b,a,a,-a,-a};for(int i=0;i<8;i++)epd_draw_pixel(x+xx[i],y+yy[i],c,f);if(e<0)e+=4*a+6;else{e+=4*(a-b)+10;--b;}++a;}}
void epd_fill_circle(int x,int y,int r,uint8_t c,uint8_t*f){for(int j=-r;j<=r;j++){int w=(int)sqrt(r*r-j*j);epd_draw_hline(x-w,y+j,2*w+1,c,f);}}
void epd_fill_circle_helper(int x0,int y0,int r,int corners,int delta,uint8_t c,uint8_t*f){int a=0,b=r,e=1-r,dx=1,dy=-2*r;while(a<b){if(e>=0){b--;dy+=2;e+=dy;}a++;dx+=2;e+=dx;if(corners&1){epd_draw_vline(x0+a,y0-b,2*b+1+delta,c,f);epd_draw_vline(x0+b,y0-a,2*a+1+delta,c,f);}if(corners&2){epd_draw_vline(x0-a,y0-b,2*b+1+delta,c,f);epd_draw_vline(x0-b,y0-a,2*a+1+delta,c,f);}}}
void epd_draw_rotated_image(EpdRect r,const uint8_t*image,uint8_t*f){for(int y=0;y<r.height;y++)for(int x=0;x<r.width;x++){size_t i=y*r.width+x;uint8_t v=image[i/2];epd_draw_pixel(r.x+x,r.y+y,((i&1)?v>>4:v&15)<<4,f);}}
void epd_draw_image(EpdRect r,const uint8_t*p,uint8_t*f){epd_draw_rotated_image(r,p,f);}
void epd_copy_to_framebuffer(EpdRect r,const uint8_t*p,uint8_t*f){epd_draw_rotated_image(r,p,f);}
EpdRect epd_full_screen(void){return(EpdRect){0,0,1216,684};}
void epd_poweron(void){}void epd_poweroff(void){}

#ifdef __EMSCRIPTEN__
static time_t sim_clock_offset;
extern time_t __real_time(time_t *);
time_t __wrap_time(time_t *out){time_t now=__real_time(NULL)+sim_clock_offset;if(out)*out=now;return now;}
int settimeofday(const struct timeval *value,const struct timezone *zone){(void)zone;sim_clock_offset=value->tv_sec-__real_time(NULL);return 0;}
#endif
