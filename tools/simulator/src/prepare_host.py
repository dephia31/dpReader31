from pathlib import Path
import os
r=Path(__file__).resolve().parents[1]
s=(Path(os.environ.get('PICO_SIM_FIRMWARE',r/'firmware'))/'main/apps/app_book.c').read_text()
start=s.index('static bool kick_prep(void) {');end=s.index('\nstatic void reader_prefetch_end',start)
s=s[:start]+'static bool kick_prep(void) {return false;}\n'+s[end:]
(r/'src/book_preview.c').write_text(s)
