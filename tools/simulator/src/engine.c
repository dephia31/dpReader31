#include "host.h"
#include "app.h"
#include "app_registry.h"
#include "app_content_open.h"
#include "ui_gesture.h"
#include "ui_nav.h"
#include "ui_quick_menu.h"
#include "ui_click_feedback.h"
#include "ui_pinpad.h"
#include "settings.h"
#include "ttf_font.h"
#include "app_font_context.h"
#include "book_store.h"
#include "book_ticket.h"
#include "book_lock_collage.h"
#include "book_lock_bookmark.h"
#include "ui_wallpaper.h"
#include "ui_kit.h"
#ifdef __EMSCRIPTEN__
extern const uint8_t *lock_4bpp_bin_start;
#else
extern const uint8_t lock_4bpp_bin_start[] asm("_binary_lock_4bpp_bin_start");
#endif
#include "lock_pin.h"
#include "esp_timer.h"
#include "nvs.h"
#include "freertos/task.h"
#include <string.h>
#include <math.h>
extern const app_desc_t app_book;
extern const app_desc_t app_dashboard;
static bool home_preview=true;
void sim_pump_tasks(void);
void sim_frame_write(const uint8_t*,const char*);
void sim_wifi_toggle(void);
bool sim_wifi_on(void);

static uint8_t framebuffer[1216*684/2],lock_frame[1216*684/2];
static EpdiyHighlevelState hl;
static app_ctx_t ctx;
static const app_desc_t *current,*history[20];static int history_count;
static cst836u_touch_t touch;
static ui_gesture_t gestures;
static ui_quick_menu_t quick;
static app_lock_font_t lock_font;
static bool locked,authenticating;static ui_pinpad_t pinpad;
static unsigned frame_number;static char notice[180];
static void paint(app_redraw_t result){if(result==APP_REDRAW_NONE)return;if(result==APP_REDRAW_PAGE||result==APP_REDRAW_FULL)ui_click_feedback_begin(framebuffer);bool handled=current->present&&current->present(&ctx,result);if(!handled&&(result==APP_REDRAW_PAGE||result==APP_REDRAW_FULL)&&current->render)current->render(&ctx,framebuffer);if(quick.open)ui_quick_menu_draw(framebuffer,sim_wifi_on(),app_settings_ble_turner());}
static void draw(void){paint(APP_REDRAW_PAGE);}
static void change(const app_desc_t *next,bool remember){if(!next)return;if(current&&current->on_exit)current->on_exit(&ctx);if(remember&&current&&next!=current&&history_count<20)history[history_count++]=current;current=next;ctx.request_app=NULL;ctx.request_return=ctx.request_menu=false;ctx.leaf=0;ui_gesture_reset(&gestures);ui_quick_menu_reset(&quick);if(current->on_enter)current->on_enter(&ctx);draw();}
static void settle(app_redraw_t result){if(ctx.request_return){ctx.request_return=false;change(history_count?history[--history_count]:app_home_page(),false);return;}if(ctx.request_app){const app_desc_t*next=ctx.request_app;change(next,true);return;}if(ctx.request_menu){ctx.request_menu=false;change(app_at(3),true);return;}paint(result);}
static void lock(void){if(current->on_before_lock)current->on_before_lock(&ctx);app_font_begin_lock(&lock_font);int style=app_settings_lock_style();bool drawn=style==0?book_ticket_draw(framebuffer,current==&app_book):style==2?book_lock_collage_draw(framebuffer):style==3?book_lock_bookmark_draw(framebuffer,app_settings_bookmark_path()):ui_wallpaper_prepare_lock(framebuffer,app_settings_wallpaper_path());if(!drawn){memset(framebuffer,255,sizeof(framebuffer));ui_draw_full_image(framebuffer,lock_4bpp_bin_start);}memcpy(lock_frame,framebuffer,sizeof(framebuffer));locked=true;authenticating=false;ui_quick_menu_reset(&quick);}
static void wake(void){if(!locked)return;if(lock_pin_enabled()){authenticating=true;ui_pinpad_begin(&pinpad,framebuffer,"");ui_pinpad_paint(framebuffer,&pinpad,ui_pinpad_full());}else{locked=false;app_font_end_lock(&lock_font);draw();}}
static void feed(int x,int y,bool active){ctx.now_ms=esp_timer_get_time()/1000;ctx.pressed=active&&!touch.touched;ctx.released=!active&&touch.touched;touch.touched=active;touch.count=active?1:0;touch.x=x;touch.y=y;touch.points[0]=(cst836u_point_t){.active=active,.x=x,.y=y};ctx.consumed=false;
 if(!active){EpdRect dirty;ui_click_feedback_release(framebuffer,&dirty);}
 if(locked){if(authenticating){ui_gesture_event_t event;if(ui_gesture_feed(&gestures,&ctx,&event)){EpdRect dirty;ui_pin_result_t action=ui_pinpad_handle(&pinpad,&event,&dirty);if(action==UI_PIN_COMPLETE){if(lock_pin_verify(pinpad.digits)){locked=authenticating=false;ui_pinpad_end(&pinpad);app_font_end_lock(&lock_font);draw();}else{ui_pinpad_reset(&pinpad,"","密码不正确");snprintf(notice,sizeof(notice),"密码不正确");ui_pinpad_paint(framebuffer,&pinpad,ui_pinpad_full());}}else if(action==UI_PIN_CANCEL){authenticating=false;ui_pinpad_end(&pinpad);memcpy(framebuffer,lock_frame,sizeof(framebuffer));}else ui_pinpad_paint(framebuffer,&pinpad,ui_pinpad_full());}}return;}
 if(ctx.pressed&&!quick.open){EpdRect dirty;ui_click_feedback_press(framebuffer,x,y,&dirty);}
 ui_quick_action_t action=UI_QUICK_NONE;
 if(ui_quick_menu_feed(&quick,&ctx,true,&action)){if(action==UI_QUICK_WIFI)sim_wifi_toggle();if(action==UI_QUICK_BLUETOOTH)app_settings_set_ble_turner(!app_settings_ble_turner());if(action==UI_QUICK_LOCK){lock();return;}if(action==UI_QUICK_CLOSE){ui_quick_menu_end();draw();}else if(action==UI_QUICK_OPEN)ui_quick_menu_draw(framebuffer,sim_wifi_on(),app_settings_ble_turner());else if(quick.open){EpdRect dirty;for(int b=0;b<4;b++)ui_quick_menu_draw_button(framebuffer,b,b==0?sim_wifi_on():b==1?app_settings_ble_turner():false,ui_quick_menu_pressed(&quick)==b,&dirty);}return;}
 ui_gesture_event_t event;if(ui_gesture_feed(&gestures,&ctx,&event)&&current->on_gesture)settle(current->on_gesture(&ctx,&event));
}
static void emit(void){const char *root=getenv("PICO_SIM_DATA");char path[4096];snprintf(path,sizeof(path),"%s/frame.png",root?root:"Data");sim_frame_write(framebuffer,path);printf("{\"frame\":\"%s\",\"sequence\":%u,\"page\":\"%s\",\"locked\":%s,\"notice\":\"%s\",\"version\":\"0.3.3-rc97\",\"homePreview\":%s}\n",path,++frame_number,locked?"锁屏":current->title,locked?"true":"false",notice,home_preview?"true":"false");fflush(stdout);notice[0]=0;}
void sim_init(void){setvbuf(stdout,NULL,_IOLBF,0);memset(framebuffer,255,sizeof(framebuffer));hl.front_fb=framebuffer;hl.back_fb=framebuffer;ctx=(app_ctx_t){.hl=&hl,.fb=framebuffer,.touch=&touch,.now_ms=esp_timer_get_time()/1000};app_settings_init();nvs_handle_t host_nvs;uint8_t initialized=0;if(nvs_open("sim_host",NVS_READWRITE,&host_nvs)==ESP_OK){nvs_get_u8(host_nvs,"initialized",&initialized);if(!initialized){if(ttf_font_path_is_builtin(app_settings_font_path())&&access(sim_path("/sdcard/fonts/Hei.ttf"),R_OK)==0)app_settings_set_font_path("/sdcard/fonts/Hei.ttf");nvs_set_u8(host_nvs,"initialized",1);nvs_commit(host_nvs);}nvs_close(host_nvs);}app_font_activate_system();change(app_home_page(),false);for(int i=0;i<12;i++){ctx.now_ms=esp_timer_get_time()/1000;sim_pump_tasks();if(current->on_tick)settle(current->on_tick(&ctx));}}
void sim_command(const char *command){char line[2048];snprintf(line,sizeof(line),"%s",command);int x=0,y=0,n=0;ctx.now_ms=esp_timer_get_time()/1000;
 if(sscanf(line,"home %d",&n)==1){if(!locked){home_preview=n!=0;change(app_home_page(),false);}}
 else if(sscanf(line,"page %d",&n)==1){if(!locked)change(app_at(n),false);}
 else if(sscanf(line,"down %d %d",&x,&y)==2)feed(x,y,true);
 else if(sscanf(line,"move %d %d",&x,&y)==2)feed(x,y,true);
 else if(sscanf(line,"up %d %d",&x,&y)==2)feed(x,y,false);
 else if(sscanf(line,"tap %d %d",&x,&y)==2){feed(x,y,true);vTaskDelay(35);feed(x,y,false);}
 else if(sscanf(line,"key %d",&n)==1){if(locked)wake();else if(current->on_key)settle(current->on_key(&ctx,n));}
 else if(!strncmp(line,"suspend",7)){if(current&&current->on_before_lock)current->on_before_lock(&ctx);}
 else if(!strncmp(line,"power",5)){if(locked)wake();else lock();}
 else if(!strncmp(line,"quick",5)){if(!locked){quick.open=!quick.open;if(quick.open)ui_quick_menu_draw(framebuffer,sim_wifi_on(),app_settings_ble_turner());else{ui_quick_menu_end();draw();}}}
 else if(!strncmp(line,"refresh",7)){if(!locked)draw();}
 else if(!strncmp(line,"rescan",6)){if(!locked){book_store_notify_changed();change(current,false);}}
 else if(!strncmp(line,"open ",5)){line[strcspn(line,"\r\n")]=0;char *path=line+5;if(!locked){app_book_request_open(path);change(&app_book,true);}}
 else if(!strncmp(line,"font ",5)){if(!locked){line[strcspn(line,"\r\n")]=0;app_settings_set_font_path(line+5);app_settings_set_system_font_path(line+5);app_font_retry_active();draw();}}
 else if(!strncmp(line,"quit",4)){return;}
 if(!locked&&current->on_tick){for(int i=0;i<3;i++){ctx.now_ms=esp_timer_get_time()/1000;sim_pump_tasks();settle(current->on_tick(&ctx));}}
 }
void sim_shutdown(void){if(current&&current->on_before_lock)current->on_before_lock(&ctx);if(current&&current->on_exit)current->on_exit(&ctx);}
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
EMSCRIPTEN_KEEPALIVE uint8_t *sim_frame(void){return framebuffer;}
EMSCRIPTEN_KEEPALIVE const char *sim_page(void){return locked?"锁屏":current->title;}
EMSCRIPTEN_KEEPALIVE int sim_locked(void){return locked;}
EMSCRIPTEN_KEEPALIVE int sim_quick_open(void){return quick.open;}
#else
int main(void){sim_init();emit();char line[2048];while(fgets(line,sizeof(line),stdin)){if(!strncmp(line,"quit",4))break;sim_command(line);emit();}sim_shutdown();return 0;}
#endif
