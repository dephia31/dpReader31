#pragma once
#include <stdbool.h>

#if KIIKO_BOARD_METALIO47
static inline bool usb_storage_active(void) { return false; }
#else
bool test_usb_storage_active(void);
static inline bool usb_storage_active(void) { return test_usb_storage_active(); }
#endif
