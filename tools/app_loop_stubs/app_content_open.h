#pragma once
#include <stdbool.h>

static inline bool app_book_request_open(const char *path) { (void)path; return true; }
static inline bool app_image_request_open(const char *path) { (void)path; return true; }
static inline void app_files_request_folder(int folder) { (void)folder; }
static inline void app_book_request_manage(void) {}
static inline bool app_book_reader_body_visible(void) { return false; }
static inline bool app_book_reader_open(void) { return false; }

#include <stddef.h>
bool app_book_resume_context(char *path, size_t capacity, bool *fullscreen);

void app_book_cover_mode_changed(void);
void app_home_cover_mode_changed(void);

#if DPREADER_BOARD_METALIO47
bool app_book_volume_key(app_ctx_t *ctx, int direction, app_redraw_t *redraw);
bool app_book_ai_bookmark(void);
#endif

static inline app_redraw_t app_book_handle_key_action(app_ctx_t *ctx,int action){(void)ctx;(void)action;return APP_REDRAW_NONE;}
static inline void app_book_request_kosync(app_ctx_t *ctx){(void)ctx;}

static inline bool app_book_background_radio_ready(void){return true;}
