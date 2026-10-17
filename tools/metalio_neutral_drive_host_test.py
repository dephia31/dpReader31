from pathlib import Path
import subprocess,tempfile,sys
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'tools'))
from home_cover_cache_host_test import function
unit='''#include <stdbool.h>
#include <assert.h>
typedef int esp_err_t;
#define ESP_OK 0
typedef struct {bool ep_output_enable,ep_mode;} epd_ctrl_state_t;
static epd_ctrl_state_t state;
static epd_ctrl_state_t *epd_ctrl_state(void){return &state;}
static unsigned calls,fail;
static int gpio_set_level(unsigned pin,unsigned level){assert(!level);assert(pin==(calls==0?18:19));++calls;return calls==fail?-1:0;}
'''+function('read_pico_epd_release_drive',root/'components/read_pico/read_pico_board.c')+'''
int main(void){for(fail=0;fail<=2;fail++){calls=0;state.ep_output_enable=state.ep_mode=true;assert(read_pico_epd_release_drive()==!fail);assert(calls==2&&!state.ep_output_enable&&!state.ep_mode);}return 0;}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(unit);subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
print('PASS: S31 direct XOE18/MOD19 neutral drive, both writes attempted after every GPIO fault; no I2C/VCOM operations')
