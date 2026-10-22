#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/metalio-tests
cc -std=c11 -Wall -Wextra -Werror -Wno-unknown-attributes -Wno-sign-compare -Wno-unused-parameter -Wno-unused-function -g -fsanitize=address,undefined  -Itools/host_board_metalio \
    -include tools/metalio_stubs/sdkconfig.h \
    -Itools/metalio_stubs -Icomponents/epdiy/src -Icomponents/epdiy/include \
    -Icomponents/e0470_epaper_waveform/include -Icomponents/e0470_epaper_waveform/waveforms \
    -Icomponents/e0470_epaper_waveform/metalio -Icomponents/cst836u/include \
    tools/metalio_host_test.c components/epdiy/src/riscv_pixels.c \
    components/epdiy/src/output_common/lut.c components/epdiy/src/output_common/render_context.c \
    components/e0470_epaper_waveform/e0470_epaper_waveform.c \
    components/e0470_epaper_waveform/e0470_waveform_trim.c \
    components/cst836u/metalio_touch_decode.c -o build/metalio-tests/hardware-logic
build/metalio-tests/hardware-logic
