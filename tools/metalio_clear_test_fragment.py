from pathlib import Path
from home_cover_cache_host_test import function
r=Path(__file__).resolve().parents[1]
(r/'build/book-tests/metalio_clear_checked.inc').write_text(function('epd_clear_checked',r/'components/epdiy/src/render.c')+'\n')
