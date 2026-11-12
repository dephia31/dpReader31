/* SPDX-License-Identifier: Apache-2.0 */
#define main existing_settings_backup_fixture_main
#include "settings_backup_host_test.c"
#undef main
int main(void) {
    (void)mkdir("build",0700);(void)mkdir("build/book-tests",0700);(void)mkdir(APP_SETTINGS_BACKUP_ROOT,0700);
    app_settings_init();
    assert(!app_settings_book_text_contrast());
    assert(app_settings_set_book_text_contrast(true));
    app_settings_init();
    assert(app_settings_book_text_contrast());
    commit_fails=true;
    assert(!app_settings_set_book_text_contrast(false)&&app_settings_book_text_contrast());
    commit_fails=false;
    app_settings_set_reader_turn_effect(APP_READER_TURN_FLASH);
    s_book_px=72;s_book_tracking=23;s_book_weight_fine=-17;s_book_line=186;s_book_para=251;s_book_word_spacing=-10;
    s_volume_keys[0]=APP_READER_KEY_TOOLS;s_reader_edge_warmth=false;
    s_book_align=3;s_book_progress_style=APP_BOOK_PROGRESS_STYLE_BOTTOM;s_book_footer_pct=false;s_reader_keys[0]=APP_READER_KEY_HOME;
    assert(app_settings_backup_save()==ESP_OK);
    reader_backup_t original;reader_backup_capture(&original);assert(reader_backup_valid(&original));
    for(unsigned i=0;i<READER_BACKUP_FIELDS;++i)memset(s_reader_backup_fields[i].state,0,1);
    assert(app_settings_set_book_text_contrast(false));
    int before=commit_count;assert(app_settings_backup_restore()==ESP_OK&&commit_count==before+1);
    reader_backup_t restored;reader_backup_capture(&restored);assert(!memcmp(&original,&restored,sizeof(original)));
    assert(app_settings_book_text_contrast()&&loaded_text_contrast==1);
    assert(history_restores==1&&wifi_imports==1);
    assert(app_settings_reader_turn_effect()==APP_READER_TURN_FLASH);
    app_settings_set_reader_turn_effect(0);
    app_settings_set_reader_turn_effect(APP_READER_TURN_FLASH);
    s_reader_turn_effect=0;app_settings_init();
    assert(app_settings_reader_turn_effect()==APP_READER_TURN_FLASH);
    assert(loaded_tracking==192+23);s_book_tracking=255;app_settings_init();assert(s_book_tracking==23);
    FILE *file=fopen(BACKUP_FILE,"r+b");assert(file);
    long offset=sizeof(settings_backup_v1_t)+7+sizeof(settings_backup_profile_t)+sizeof(settings_backup_keys_t)+sizeof(settings_backup_hold_t)+sizeof(settings_backup_indent_adjust_t)+sizeof(settings_backup_weight_t)+sizeof(settings_backup_wifi_t)+sizeof(settings_backup_background_t)+sizeof(settings_backup_home_t)+sizeof(settings_backup_background_t)+sizeof(settings_backup_indent_adjust_t);
    assert(!fseek(file,offset+offsetof(reader_backup_t,values),SEEK_SET));int byte=fgetc(file);assert(byte!=EOF);assert(!fseek(file,-1,SEEK_CUR));fputc(byte^1,file);fclose(file);
    before=commit_count;assert(app_settings_backup_restore()==ESP_ERR_INVALID_RESPONSE&&commit_count==before);
    assert(app_settings_backup_save()==ESP_OK);
    file=fopen(BACKUP_FILE,"r+b");assert(file);reader_backup_t bad=original;bad.values[2]=2;
    uint32_t badhash=reader_backup_checksum(&bad);for(unsigned i=0;i<4;++i)bad.checksum[i]=badhash>>(8*i);
    assert(!fseek(file,offset,SEEK_SET));assert(fwrite(&bad,1,sizeof(bad),file)==sizeof(bad));fclose(file);
    before=commit_count;assert(app_settings_backup_restore()==ESP_ERR_INVALID_RESPONSE&&commit_count==before);
    assert(app_settings_backup_save()==ESP_OK);commit_fails=true;s_book_px=48;
    assert(app_settings_backup_restore()!=ESP_OK&&s_book_px==48);commit_fails=false;
    card_mounted=false;assert(app_settings_backup_save()==ESP_ERR_INVALID_STATE);assert(app_settings_backup_restore()==ESP_ERR_INVALID_STATE);card_mounted=true;
    file=fopen(BACKUP_FILE,"rb");assert(file);unsigned char bytes[4096];size_t len=fread(bytes,1,sizeof(bytes),file);assert(feof(file));fclose(file);
    settings_backup_v1_t base;memcpy(&base,bytes,sizeof(base));base.magic[7]='L';backup_seal(&base);memcpy(bytes,&base,sizeof(base));
    uint8_t *ext=bytes+sizeof(base);uint32_t hash=backup_shutdown_checksum(&base,ext[0],ext[1],ext[2]);
    for(unsigned i=0;i<4;++i)ext[3+i]=hash>>(8*i);
    settings_backup_profile_t profile;memcpy(&profile,ext+7,sizeof(profile));hash=backup_profile_checksum(&base,ext[0],ext[1],ext[2],&profile);
    for(unsigned i=0;i<4;++i)profile.checksum[i]=hash>>(8*i);
    memcpy(ext+7,&profile,sizeof(profile));
    size_t removed=sizeof(reader_backup_t)+sizeof(settings_backup_utilities_t)+
        sizeof(backup_identity_t)+sizeof(settings_backup_accounts_t)+sizeof(settings_backup_preferences_t);
    assert(len>=(size_t)offset+removed);
    memmove(bytes+offset,bytes+offset+removed,len-offset-removed);
    len-=removed;
    file=fopen(BACKUP_FILE,"wb");assert(file);assert(fwrite(bytes,1,len,file)==len);fclose(file);
    assert(app_settings_backup_restore()==ESP_OK);
    puts("PASS unified backup: all reader fields, WiFi and history restored in one operation; tracking survives reboot; corrupt reader block rejected before commit");
}
