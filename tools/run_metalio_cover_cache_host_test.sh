#!/bin/sh
set -eu
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
cc -DKIIKO_BOARD_METALIO47=1 -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -pthread \
  -Itools/metalio_cache_host_stubs -Imain/book \
  tools/metalio_cover_cache_host_test.c main/book/metalio_cover_cache.c -o "$out/test"
"$out/test"
