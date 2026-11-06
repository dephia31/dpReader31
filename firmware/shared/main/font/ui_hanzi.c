/* SPDX-License-Identifier: Apache-2.0
 * Bộ chữ Hán nén ui_hanzi đã được lược bỏ trong dpReader vì toàn bộ ký tự Tiếng Việt
 * đã được nhúng trực tiếp vào vector font builtin.ttf.
 */
#include "ui_hanzi.h"
#include <stdbool.h>
#include <stdint.h>

bool ui_hanzi_has(uint32_t cp) {
    (void)cp;
    return false;
}

bool ui_hanzi_get(uint32_t cp, uint8_t record[UI_HANZI_RECORD]) {
    (void)cp;
    (void)record;
    return false;
}
