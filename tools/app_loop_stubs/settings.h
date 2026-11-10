/* 中文：调度测试替身。/ English: Scheduler test shim. */
#pragma once
#include "common.h"

bool app_settings_simple_ui(void);
bool app_settings_simple_full_ui(void);
bool app_settings_ble_turner(void);
void app_settings_set_ble_turner(bool enabled);

uint8_t app_settings_auto_lock_minutes(void);

uint8_t app_settings_system_contrast(void);

#if DPREADER_BOARD_METALIO47
typedef enum {APP_AI_REFRESH,APP_AI_BOOKMARK,APP_AI_FRONTLIGHT,APP_AI_HOME,APP_AI_NONE,APP_AI_KOSYNC,APP_AI_PREV_PAGE,APP_AI_NEXT_PAGE,APP_AI_PREV_CHAPTER,APP_AI_NEXT_CHAPTER,APP_AI_TOOLS,APP_AI_ACTION_MENU,APP_AI_LOCK,APP_AI_INVERT,APP_AI_ACTION_COUNT}app_ai_action_t;
#define APP_AI_MENU_COUNT 10
app_ai_action_t app_settings_ai_short(void);
esp_err_t app_settings_set_ai_short(app_ai_action_t action);
#endif

typedef enum {APP_READER_KEY_PREV,APP_READER_KEY_NEXT,APP_READER_KEY_TOOLS,APP_READER_KEY_FULLSCREEN,APP_READER_KEY_HOME,APP_READER_KEY_REFRESH,APP_READER_KEY_NONE,APP_READER_KEY_PREV_CHAPTER,APP_READER_KEY_NEXT_CHAPTER,APP_READER_KEY_LIGHT,APP_READER_KEY_KOSYNC,APP_READER_KEY_ACTION_MENU,APP_READER_KEY_LOCK,APP_READER_KEY_INVERT,APP_READER_KEY_COUNT}app_reader_key_action_t;
#define APP_CONTROL_HOLD_DEFAULT 255
static inline uint8_t app_settings_control_hold(unsigned key){(void)key;return APP_CONTROL_HOLD_DEFAULT;}
static inline app_reader_key_action_t app_settings_reader_key_action(unsigned key){return key==0?APP_READER_KEY_PREV:key==1?APP_READER_KEY_TOOLS:APP_READER_KEY_NEXT;}
static inline app_reader_key_action_t app_settings_volume_key_action(unsigned key){return key?APP_READER_KEY_NEXT:APP_READER_KEY_PREV;}

static inline bool app_settings_power_action_menu(void){return false;}
static inline unsigned app_settings_wifi_timeout_minutes(void){return 0;}
static inline unsigned app_settings_ble_timeout_minutes(void){return 0;}

static inline bool app_settings_screen_inverted(void){return false;}
