#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
sdk_path="${METALIO_IDF_PATH:-/private/tmp/metalio-s31-sdk-20261010}"
mkdir -p build/metalio-tests
cc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -Wno-unused-variable -g \
    -fsanitize=address,undefined -Itools/metalio_stubs -Imain \
    -I"$sdk_path/components/esp_app_format/include" -I"$sdk_path/components/bootloader_support/include" \
    tools/metalio_ota_host_test.c main/ota_update.c \
    -o build/metalio-tests/ota-guard
build/metalio-tests/ota-guard
