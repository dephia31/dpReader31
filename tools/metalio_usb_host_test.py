"""Production S31 USJ recovery, early GPIO36 configuration and CX25601N sleep guard."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
ROOT=Path(__file__).resolve().parents[1]
stub=r'''
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#define ESP_OK 0
#define IRAM_ATTR
#define CONFIG_ESP_SYSTEM_SINGLE_CORE_MODE 0
#define CONFIG_SPIRAM 1
#define WDT_MWDT0 0
#define WDT_MWDT1 1
#define CACHE_MAP_L1_DCACHE 1
typedef struct {int inst;void *mwdt_dev;} wdt_hal_context_t;
extern int TIMERG0,TIMERG1;
void rv_utils_intr_global_disable(void);
unsigned esp_cpu_get_core_id(void);
void esp_cpu_reset(unsigned core);
void esp_cpu_stall(unsigned core);
void esp_cpu_unstall(unsigned core);
void wdt_hal_write_protect_disable(wdt_hal_context_t *w);
void wdt_hal_disable(wdt_hal_context_t *w);
void wdt_hal_write_protect_enable(wdt_hal_context_t *w);
void Cache_WriteBack_All(unsigned mask);
void rtc_clk_cpu_set_to_default_config(void);
void esp_system_reset_modules_on_exit(void);
void ets_set_appcpu_boot_addr(unsigned addr);
#define ESP_ERR_NOT_SUPPORTED -1
#define ESP_ERR_INVALID_STATE -2
#define GPIO_PULLUP_ENABLE 1
#define ESP_EFUSE_DIS_DOWNLOAD_MODE 0
#define ESP_EFUSE_DIS_FORCE_DOWNLOAD 1
#define ESP_EFUSE_DIS_USB_SERIAL_JTAG_DOWNLOAD_MODE 2
#define USB_SERIAL_JTAG_LL_INTR_MASK 0x7ffff
#define USB_SERIAL_JTAG_INTR_SERIAL_OUT_RECV_PKT 4
#define USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY 8
struct usj_t {
 union {struct {unsigned usb_mem_pd:1,usb_mem_clk_en:1;};unsigned val;} mem_conf;
 union {struct {unsigned rts:1,dtr:1,usb_uart_chip_rst_dis:1;};unsigned val;} chip_rst;
 union {unsigned config_update;unsigned val;} config_update;
 struct {unsigned val;} conf0,ser_afifo_config;
};
extern struct usj_t USB_SERIAL_JTAG;
#define LP_SYSTEM_REG_SYS_CTRL_REG 0
#define LP_SYSTEM_REG_FORCE_DOWNLOAD_BOOT 4
#define REG_SET_BIT(r,b) arm_force_download(r,b)
#define ESP_ERROR_CHECK(x) assert((x)==0)
#define GPIO_NUM_36 36
#define GPIO_MODE_OUTPUT 1
#define GPIO_MODE_INPUT 2
#define GPIO_MODE_INPUT_OUTPUT (GPIO_MODE_OUTPUT|GPIO_MODE_INPUT)
#define pdMS_TO_TICKS(x) (x)
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
typedef int portMUX_TYPE;
typedef int esp_err_t;
typedef struct {uint64_t pin_bit_mask;int mode,pull_up_en;} gpio_config_t;
int gpio_hold_en(int pin);
bool esp_efuse_read_field_bit(int field);
void esp_rom_software_reset_system(void);
void metalio_haptic_stop(void);
void metalio_frontlight(unsigned b,unsigned w);
void metalio_tps_off(void);
void arm_force_download(int reg,int mask);
int gpio_hold_dis(int pin);
void gpio_set_level(int pin,int value);
int gpio_config(const gpio_config_t *cfg);
int gpio_get_level(int pin);
void usb_serial_jtag_ll_enable_bus_clock(bool value);
void usb_serial_jtag_ll_phy_enable_pad(bool value);
void usb_serial_jtag_ll_phy_set_defaults(void);
bool usb_serial_jtag_is_connected(void);
bool usb_serial_jtag_ll_module_is_enabled(void);
bool usb_serial_jtag_is_driver_installed(void);
uint32_t usb_serial_jtag_ll_get_intr_ena_status(void);
void usb_serial_jtag_ll_disable_intr_mask(uint32_t);
void usb_serial_jtag_ll_ena_intr_mask(uint32_t);
void usb_serial_jtag_ll_reset_register(void);
int64_t esp_timer_get_time(void);
void vTaskDelay(int ms);
int metalio_read(uint8_t address,uint8_t reg,void *value,size_t n);
'''
unit=r'''
#include "stub.h"
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
static int64_t clock_us;
static bool route,output,input,connected,pad,clock_on;
static bool driver_installed=true;
static uint32_t interrupt_mask=4;
static unsigned controller_resets;
static int charger_error;
static uint8_t charger_status;
static unsigned reads,holds,configs,delays,detach,defaults;
struct usj_t USB_SERIAL_JTAG={.mem_conf={.usb_mem_pd=1,.usb_mem_clk_en=0},.chip_rst={.usb_uart_chip_rst_dis=1}};
static bool retained,force_armed,download_disabled;
static int disabled_field=-1;
static int hold_error,release_error;
static unsigned resets,lights_off,current_core,wdt_stopped;
static bool irq_disabled,xtal,cache_flushed,other_stalled,boot_entry_cleared,modules_exited;
int TIMERG0,TIMERG1;
static jmp_buf rom_reset;
static char sequence[128];static unsigned seq_count;
static void mark(char c){sequence[seq_count++]=c;sequence[seq_count]=0;}
void rv_utils_intr_global_disable(void){irq_disabled=true;}
unsigned esp_cpu_get_core_id(void){return current_core;}
void esp_cpu_reset(unsigned core){assert(irq_disabled&&core==!current_core);}
void esp_cpu_stall(unsigned core){assert(irq_disabled&&core==!current_core);other_stalled=true;}
void esp_cpu_unstall(unsigned core){assert(xtal&&boot_entry_cleared&&other_stalled&&core==!current_core);other_stalled=false;}
void wdt_hal_write_protect_disable(wdt_hal_context_t *w){assert(irq_disabled&&other_stalled&&w->mwdt_dev==(w->inst?&TIMERG1:&TIMERG0));}
void wdt_hal_disable(wdt_hal_context_t *w){assert((unsigned)w->inst==wdt_stopped);++wdt_stopped;}
void wdt_hal_write_protect_enable(wdt_hal_context_t *w){assert(w->inst<2);}
void Cache_WriteBack_All(unsigned mask){assert(mask==CACHE_MAP_L1_DCACHE&&wdt_stopped==2);cache_flushed=true;}
void esp_system_reset_modules_on_exit(void){assert(cache_flushed&&other_stalled&&wdt_stopped==2);modules_exited=true;}
void rtc_clk_cpu_set_to_default_config(void){assert(cache_flushed&&other_stalled&&modules_exited);xtal=true;}
void ets_set_appcpu_boot_addr(unsigned addr){assert(!addr&&xtal);boot_entry_cleared=true;}
int gpio_hold_dis(int pin){assert(pin==36&&output&&input);if(release_error)return release_error;retained=false;++holds;return 0;}
int gpio_hold_en(int pin){assert(pin==36&&route&&output&&input);retained=!hold_error;return hold_error;}
bool esp_efuse_read_field_bit(int field){assert(field>=0&&field<=2);return download_disabled || field==disabled_field;}
void metalio_haptic_stop(void){assert(retained);}
void metalio_frontlight(unsigned b,unsigned w){assert(retained&&b==0&&w==50);++lights_off;}
void metalio_tps_off(void){assert(retained&&lights_off);}
void arm_force_download(int reg,int mask){assert(reg==0&&mask==4&&retained&&lights_off);assert(irq_disabled&&xtal&&cache_flushed&&wdt_stopped==2&&!other_stalled&&boot_entry_cleared);force_armed=true;}
void esp_rom_software_reset_system(void){assert(force_armed&&retained&&route&&pad);++resets;longjmp(rom_reset,1);}
void gpio_set_level(int pin,int value){assert(pin==36);if(!retained)route=value;}
int gpio_config(const gpio_config_t *cfg){assert(cfg->pin_bit_mask==(1ULL<<36)&&cfg->mode==GPIO_MODE_INPUT_OUTPUT&&cfg->pull_up_en==1&&route);output=(cfg->mode&GPIO_MODE_OUTPUT)!=0;input=(cfg->mode&GPIO_MODE_INPUT)!=0;++configs;return 0;}
int gpio_get_level(int pin){assert(pin==36&&output);return input?route:0;}
void usb_serial_jtag_ll_enable_bus_clock(bool value){assert(value);clock_on=true;mark('C');}
void usb_serial_jtag_ll_phy_enable_pad(bool value){assert(clock_on&&!USB_SERIAL_JTAG.mem_conf.usb_mem_pd&&USB_SERIAL_JTAG.mem_conf.usb_mem_clk_en&&!USB_SERIAL_JTAG.chip_rst.usb_uart_chip_rst_dis);assert(USB_SERIAL_JTAG.config_update.val==1);USB_SERIAL_JTAG.config_update.val=0;pad=value;if(!value){++detach;mark('D');}else mark('A');}
void usb_serial_jtag_ll_phy_set_defaults(void){assert(clock_on);++defaults;mark('F');}
bool usb_serial_jtag_is_connected(void){return connected;}
bool usb_serial_jtag_ll_module_is_enabled(void){return clock_on;}
bool usb_serial_jtag_is_driver_installed(void){return driver_installed;}
uint32_t usb_serial_jtag_ll_get_intr_ena_status(void){return interrupt_mask;}
void usb_serial_jtag_ll_disable_intr_mask(uint32_t mask){assert(!route&&mask==USB_SERIAL_JTAG_LL_INTR_MASK);interrupt_mask&=~mask;}
void usb_serial_jtag_ll_ena_intr_mask(uint32_t mask){assert(!route&&controller_resets);interrupt_mask|=mask;}
void usb_serial_jtag_ll_reset_register(void){
 assert(!route&&!retained&&clock_on&&!interrupt_mask);
 ++controller_resets;
 USB_SERIAL_JTAG=(struct usj_t){0};pad=false;
}
int64_t esp_timer_get_time(void){return clock_us;}
void vTaskDelay(int ms){assert((ms==150&&!pad&&route&&USB_SERIAL_JTAG.config_update.val==1)||(ms==500&&!route&&!retained));++delays;clock_us+=ms*1000;mark('W');}
int metalio_read(uint8_t a,uint8_t r,void *p,size_t n){assert(a==0x6b&&r==0x1e&&n==1);++reads;*(uint8_t*)p=charger_status;return charger_error;}
'''
unit+=function('metalio_ana_sw',ROOT/'components/metalio_hw/metalio_hw.c')+'\n'
unit+='#include "'+str(ROOT/'components/metalio_hw/metalio_usb.c')+'"\n'
unit+=r'''
int main(void){
 metalio_usb_serial_startup();assert(output&&input&&route&&retained&&configs==1&&pad&&clock_on);
 assert(USB_SERIAL_JTAG.config_update.val==1&&!download_prepared);
 // 正常启动即保持SEL，模拟主机复位时不再依赖未装的外部上拉。
 // Normal startup retains SEL across a simulated host reset, without an external pull-up.
 assert(retained&&route);if(!retained)route=false;assert(route);
 input=false;assert(!gpio_get_level(36)&&route);input=true;
 assert(metalio_usb_blocks_sleep());clock_us=2000000;assert(!metalio_usb_blocks_sleep());
 for(unsigned v=0;v<8;++v){usb_guard_until=0;charger_status=0xf8|v;assert(metalio_usb_blocks_sleep()==(v!=0&&v!=7));}
 usb_guard_until=0;charger_error=-1;assert(metalio_usb_blocks_sleep());charger_error=0;
 charger_status=0;connected=true;unsigned before=reads;assert(metalio_usb_blocks_sleep()&&reads==before);
 // 长时间正常连接不得周期断连或重置；只读睡眠检查保持硬件下载通道。
 // Repeated normal checks must never detach/reset a connected host.
 for(unsigned n=0;n<200;n++)assert(metalio_usb_blocks_sleep()&&retained&&route);
 assert(controller_resets==0&&delays==0);
 connected=false;clock_us+=2000000;assert(!metalio_usb_blocks_sleep());
 // SD/CNNT恢复后，即使寄存器/端点状态已丢失也完整恢复；不进入ROM或停止CPU。
 // Restore lost USJ state after SD/CNNT without entering ROM or stopping the CPU.
 clock_on=pad=false;USB_SERIAL_JTAG.mem_conf.usb_mem_pd=1;USB_SERIAL_JTAG.chip_rst.usb_uart_chip_rst_dis=1;
 interrupt_mask=6;metalio_usb_serial_reconnect();
 assert(retained&&route&&pad&&clock_on&&controller_resets==1&&delays==1&&interrupt_mask==14);
 assert(!resets&&!force_armed&&!irq_disabled&&!lights_off&&!wdt_stopped);
 assert(metalio_usb_blocks_sleep());
 // MSC必须显式释放保持；串口恢复函数不能抢走读卡端口。
 // MSC must release retained SEL explicitly; serial recovery must not steal the MSC route.
 assert(metalio_usb_route_serial(false)==ESP_OK&&!retained&&!route);
 unsigned count=controller_resets;metalio_usb_serial_reconnect();assert(!route&&!retained&&controller_resets==count);
 assert(metalio_download_prepare()==ESP_ERR_INVALID_STATE&&!download_prepared);
 assert(metalio_usb_route_serial(true)==ESP_OK&&route&&retained);metalio_usb_serial_reconnect();
 assert(route&&retained&&controller_resets==2);
 for(disabled_field=0;disabled_field<=2;disabled_field+=2)assert(metalio_download_prepare()==ESP_ERR_NOT_SUPPORTED&&retained);
 disabled_field=1;assert(metalio_download_prepare()==ESP_OK);metalio_download_cancel();assert(retained&&route&&!download_prepared);
 disabled_field=-1;
 // 取消手动恢复页仍保持普通可刷机状态；失败不能伪装成读卡已接管。
 // Cancelling manual recovery keeps normal flashing available; handoff failures are observable.
 release_error=-9;assert(metalio_usb_route_serial(false)==-9&&retained&&route);release_error=0;
 hold_error=-3;assert(metalio_download_prepare()==-3&&!download_prepared&&!retained&&route);hold_error=0;
 assert(metalio_usb_route_serial(true)==ESP_OK&&retained);
 assert(metalio_download_prepare()==ESP_OK);count=controller_resets;
 assert(metalio_request_download()==ESP_OK&&retained&&route&&controller_resets==count+1);
 assert(!metalio_download_connected());connected=true;assert(metalio_download_connected());
 metalio_download_cancel();assert(!metalio_download_connected()&&retained&&route);
 metalio_download_cancel();assert(retained&&route);
 char diagnostic[256];metalio_usb_describe(diagnostic,sizeof(diagnostic));assert(strstr(diagnostic,"SEL=1")&&strstr(diagnostic,"clock=1")&&strstr(diagnostic,"prepared=0"));
 assert(metalio_request_download()==ESP_ERR_INVALID_STATE);
 driver_installed=false;interrupt_mask=14;assert(metalio_download_prepare()==ESP_OK&&metalio_request_download()==ESP_OK&&interrupt_mask==2);
 metalio_download_cancel();
 // 高保持失败可见，重试正常恢复可以重新建立持续下载通路。
 // A retention failure is visible; a successful retry restores the normal download route.
 assert(metalio_download_prepare()==ESP_OK);hold_error=-3;
 assert(metalio_request_download()==-3&&route&&!retained);hold_error=0;
 metalio_download_cancel();assert(!download_prepared&&route&&retained);
 puts("PASS: normal GPIO36 retained, no periodic host reset, SD/CNNT controller recovery, MSC explicit release/restore, manual cancel keeps normal USB, hold failures, driver IRQ restoration, read-only fuses and VBUS sleep gates");

}
'''
with tempfile.TemporaryDirectory() as d:
    p=Path(d);(p/'stub.h').write_text(stub)
    for n in ('soc/usb_serial_jtag_struct.h','metalio_hw.h','driver/gpio.h','driver/usb_serial_jtag.h','hal/usb_serial_jtag_ll.h','esp_timer.h','esp_log.h','esp_efuse.h','esp_efuse_table.h','esp_rom_sys.h','esp_attr.h','esp_cpu.h','esp_private/rtc_clk.h','esp_private/system_internal.h','riscv/rv_utils.h','hal/wdt_hal.h','soc/soc_caps.h','esp32s31/rom/cache.h','esp32s31/rom/ets_sys.h','soc/lp_system_reg.h','soc/soc.h','freertos/FreeRTOS.h','freertos/task.h'):
        h=p/n;h.parent.mkdir(parents=True,exist_ok=True);h.write_text('#include "stub.h"\n')
    (p/'test.c').write_text(unit)
    subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(p),str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
