/* SPDX-License-Identifier: Apache-2.0 */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
static bool restore_oom;
static void *restore_alloc(size_t size) { return restore_oom ? NULL : calloc(1, size); }
#define CONFIG_DPREADER_AUTO_BACKUP 1
#define CONFIG_DPREADER_AUTO_RESTORE 1
#define backup_alloc restore_alloc
#define main settings_restore_fixture_main
#include "settings_backup_host_test.c"
#undef main
#undef backup_alloc
uint32_t book_progress_revision(void) { return 0; }

static void erase_profile(void) {
    test_missing_settings_namespace = true;
    test_backup_guard = test_restore_state = -1;
    test_history_empty = true;
    s_book_px = 48;
    app_settings_init();
    assert(app_settings_autorestore_pending());
    assert(test_backup_guard == 0 && test_restore_state == 0);
}
static void seed_snapshot(int px) {
    // Explicit successful manual import/save is the only way out of a blocked journal.
    test_missing_settings_namespace = false;
    test_backup_guard = 1; test_restore_state = 2;
    test_history_restore_fails = test_fail_restore_complete = false;
    test_nvs_init_error = ESP_OK;
    app_settings_init();
    s_font[0]=0;test_persisted_fonts[0][0]=0;
    app_settings_set_book_px(px);
    assert(app_settings_backup_save() == ESP_OK);
}
static long identity_offset(void) {
    return sizeof(settings_backup_v1_t) + 7 + sizeof(settings_backup_profile_t) +
        sizeof(settings_backup_keys_t) + sizeof(settings_backup_hold_t) +
        sizeof(settings_backup_indent_adjust_t) + sizeof(settings_backup_weight_t) +
        sizeof(settings_backup_wifi_t) + sizeof(settings_backup_background_t) +
        sizeof(settings_backup_home_t) + sizeof(settings_backup_background_t) +
        sizeof(settings_backup_indent_adjust_t) + sizeof(reader_backup_t) +
        sizeof(settings_backup_utilities_t);
}
static void corrupt_primary(bool foreign_device) {
    FILE *file = fopen(BACKUP_FILE, "r+b"); assert(file);
    if (foreign_device) {
        backup_identity_t identity;
        assert(!fseek(file, identity_offset(), SEEK_SET));
        assert(fread(&identity, 1, sizeof(identity), file) == sizeof(identity));
        identity.mac[5] ^= 1;
        uint32_t hash = backup_identity_checksum(&identity);
        for (unsigned i=0;i<4;++i)identity.checksum[i]=hash>>(8*i);
        assert(!fseek(file, identity_offset(), SEEK_SET));
        assert(fwrite(&identity, 1, sizeof(identity), file) == sizeof(identity));
    } else { assert(fputc('X', file) != EOF); }
    assert(fclose(file) == 0);
}
static void convert_to_v25(void) {
    unsigned char bytes[8192]; FILE *file = fopen(BACKUP_FILE, "rb"); assert(file);
    size_t len = fread(bytes, 1, sizeof(bytes), file); assert(feof(file)); fclose(file);
    size_t accounts_offset=(size_t)identity_offset()+sizeof(backup_identity_t);
    size_t newer_sections=sizeof(settings_backup_accounts_t)+sizeof(settings_backup_preferences_t);
    assert(len>=accounts_offset+newer_sections);
    memmove(bytes+accounts_offset,bytes+accounts_offset+newer_sections,
            len-accounts_offset-newer_sections);
    len-=newer_sections;
    settings_backup_v1_t base; memcpy(&base,bytes,sizeof(base)); base.magic[7]='P';backup_seal(&base);
    memcpy(bytes,&base,sizeof(base)); uint8_t *ext=bytes+sizeof(base);
    uint32_t hash=backup_shutdown_checksum(&base,ext[0],ext[1],ext[2]);
    for(unsigned i=0;i<4;++i)ext[3+i]=hash>>(8*i);
    settings_backup_profile_t profile;memcpy(&profile,ext+7,sizeof(profile));
    hash=backup_profile_checksum(&base,ext[0],ext[1],ext[2],&profile);
    for(unsigned i=0;i<4;++i)profile.checksum[i]=hash>>(8*i);
    memcpy(ext+7,&profile,sizeof(profile));
    long off=identity_offset();assert(len>(size_t)off+sizeof(backup_identity_t));
    memmove(bytes+off,bytes+off+sizeof(backup_identity_t),len-off-sizeof(backup_identity_t));
    len-=sizeof(backup_identity_t);file=fopen(BACKUP_FILE,"wb");assert(file);
    assert(fwrite(bytes,1,len,file)==len);assert(!fclose(file));remove(BACKUP_PREVIOUS);
}
static void convert_to_v26(void) {
    unsigned char bytes[8192];FILE *file=fopen(BACKUP_FILE,"rb");assert(file);
    size_t len=fread(bytes,1,sizeof(bytes),file);assert(feof(file));fclose(file);
    size_t accounts_offset=(size_t)identity_offset()+sizeof(backup_identity_t);
    size_t newer_sections=sizeof(settings_backup_accounts_t)+sizeof(settings_backup_preferences_t);
    assert(len>=accounts_offset+newer_sections);
    memmove(bytes+accounts_offset,bytes+accounts_offset+newer_sections,
            len-accounts_offset-newer_sections);
    len-=newer_sections;
    settings_backup_v1_t base;memcpy(&base,bytes,sizeof(base));base.magic[7]='Q';backup_seal(&base);
    memcpy(bytes,&base,sizeof(base));uint8_t *ext=bytes+sizeof(base);
    uint32_t hash=backup_shutdown_checksum(&base,ext[0],ext[1],ext[2]);
    for(unsigned i=0;i<4;++i)ext[3+i]=hash>>(8*i);
    settings_backup_profile_t profile;memcpy(&profile,ext+7,sizeof(profile));
    hash=backup_profile_checksum(&base,ext[0],ext[1],ext[2],&profile);
    for(unsigned i=0;i<4;++i)profile.checksum[i]=hash>>(8*i);
    memcpy(ext+7,&profile,sizeof(profile));
    backup_identity_t identity;memcpy(&identity,bytes+identity_offset(),sizeof(identity));identity.reserved[0]=0;
    hash=backup_identity_checksum(&identity);
    for(unsigned i=0;i<4;++i)identity.checksum[i]=hash>>(8*i);
    memcpy(bytes+identity_offset(),&identity,sizeof(identity));
    file=fopen(BACKUP_FILE,"wb");assert(file);assert(fwrite(bytes,1,len,file)==len);assert(!fclose(file));
}
int main(void) {
    (void)mkdir("build",0700);(void)mkdir("build/book-tests",0700);(void)mkdir(APP_SETTINGS_BACKUP_ROOT,0700);
    remove(BACKUP_FILE);remove(BACKUP_PREVIOUS);remove(BACKUP_TEMP);
    seed_snapshot(72);
    unsigned restores=history_restores;
    app_settings_init();assert(app_settings_autorestore_boot()==ESP_OK);
    assert(history_restores==restores); // Healthy reboot never imports stale data.
    erase_profile();
    assert(app_settings_autorestore_boot()==ESP_OK);
    assert(app_settings_autorestore_status()==APP_RESTORE_DONE && s_book_px==72);
    assert(test_restore_state==2 && test_backup_guard==1 && history_restores==restores+1);
    assert(app_settings_autorestore_boot()==ESP_ERR_INVALID_STATE);
    app_settings_init();assert(app_settings_autorestore_boot()==ESP_OK && history_restores==restores+1);

    // Missing font bytes must not erase the saved choice during automatic recovery.
    seed_snapshot(72);strcpy(s_font,"/sdcard/fonts/missing.ttf");
    assert(app_settings_backup_save()==ESP_OK);
    erase_profile();assert(app_settings_autorestore_boot()==ESP_OK);
    assert(!strcmp(app_settings_font_path(),"/sdcard/fonts/missing.ttf"));
    seed_snapshot(72);
    erase_profile();card_mounted=false;restores=history_restores;
    assert(app_settings_autorestore_boot()==ESP_ERR_INVALID_STATE);
    assert(app_settings_autorestore_status()==APP_RESTORE_NEED_MEDIA && test_restore_state==0);
    card_mounted=true;assert(app_settings_autorestore_boot()==ESP_ERR_INVALID_STATE);
    app_settings_init();assert(app_settings_autorestore_boot()==ESP_OK && history_restores==restores+1);

    erase_profile();test_history_empty=false;restores=history_restores;
    assert(app_settings_autorestore_boot()!=ESP_OK && history_restores==restores);
    assert(test_restore_state==3 && app_settings_autorestore_status()==APP_RESTORE_BLOCKED);
    app_settings_init();assert(!app_settings_autorestore_pending());
    test_history_empty=true;assert(app_settings_backup_restore()==ESP_OK);

    seed_snapshot(72);seed_snapshot(74);corrupt_primary(false);
    erase_profile();assert(app_settings_autorestore_boot()==ESP_OK && s_book_px==72);

    seed_snapshot(72);corrupt_primary(true);remove(BACKUP_PREVIOUS);
    erase_profile();restores=history_restores;
    assert(app_settings_autorestore_boot()!=ESP_OK && history_restores==restores && s_book_px==48);
    assert(test_restore_state==3 && app_settings_backup_save()==ESP_ERR_INVALID_STATE);
    app_settings_init();assert(!app_settings_autorestore_pending());

    seed_snapshot(72);convert_to_v26();
    assert(app_settings_set_autobackup_enabled(false)==ESP_OK);
    assert(app_settings_backup_restore()==ESP_OK && !app_settings_autobackup_enabled());
    app_settings_init();assert(!app_settings_autobackup_enabled());
    assert(app_settings_set_autobackup_enabled(true)==ESP_OK);
    seed_snapshot(72);convert_to_v25();erase_profile();restores=history_restores;
    assert(app_settings_autorestore_boot()!=ESP_OK && history_restores==restores);
    assert(app_settings_backup_restore()==ESP_OK && s_book_px==72); // Legacy remains manual.

    seed_snapshot(74);remove(BACKUP_PREVIOUS);erase_profile();test_history_restore_fails=true;
    assert(app_settings_autorestore_boot()!=ESP_OK && test_restore_state==3);
    restores=history_restores;app_settings_init();assert(!app_settings_autorestore_pending());
    assert(app_settings_autorestore_boot()!=ESP_OK && history_restores==restores);
    test_history_restore_fails=false;assert(app_settings_backup_restore()==ESP_OK);

    test_restore_state=1;test_backup_guard=1;app_settings_init();
    assert(!s_auto_backup.healthy && app_settings_autorestore_status()==APP_RESTORE_BLOCKED);
    assert(app_settings_backup_restore()==ESP_OK);

    erase_profile();test_fail_restore_complete=true;
    assert(app_settings_autorestore_boot()!=ESP_OK);
    assert(test_restore_state==3 && app_settings_autorestore_status()==APP_RESTORE_BLOCKED);
    test_fail_restore_complete=false;assert(app_settings_backup_restore()==ESP_OK);

    erase_profile();restore_oom=true;restores=history_restores;
    assert(app_settings_autorestore_boot()==ESP_ERR_NO_MEM && history_restores==restores);
    restore_oom=false;app_settings_init();assert(!app_settings_autorestore_pending());
    assert(app_settings_backup_restore()==ESP_OK);
    erase_profile();test_corrupt_font_readback=true;
    assert(app_settings_autorestore_boot()!=ESP_OK && test_restore_state==3);
    test_corrupt_font_readback=false;assert(app_settings_backup_restore()==ESP_OK);
    test_nvs_init_error=ESP_ERR_NVS_NO_FREE_PAGES;app_settings_init();
    assert(app_settings_autorestore_boot()!=ESP_OK && app_settings_autorestore_status()==APP_RESTORE_BLOCKED);
    assert(!test_nvs_erases);
    puts("PASS auto restore: healthy skip, same-device recovery, one attempt, missing media, surviving history, previous fallback, foreign/legacy rejection, interrupted/failed journal, OOM, corrupt NVS without erase, manual recovery");
    return 0;
}
