from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
p=Path(__file__).resolve().parents[1]/'components/ble_page_turner/src/ble_page_turner.c'
s=p.read_text();code='#include <stdint.h>\n#include <assert.h>\n'+function('reconnect_delay_ms',p)+'''\nint main(void){assert(reconnect_delay_ms(0)==4000);assert(reconnect_delay_ms(1)==8000);assert(reconnect_delay_ms(2)==16000);assert(reconnect_delay_ms(3)==32000);for(unsigned n=4;n<1000;n++)assert(reconnect_delay_ms(n)==60000);}'''
assert 's_link_up = true;\n            s_reconnect_failures=0;' in s
assert 's_reconnect_failures=0;' in function('reconnect_recent_requested',p)
assert 'reconnect_delay_ms(s_reconnect_failures)' in function('poll_locked',p)
with tempfile.TemporaryDirectory() as t:
 d=Path(t);(d/'test.c').write_text(code)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(d/'test.c'),'-o',str(d/'test')],check=True);subprocess.run([str(d/'test')],check=True)
print('PASS bounded BLE retry4/8/16/32/60s, success/manual reset and poll wiring')
