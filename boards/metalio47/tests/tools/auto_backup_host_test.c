/* SPDX-License-Identifier: Apache-2.0 */
/* Exercise checkpoint retries and snapshot rotation using the real serializer. */
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
static void *test_backup_alloc(size_t size);
static size_t test_backup_write(const void *data, size_t size, size_t count, FILE *file);
static int test_backup_rename(const char *from, const char *to);
#define CONFIG_DPREADER_AUTO_BACKUP 1
#define backup_alloc test_backup_alloc
#define fwrite test_backup_write
#define rename test_backup_rename
#define main backup_fixture_main
#include "settings_backup_host_test.c"
#undef main
#undef fwrite
#undef rename
#undef backup_alloc

static bool fail_alloc, fail_write, fail_promote;
static uint32_t progress_revision;
uint32_t book_progress_revision(void) { return progress_revision; }
static void *test_backup_alloc(size_t size) { return fail_alloc ? NULL : calloc(1, size); }
static size_t test_backup_write(const void *data, size_t size, size_t count, FILE *file) {
    return fail_write ? 0 : fwrite(data, size, count, file);
}
static int test_backup_rename(const char *from, const char *to) {
    if (fail_promote && !strcmp(from, BACKUP_TEMP)) { fail_promote = false; errno = EIO; return -1; }
    return rename(from, to);
}
static bool exists(const char *path) { struct stat st; return stat(path, &st) == 0; }
static unsigned long digest(const char *path) {
    FILE *file = fopen(path, "rb"); assert(file);
    unsigned long hash = 5381; int c;
    while ((c = fgetc(file)) != EOF) hash = hash * 33 + (unsigned char)c;
    assert(!ferror(file)); assert(fclose(file) == 0); return hash;
}
int main(void) {
    (void)mkdir("build", 0700); (void)mkdir("build/book-tests", 0700);
    (void)mkdir(APP_SETTINGS_BACKUP_ROOT, 0700);
    remove(BACKUP_FILE); remove(BACKUP_PREVIOUS); remove(BACKUP_TEMP);
    app_settings_init();
    assert(s_auto_backup.healthy && app_settings_autobackup_enabled());
    assert(app_settings_set_autobackup_enabled(false)==ESP_OK);
    app_settings_init();assert(!app_settings_autobackup_enabled());
    card_mounted=false;fail_alloc=true;
    assert(app_settings_autobackup_checkpoint()==ESP_OK && history_saves==0 && !exists(BACKUP_FILE));
    card_mounted=true;fail_alloc=false;
    assert(app_settings_backup_save()==ESP_OK); // Manual backup still works while automatic is off.
    assert(app_settings_set_autobackup_enabled(true)==ESP_OK);
    assert(app_settings_backup_restore()==ESP_OK && !app_settings_autobackup_enabled());
    app_settings_init();assert(!app_settings_autobackup_enabled());
    assert(app_settings_set_autobackup_enabled(true)==ESP_OK);
    commit_fails=true;
    assert(app_settings_set_autobackup_enabled(false)!=ESP_OK && app_settings_autobackup_enabled());
    commit_fails=false;test_auto_backup=1;app_settings_init();
    assert(app_settings_autobackup_enabled());
    remove(BACKUP_FILE);remove(BACKUP_PREVIOUS);history_saves=history_restores=0;
    assert(app_settings_autobackup_checkpoint() == ESP_OK);
    assert(exists(BACKUP_FILE) && !exists(BACKUP_PREVIOUS));
    unsigned saves = history_saves;
    assert(app_settings_autobackup_checkpoint() == ESP_OK && history_saves == saves);
    remove(BACKUP_FILE);
    assert(app_settings_autobackup_checkpoint() == ESP_OK && history_saves == saves + 1);
    saves = history_saves;
    app_settings_autobackup_media_changed();
    assert(app_settings_autobackup_checkpoint() == ESP_OK && history_saves == saves + 1);
    unsigned long first = digest(BACKUP_FILE);
    app_settings_set_book_px(72);
    assert(app_settings_autobackup_checkpoint() == ESP_OK);
    assert(digest(BACKUP_PREVIOUS) == first);
    unsigned long latest = digest(BACKUP_FILE);
    assert(latest != first);
    s_book_px = 48;
    assert(app_settings_backup_restore() == ESP_OK && s_book_px == 72);
    assert(app_settings_autobackup_checkpoint() == ESP_OK);
    latest = digest(BACKUP_FILE);
    ++progress_revision; card_mounted = false;
    assert(app_settings_autobackup_checkpoint() == ESP_ERR_INVALID_STATE);
    card_mounted = true; fail_alloc = true;
    assert(app_settings_autobackup_checkpoint() == ESP_ERR_NO_MEM);
    assert(digest(BACKUP_FILE) == latest);
    fail_alloc = false; fail_write = true;
    assert(app_settings_autobackup_checkpoint() != ESP_OK);
    assert(digest(BACKUP_FILE) == latest && !exists(BACKUP_TEMP));
    fail_write = false; fail_promote = true;
    assert(app_settings_autobackup_checkpoint() != ESP_OK);
    assert(digest(BACKUP_FILE) == latest);
    assert(app_settings_autobackup_checkpoint() == ESP_OK);
    saves = history_saves;
    assert(app_settings_autobackup_checkpoint() == ESP_OK && saves == history_saves);
    s_auto_backup.healthy = false; ++progress_revision;
    assert(app_settings_autobackup_checkpoint() == ESP_ERR_INVALID_STATE && saves == history_saves);
    s_auto_backup.healthy = true;
    commit_fails = true; app_settings_set_book_px(74); commit_fails = false;
    assert(s_auto_backup.failed);
    assert(app_settings_autobackup_checkpoint() == ESP_ERR_INVALID_STATE && saves == history_saves);
    s_auto_backup.failed = false;
    assert(app_settings_autobackup_checkpoint() == ESP_OK);
    // WiFi lives outside the settings namespace and must participate in change detection.
    saves = history_saves;
    strcpy(saved_wifi.ssid, "UpdatedNetwork");
    assert(app_settings_autobackup_checkpoint() == ESP_OK && history_saves == saves + 1);
    saves = history_saves;
    assert(app_settings_autobackup_checkpoint() == ESP_OK && history_saves == saves);
    // Recoverable interrupted promotion: previous exists but primary is absent.
    assert(rename(BACKUP_FILE, BACKUP_PREVIOUS) == 0);
    latest = digest(BACKUP_PREVIOUS);
    ++progress_revision; fail_promote = true;
    assert(app_settings_autobackup_checkpoint() != ESP_OK);
    assert(!exists(BACKUP_FILE) && digest(BACKUP_PREVIOUS) == latest);
    assert(app_settings_backup_restore() == ESP_OK);
    assert(app_settings_autobackup_checkpoint() == ESP_OK);
    assert(exists(BACKUP_FILE) && exists(BACKUP_PREVIOUS));
    FILE *corrupt = fopen(BACKUP_FILE, "r+b"); assert(corrupt);
    int byte = fgetc(corrupt); assert(byte != EOF); rewind(corrupt);
    assert(fputc(byte ^ 1, corrupt) != EOF && fclose(corrupt) == 0);
    int commits = commit_count;
    assert(app_settings_backup_restore() == ESP_ERR_INVALID_RESPONSE && commit_count == commits);
    corrupt = fopen(BACKUP_FILE, "r+b"); assert(corrupt);
    assert(fputc(byte, corrupt) != EOF && fclose(corrupt) == 0);
    // Erased NVS remains quarantined across reboots despite migrations creating defaults.
    saves = history_saves; test_missing_settings_namespace = true; test_backup_guard = -1;
    app_settings_init();
    assert(!s_auto_backup.healthy && test_backup_guard == 0);
    assert(app_settings_autobackup_checkpoint() == ESP_ERR_INVALID_STATE && history_saves == saves);
    app_settings_init();
    assert(!s_auto_backup.healthy);
    assert(app_settings_autobackup_checkpoint() == ESP_ERR_INVALID_STATE && history_saves == saves);
    assert(app_settings_backup_restore() == ESP_OK && s_auto_backup.healthy && test_backup_guard == 1);
    app_settings_init(); assert(s_auto_backup.healthy);
    puts("PASS auto backup: changes, no-op checkpoints, manual restore, retained previous snapshot, absent card, OOM, failed writes/promotion, retry, unhealthy NVS guard");
    return 0;
}
