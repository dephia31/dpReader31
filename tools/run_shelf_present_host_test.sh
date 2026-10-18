#!/usr/bin/env bash
# 同步书架分区输出与真实前帧回归。/ Test simultaneous shelf regions and the real previous-frame baseline.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/book-tests
cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
    -Itools/water_turn_stubs -Icomponents/e0470_page_turn/include \
    tools/shelf_present_host_test.c components/e0470_page_turn/e0470_page_turn.c \
    -o build/book-tests/shelf-present
build/book-tests/shelf-present
