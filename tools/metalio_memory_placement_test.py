"""中文：检查最终ELF的CPU大表位于PSRAM，DMA端点保持内部。/ English: Check final CPU tables in PSRAM and DMA endpoints in internal RAM."""
import argparse,json,subprocess
p=argparse.ArgumentParser();p.add_argument('--nm',required=True);p.add_argument('--elf',required=True);a=p.parse_args()
symbols={}
for line in subprocess.check_output([a.nm,'-S','--size-sort',a.elf],text=True).splitlines():
 fields=line.split()
 if len(fields)==4:symbols[fields[3]]=(int(fields[0],16),int(fields[1],16))
names=['s_log_buf','s_slots','s_gray','s_bw','s_page_gray','e0470_full_gl16_live','e0470_gc16_data','e0470_gl16_data','e0470_textturn_gl16_data','s_auto_push','s_auto_pull','s_auto_running','s_auto_result','s_devices','s_bonds','s_opds_history','s_line_runs','s_io_path']
for name in names:
 address,size=symbols[name];assert 0x50000000<=address<0x51000000,(name,hex(address))
address,size=symbols['_mscd_epbuf'];assert 0x2f000000<=address<0x2f080000
print('PASS: %d CPU objects in S31 external BSS (%d bytes); TinyUSB endpoint remains internal'%(len(names),sum(symbols[n][1] for n in names)))
