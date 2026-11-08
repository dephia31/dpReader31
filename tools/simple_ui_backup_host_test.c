/* SPDX-License-Identifier: Apache-2.0 */
#define main existing_settings_backup_fixture_main
#include "settings_backup_host_test.c"
#undef main
int main(void) {
    (void)mkdir("build",0700);(void)mkdir("build/book-tests",0700);(void)mkdir(APP_SETTINGS_BACKUP_ROOT,0700);
    app_settings_init();assert(!app_settings_simple_ui());
    stored_ui_choice=1;s_simple_choice_known=false;app_settings_init();
    assert(app_settings_simple_full_ui()&&app_settings_ui_mode()==2);
    assert(app_settings_set_ui_mode(1)==ESP_OK&&stored_ui_choice==2&&app_settings_ui_mode()==2);
    assert(app_settings_set_ui_mode(0)==ESP_OK);

    assert(app_settings_set_ui_mode(2)==ESP_OK&&app_settings_simple_ui()&&app_settings_simple_full_ui());
    app_settings_init();assert(app_settings_ui_mode()==2);
    commit_fails=true;assert(app_settings_set_ui_mode(0)!=ESP_OK&&app_settings_ui_mode()==2);commit_fails=false;
    assert(app_settings_set_ui_mode(3)==ESP_ERR_INVALID_ARG&&app_settings_ui_mode()==2);
    assert(app_settings_set_ui_mode(0)==ESP_OK&&!app_settings_simple_full_ui());
    assert(app_settings_set_simple_ui(true)==ESP_OK&&app_settings_simple_ui());
    app_settings_init();assert(app_settings_simple_ui());
    commit_fails=true;
    assert(app_settings_set_simple_ui(false)!=ESP_OK&&app_settings_simple_ui());commit_fails=false;
    ui_get_fails=true;app_settings_init();assert(app_settings_simple_ui());ui_get_fails=false;
    ui_open_fails=true;app_settings_init();assert(app_settings_simple_ui());ui_open_fails=false;
    stored_ui_choice=-1;loaded_simple_ui=0;app_settings_init();assert(app_settings_simple_ui());
    assert(app_settings_set_simple_ui(true)==ESP_OK);
    /* Simulate cold boot after the profile namespace was reset/restored to OFF. */
    s_simple_ui=false;s_simple_choice_known=false;loaded_simple_ui=0;app_settings_init();assert(app_settings_simple_ui());
    /* One-time upgrade from RC5's profile key preserves ON. */
    stored_ui_choice=-1;s_simple_ui=false;s_simple_choice_known=false;loaded_simple_ui=1;
    app_settings_init();assert(app_settings_simple_ui()&&stored_ui_choice==1);
    stored_ui_choice=7;s_simple_ui=false;s_simple_choice_known=false;app_settings_init();assert(app_settings_simple_ui());
    test_nvs_init_error=ESP_FAIL;s_simple_ui=false;s_simple_choice_known=false;app_settings_init();assert(app_settings_simple_ui());test_nvs_init_error=ESP_OK;
    assert(app_settings_set_simple_ui(true)==ESP_OK);
    const uint8_t simple_actions[2]={APP_HOME_UTIL_TRANSFER,APP_HOME_UTIL_OPDS};
    uint8_t regular[4];for(unsigned i=0;i<4;++i)regular[i]=app_settings_home_utility(i);
    assert(app_settings_set_simple_utilities(simple_actions)==ESP_OK);
    app_settings_init();assert(app_settings_simple_utility(2)==simple_actions[0]&&app_settings_simple_utility(3)==simple_actions[1]);
    commit_fails=true;const uint8_t empty_actions[2]={0,0};
    assert(app_settings_set_simple_utilities(empty_actions)!=ESP_OK&&app_settings_simple_utility(2)==simple_actions[0]);commit_fails=false;
    const uint8_t bad_actions[2]={APP_HOME_UTIL_COUNT,0};
    assert(app_settings_set_simple_utilities(bad_actions)==ESP_ERR_INVALID_ARG);
    backup_identity_t identity={0};memcpy(identity.magic,"S31BIND1",8);assert(backup_device_mac(identity.mac)==ESP_OK);
    identity.reserved[1]=simple_actions[0]|(simple_actions[1]<<4);
    uint32_t seal=backup_identity_checksum(&identity);for(unsigned i=0;i<4;++i)identity.checksum[i]=seal>>(i*8);
    assert(backup_identity_valid(&identity,29)&&!backup_identity_valid(&identity,28));
    identity.reserved[1]=255;seal=backup_identity_checksum(&identity);
    for(unsigned i=0;i<4;++i)identity.checksum[i]=seal>>(i*8);
    assert(!backup_identity_valid(&identity,29));
    assert(app_settings_backup_save()==ESP_OK);
    memset(s_simple_utilities,0,sizeof(s_simple_utilities));
    assert(app_settings_backup_restore()==ESP_OK&&app_settings_simple_utility(2)==simple_actions[0]&&app_settings_simple_utility(3)==simple_actions[1]);
    for(unsigned i=0;i<4;++i)assert(app_settings_home_utility(i)==regular[i]);
    assert(app_settings_set_simple_ui(false)==ESP_OK);
    assert(app_settings_backup_restore()==ESP_OK&&!app_settings_simple_ui());
    assert(app_settings_set_simple_ui(true)==ESP_OK);
    bool regular_labels=app_settings_home_utility_labels(),regular_border=app_settings_home_utility_border();
    bool auto_policy=app_settings_autobackup_enabled();
    for(unsigned flags=0;flags<4;++flags){
        bool labels=(flags&1)!=0,border=(flags&2)!=0;
        assert(app_settings_set_simple_utilities_config(simple_actions,labels,border)==ESP_OK);
        app_settings_init();assert(app_settings_simple_utility_labels()==labels&&app_settings_simple_utility_border()==border);
        commit_fails=true;assert(app_settings_set_simple_utilities_config(simple_actions,!labels,!border)!=ESP_OK);commit_fails=false;
        assert(app_settings_simple_utility_labels()==labels&&app_settings_simple_utility_border()==border);
        assert(app_settings_backup_save()==ESP_OK);s_simple_labels=!labels;s_simple_border=!border;
        assert(app_settings_backup_restore()==ESP_OK&&app_settings_simple_utility_labels()==labels&&app_settings_simple_utility_border()==border);
        assert(app_settings_home_utility_labels()==regular_labels&&app_settings_home_utility_border()==regular_border);
        assert(app_settings_autobackup_enabled()==auto_policy);
    }
    assert(app_settings_set_simple_utilities_config(empty_actions,true,true)==ESP_OK);
    assert(app_settings_backup_save()==ESP_OK);
    FILE *file=fopen(BACKUP_FILE,"r+b");assert(file);
    settings_backup_v1_t base;uint8_t extension[7];settings_backup_profile_t profile;
    assert(fread(&base,1,sizeof(base),file)==sizeof(base)&&backup_version(&base)==32);
    assert(fread(extension,1,7,file)==7);
    assert(fread(&profile,1,sizeof(profile),file)==sizeof(profile)&&(profile.home_full_refresh&64));
    /* V27's retired bit must not opt into simple UI. Keep its original checked byte layout. */
    memcpy(base.magic,"PICOSETR",8);backup_seal(&base);
    uint32_t ext_hash=backup_shutdown_checksum(&base,extension[0],extension[1],extension[2]);
    for(unsigned i=0;i<4;++i)extension[i+3]=ext_hash>>(i*8);
    uint32_t hash=backup_profile_checksum(&base,extension[0],extension[1],extension[2],&profile);
    for(unsigned i=0;i<4;++i)profile.checksum[i]=hash>>(i*8);
    rewind(file);assert(fwrite(&base,1,sizeof(base),file)==sizeof(base));
    assert(fwrite(extension,1,7,file)==7);
    assert(fwrite(&profile,1,sizeof(profile),file)==sizeof(profile));
    long accounts_offset=sizeof(base)+sizeof(extension)+sizeof(profile)+sizeof(settings_backup_keys_t)+
        sizeof(settings_backup_hold_t)+sizeof(settings_backup_indent_adjust_t)+sizeof(settings_backup_weight_t)+
        sizeof(settings_backup_wifi_t)+sizeof(settings_backup_background_t)+sizeof(settings_backup_home_t)+
        sizeof(settings_backup_background_t)+sizeof(settings_backup_indent_adjust_t)+sizeof(reader_backup_t)+
        sizeof(settings_backup_utilities_t)+sizeof(backup_identity_t);
    assert(!fseek(file,accounts_offset+sizeof(settings_backup_accounts_t)+sizeof(settings_backup_preferences_t),SEEK_SET));
    uint8_t history[4096];size_t bytes=fread(history,1,sizeof(history),file);assert(feof(file));
    assert(!fseek(file,accounts_offset,SEEK_SET)&&fwrite(history,1,bytes,file)==bytes);
    assert(!fflush(file)&&!ftruncate(fileno(file),accounts_offset+bytes));assert(!fclose(file));
    assert(app_settings_backup_restore()==ESP_OK&&app_settings_simple_ui());
    puts("PASS simple UI preference: default off, commit failure keeps live state, V32 round-trip, four label/border combinations and independent custom shortcuts, legacy restore and transient/cold-read failures preserve local choice");
}
