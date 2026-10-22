#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/book-tests
cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined  -Itools/host_board_pico \
    -Imain -Itools/shelf_waveform_host_stubs -Itools/waveform_host_stubs -Icomponents/epdiy/include -Icomponents/epdiy/src \
    -Icomponents/e0470_epaper_waveform/include -Icomponents/e0470_epaper_waveform/waveforms -Icomponents/e0470_epaper_waveform/metalio \
    tools/shelf_waveform_host_test.c main/display_main_waveform.c \
    components/e0470_epaper_waveform/e0470_epaper_waveform.c \
    components/e0470_epaper_waveform/e0470_waveform_trim.c -o build/book-tests/shelf-waveform
for fail in -1 0 1 2 3; do build/book-tests/shelf-waveform "$fail"; done
