/* SPDX-License-Identifier: Apache-2.0 */
#define main existing_settings_backup_fixture_main
#include "settings_backup_host_test.c"
#undef main
int main(void) {
    (void)mkdir("build",0700);
    (void)mkdir("build/book-tests",0700);
    (void)mkdir(APP_SETTINGS_BACKUP_ROOT,0700);
    app_settings_init();
    strlcpy(s_kosync_url,"https://sync.example.test",sizeof(s_kosync_url));
    strlcpy(s_kosync_user,"test-reader",sizeof(s_kosync_user));
    strlcpy(s_kosync_key,"test-key",sizeof(s_kosync_key));
    strlcpy(s_opds_url,"https://books.example.test/opds",sizeof(s_opds_url));
    strlcpy(s_opds_user,"test-library",sizeof(s_opds_user));
    strlcpy(s_opds_pass,"test-password",sizeof(s_opds_pass));
    s_kosync_auto=true;
    s_book_footer_enabled=false;
    s_book_footer_page=false;s_book_footer_chapter=true;s_book_footer_pct=false;
    s_reader_turn_mode=2;s_reader_full_pages=10;
    s_sleep_off_hours=4;s_wake_presses=2;s_wifi_timeout_minutes=10;s_ble_timeout_minutes=30;
    s_ota_edition=1;s_book_status_hidden=true;s_pin_grace_code_v=4;
    s_ai_short=APP_AI_BOOKMARK;s_action_menu=(1u<<APP_AI_TOOLS);s_power_action_menu=true;
    const uint8_t holds[7]={APP_READER_KEY_PREV,APP_READER_KEY_NEXT,APP_READER_KEY_HOME,
        APP_READER_KEY_TOOLS,APP_CONTROL_HOLD_DEFAULT,APP_READER_KEY_REFRESH,APP_READER_KEY_NONE};
    memcpy(s_control_holds,holds,sizeof(holds));
    s_reader_hold_action=APP_READER_KEY_NEXT;
    assert(app_settings_backup_save()==ESP_OK);
    s_kosync_user[0]=s_kosync_key[0]=s_opds_user[0]=s_opds_pass[0]=0;
    s_kosync_auto=false;s_book_footer_enabled=true;
    s_book_footer_page=true;s_book_footer_chapter=false;s_book_footer_pct=true;
    s_reader_turn_mode=0;s_reader_full_pages=1;
    s_sleep_off_hours=0;s_wake_presses=1;s_wifi_timeout_minutes=0;s_ble_timeout_minutes=0;
    s_ota_edition=255;s_book_status_hidden=false;s_pin_grace_code_v=0;
    s_ai_short=APP_AI_REFRESH;s_action_menu=APP_ACTION_MENU_DEFAULT;s_power_action_menu=false;
    memset(s_control_holds,APP_CONTROL_HOLD_DEFAULT,sizeof(s_control_holds));
    s_reader_hold_action=APP_READER_KEY_NONE;
    assert(app_settings_backup_restore()==ESP_OK);
    assert(!strcmp(s_kosync_url,"https://sync.example.test"));
    assert(!strcmp(s_kosync_user,"test-reader")&&!strcmp(s_kosync_key,"test-key"));
    assert(!strcmp(s_opds_url,"https://books.example.test/opds"));
    assert(!strcmp(s_opds_user,"test-library")&&!strcmp(s_opds_pass,"test-password"));
    assert(s_kosync_auto&&!s_book_footer_enabled);
    assert(!s_book_footer_page&&s_book_footer_chapter&&!s_book_footer_pct);
    assert(s_reader_turn_mode==2&&s_reader_full_pages==10&&wifi_imports==1);
    assert(s_sleep_off_hours==4&&s_wake_presses==2&&s_wifi_timeout_minutes==10&&s_ble_timeout_minutes==30);
    assert(s_ota_edition==1&&s_book_status_hidden&&s_pin_grace_code_v==4);
    assert(s_ai_short==APP_AI_BOOKMARK&&s_action_menu==(1u<<APP_AI_TOOLS)&&s_power_action_menu);
    assert(app_settings_control_hold(4)==APP_READER_KEY_NEXT);
    for(unsigned i=0;i<7;++i)if(i!=4)assert(app_settings_control_hold(i)==holds[i]);
    FILE *file=fopen(BACKUP_FILE,"r+b");assert(file);
    backup_workspace_t *work=calloc(1,sizeof(*work));assert(work);
    assert(fread(&work->backup,1,sizeof(work->backup),file)==sizeof(work->backup));
    assert(backup_version(&work->backup)==32);
    long offset=sizeof(work->backup)+sizeof(work->extension)+sizeof(work->profile)+sizeof(work->keys)+
        sizeof(work->hold)+sizeof(work->adjust)+sizeof(work->weight)+sizeof(work->wifi)+
        sizeof(work->background)+sizeof(work->home)+sizeof(work->bookmark)+sizeof(work->word_spacing)+
        sizeof(work->reader)+sizeof(work->utilities)+sizeof(work->identity);
    assert(!fseek(file,offset,SEEK_SET));
    assert(fread(&work->accounts,1,sizeof(work->accounts),file)==sizeof(work->accounts));
    assert(backup_accounts_valid(&work->accounts));
    settings_backup_preferences_t invalid;
    long preferences_offset=offset+sizeof(work->accounts);
    assert(fread(&invalid,1,sizeof(invalid),file)==sizeof(invalid));
    assert(preferences_valid(&invalid));
    invalid.wake_presses=0;
    uint32_t invalid_hash=preferences_checksum(&invalid);
    for(unsigned i=0;i<4;++i)invalid.checksum[i]=invalid_hash>>(8*i);
    assert(!fseek(file,preferences_offset,SEEK_SET));
    assert(fwrite(&invalid,1,sizeof(invalid),file)==sizeof(invalid));
    assert(!fclose(file));
    int before=commit_count;
    assert(app_settings_backup_restore()==ESP_ERR_INVALID_RESPONSE&&commit_count==before);
    assert(app_settings_backup_save()==ESP_OK);
    file=fopen(BACKUP_FILE,"r+b");assert(file);
    work->accounts.opds_pass[0]^=1;
    assert(!fseek(file,offset,SEEK_SET));
    assert(fwrite(&work->accounts,1,sizeof(work->accounts),file)==sizeof(work->accounts));fclose(file);
    before=commit_count;
    assert(app_settings_backup_restore()==ESP_ERR_INVALID_RESPONSE&&commit_count==before);
    assert(app_settings_backup_save()==ESP_OK);
    /* V31 and V30 has no account block. Seal the legacy chained profile checksums. */
    file=fopen(BACKUP_FILE,"r+b");assert(file);
    assert(fread(&work->backup,1,sizeof(work->backup),file)==sizeof(work->backup));
    assert(fread(work->extension,1,sizeof(work->extension),file)==sizeof(work->extension));
    assert(fread(&work->profile,1,sizeof(work->profile),file)==sizeof(work->profile));
    assert(!fseek(file,preferences_offset+sizeof(work->preferences),SEEK_SET));
    uint8_t history[4096];size_t bytes=fread(history,1,sizeof(history),file);assert(feof(file));
    for (unsigned version = 31; version >= 30; --version) {
        if (version == 30) { file=fopen(BACKUP_FILE,"r+b");assert(file); }
        work->backup.magic[7]=version==31?'V':'U';backup_seal(&work->backup);
        uint32_t hash=backup_shutdown_checksum(&work->backup,work->extension[0],work->extension[1],work->extension[2]);
        for(unsigned i=0;i<4;++i)work->extension[3+i]=hash>>(8*i);
        hash=backup_profile_checksum(&work->backup,work->extension[0],work->extension[1],work->extension[2],&work->profile);
        for(unsigned i=0;i<4;++i)work->profile.checksum[i]=hash>>(8*i);
        rewind(file);assert(fwrite(&work->backup,1,sizeof(work->backup),file)==sizeof(work->backup));
        assert(fwrite(work->extension,1,sizeof(work->extension),file)==sizeof(work->extension));
        assert(fwrite(&work->profile,1,sizeof(work->profile),file)==sizeof(work->profile));
        long history_offset=version==31?preferences_offset:offset;
        assert(!fseek(file,history_offset,SEEK_SET));assert(fwrite(history,1,bytes,file)==bytes);
        assert(!fflush(file)&&!ftruncate(fileno(file),history_offset+bytes));fclose(file);
        strcpy(s_kosync_user,"keep-account");
        s_wake_presses=1;s_book_status_hidden=false;s_control_holds[0]=APP_READER_KEY_HOME;
        assert(app_settings_backup_restore()==ESP_OK);
        assert(!strcmp(s_kosync_user,version==31?"test-reader":"keep-account"));
        assert(s_wake_presses==1&&!s_book_status_hidden&&s_control_holds[0]==APP_READER_KEY_HOME);
    }
    free(work);
    puts("PASS V32 settings round trip; invalid data rejected before writes; V31 and V30 retain preferences they do not carry");
}
