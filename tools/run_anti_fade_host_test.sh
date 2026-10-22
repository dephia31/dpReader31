#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/book-tests
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g -Itools/anti_fade_stubs -Imain tools/anti_fade_host_test.c -o build/book-tests/anti-fade
build/book-tests/anti-fade
