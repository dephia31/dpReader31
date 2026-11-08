/* SPDX-License-Identifier: Apache-2.0 */
#define main existing_settings_backup_fixture_main
#include "settings_backup_host_test.c"
#undef main
int main(void) {
    (void)mkdir("build",0700);(void)mkdir("build/book-tests",0700);(void)mkdir(APP_SETTINGS_BACKUP_ROOT,0700);
    app_settings_init();assert(app_settings_home_utility_labels()&&app_settings_home_utility_border());for(unsigned i=0;i<4;++i)assert(!app_settings_home_utility(i));
    const uint8_t chosen[]={APP_HOME_UTIL_RESUME,APP_HOME_UTIL_OPDS,APP_HOME_UTIL_WIFI,APP_HOME_UTIL_OTA};
    assert(app_settings_set_home_utilities(NULL)==ESP_ERR_INVALID_ARG);
    uint8_t bad[]={APP_HOME_UTIL_COUNT,0,0,0};int before=commit_count;
    assert(app_settings_set_home_utilities(bad)==ESP_ERR_INVALID_ARG&&commit_count==before);
    assert(app_settings_set_home_utilities(chosen)==ESP_OK&&commit_count==before+1);
    assert(app_settings_set_home_utilities_config(chosen,false,false)==ESP_OK);
    s_home_utility_border=true;s_home_utility_labels=true;memset(s_home_utilities,0,4);app_settings_init();assert(!memcmp(s_home_utilities,chosen,4)&&!app_settings_home_utility_labels()&&!app_settings_home_utility_border());
    commit_fails=true;const uint8_t empty[4]={0};assert(app_settings_set_home_utilities_config(empty,true,true)!=ESP_OK&&!memcmp(s_home_utilities,chosen,4)&&!app_settings_home_utility_labels()&&!app_settings_home_utility_border());commit_fails=false;
    assert(app_settings_set_home_utilities(chosen)==ESP_OK);assert(app_settings_backup_save()==ESP_OK);
    assert(app_settings_set_home_utilities_config(empty,true,true)==ESP_OK);before=commit_count;
    assert(app_settings_backup_restore()==ESP_OK&&commit_count==before+1&&!memcmp(s_home_utilities,chosen,4)&&!app_settings_home_utility_labels()&&!app_settings_home_utility_border());
    memset(s_home_utilities,0,4);app_settings_init();assert(!memcmp(s_home_utilities,chosen,4));
    FILE *f=fopen(BACKUP_FILE,"r+b");assert(f);assert(!fseek(f,-16,SEEK_END));int byte=fgetc(f);assert(byte!=EOF);assert(!fseek(f,-1,SEEK_CUR));fputc(byte^1,f);fclose(f);
    before=commit_count;assert(app_settings_backup_restore()==ESP_ERR_INVALID_RESPONSE&&commit_count==before);
    assert(!memcmp(s_home_utilities,chosen,4));
    assert(app_settings_backup_save()==ESP_OK);
    f=fopen(BACKUP_FILE,"rb");assert(f);unsigned char bytes[4096];size_t len=fread(bytes,1,sizeof(bytes),f);assert(feof(f));fclose(f);
    /* Strip newer blocks before constructing historical V23/V24 tails. */
    size_t removed=sizeof(backup_identity_t)+sizeof(settings_backup_accounts_t)+sizeof(settings_backup_preferences_t);
    memmove(bytes+len-removed-8,bytes+len-8,8);len-=removed;
    /* A v23 backup has the same block size but no label flag. */
    settings_backup_utilities_t legacy;memcpy(&legacy,bytes+len-16,sizeof(legacy));legacy.actions[0]&=0x3f;
    uint32_t legacy_hash=backup_utilities_checksum(&legacy);for(unsigned i=0;i<4;++i)legacy.checksum[i]=legacy_hash>>(8*i);
    memcpy(bytes+len-16,&legacy,sizeof(legacy));
    settings_backup_v1_t oldbase;memcpy(&oldbase,bytes,sizeof(oldbase));oldbase.magic[7]='N';backup_seal(&oldbase);memcpy(bytes,&oldbase,sizeof(oldbase));
    uint8_t *oldext=bytes+sizeof(oldbase);uint32_t oldhash=backup_shutdown_checksum(&oldbase,oldext[0],oldext[1],oldext[2]);
    for(unsigned i=0;i<4;++i)oldext[3+i]=oldhash>>(8*i);
    settings_backup_profile_t oldprofile;memcpy(&oldprofile,oldext+7,sizeof(oldprofile));oldhash=backup_profile_checksum(&oldbase,oldext[0],oldext[1],oldext[2],&oldprofile);
    for(unsigned i=0;i<4;++i){oldprofile.checksum[i]=oldhash>>(8*i);}
    memcpy(oldext+7,&oldprofile,sizeof(oldprofile));
    f=fopen(BACKUP_FILE,"wb");assert(f);assert(fwrite(bytes,1,len,f)==len);fclose(f);
    assert(app_settings_set_home_utilities_config(empty,true,true)==ESP_OK);assert(app_settings_backup_restore()==ESP_OK&&app_settings_home_utility_labels()&&app_settings_home_utility_border()&&!memcmp(s_home_utilities,chosen,4));
    /* v24 restores labels while preserving the newer border preference. */
    oldbase.magic[7]='O';backup_seal(&oldbase);memcpy(bytes,&oldbase,sizeof(oldbase));
    oldhash=backup_shutdown_checksum(&oldbase,oldext[0],oldext[1],oldext[2]);for(unsigned i=0;i<4;++i)oldext[3+i]=oldhash>>(8*i);
    oldhash=backup_profile_checksum(&oldbase,oldext[0],oldext[1],oldext[2],&oldprofile);for(unsigned i=0;i<4;++i){oldprofile.checksum[i]=oldhash>>(8*i);}
    memcpy(oldext+7,&oldprofile,sizeof(oldprofile));
    legacy.actions[0]|=0x80;legacy_hash=backup_utilities_checksum(&legacy);for(unsigned i=0;i<4;++i){legacy.checksum[i]=legacy_hash>>(8*i);}
    memcpy(bytes+len-16,&legacy,sizeof(legacy));
    f=fopen(BACKUP_FILE,"wb");assert(f);assert(fwrite(bytes,1,len,f)==len);fclose(f);
    assert(app_settings_set_home_utilities_config(empty,true,false)==ESP_OK);assert(app_settings_backup_restore()==ESP_OK&&!app_settings_home_utility_labels()&&!app_settings_home_utility_border()&&!memcmp(s_home_utilities,chosen,4));
    settings_backup_v1_t base;memcpy(&base,bytes,sizeof(base));base.magic[7]='M';backup_seal(&base);memcpy(bytes,&base,sizeof(base));
    uint8_t *ext=bytes+sizeof(base);uint32_t hash=backup_shutdown_checksum(&base,ext[0],ext[1],ext[2]);
    for(unsigned i=0;i<4;++i)ext[3+i]=hash>>(8*i);
    settings_backup_profile_t profile;memcpy(&profile,ext+7,sizeof(profile));hash=backup_profile_checksum(&base,ext[0],ext[1],ext[2],&profile);
    for(unsigned i=0;i<4;++i){profile.checksum[i]=hash>>(8*i);}
    memcpy(ext+7,&profile,sizeof(profile));
    memmove(bytes+len-16,bytes+len-8,8);len-=8;
    f=fopen(BACKUP_FILE,"wb");assert(f);assert(fwrite(bytes,1,len,f)==len);fclose(f);
    const uint8_t newer[]={APP_HOME_UTIL_FILES,APP_HOME_UTIL_KOSYNC,APP_HOME_UTIL_BLUETOOTH,APP_HOME_UTIL_SETTINGS};
    assert(app_settings_set_home_utilities_config(newer,true,app_settings_home_utility_border())==ESP_OK);assert(app_settings_backup_restore()==ESP_OK&&!memcmp(s_home_utilities,newer,4)&&app_settings_home_utility_labels());
    puts("PASS Home utilities/labels/borders: defaults, bounds, commit failure, reboot persistence, unified backup/checksum and legacy preservation");
}
