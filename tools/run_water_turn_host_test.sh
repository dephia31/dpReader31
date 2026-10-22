#!/usr/bin/env bash
# 实际组件的方向、相位和失败基准回归。/ Exercise the real component's directions, phases and failure baseline.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/book-tests
for phases in 37 48; do
cc -DTEST_PHASES="$phases" -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined  -Itools/host_board_pico \
    -Itools/water_turn_stubs -Icomponents/e0470_page_turn/include \
    tools/water_turn_host_test.c components/e0470_page_turn/e0470_page_turn.c \
    -o "build/book-tests/water-turn-$phases"
"build/book-tests/water-turn-$phases"
done
