#pragma once
#if DPREADER_BOARD_METALIO47
#if __has_include("../../boards/metalio47/src/main/assets/ui_icons.h")
#include "../../boards/metalio47/src/main/assets/ui_icons.h"
#else
#include "../../../../boards/metalio47/src/main/assets/ui_icons.h"
#endif
#else
#include "../../boards/pico/src/main/assets/ui_icons.h"
#endif
