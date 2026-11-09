/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../main/boot_state.c"
esp_err_t nvs_open(const char *ns,int mode,nvs_handle_t *h){(void)ns;assert(mode==NVS_READONLY);*h=1;return ESP_OK;}
void nvs_close(nvs_handle_t h){(void)h;}
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *data,size_t *size){(void)h;(void)key;(void)data;(void)size;return ESP_ERR_NVS_NOT_FOUND;}
int main(int argc,char **argv) {
    (void)argv;
    if(argc>1) {
        pico_boot_init(false);
        assert(pico_boot_has_resume()&&pico_boot_lock_pending());
        pico_resume_t resume;
        assert(pico_boot_take_resume(&resume)&&!strcmp(resume.path,"/sdcard/books/a.epub"));
        assert(!pico_boot_take_resume(&resume));
        assert(pico_boot_reset_files()==ESP_OK);
        pico_boot_init(false);assert(!pico_boot_has_resume()&&!pico_boot_recovery());
        puts("PASS reboot resume, lock gate, consume once, factory reset");return 0;
    }
    assert(mkdir(BOOK_STATE_SD_ROOT,0775)==0);assert(mkdir(BOOK_STATE_FLASH_ROOT,0775)==0);
    pico_boot_init(false);assert(!pico_boot_recovery());pico_boot_ready();
    assert(pico_boot_book_begin("/sdcard/books/a.epub")==ESP_OK);
    pico_boot_init(false);assert(pico_boot_recovery());
    char interrupted[PICO_BOOT_PATH_MAX];assert(pico_boot_interrupted_book(interrupted,sizeof(interrupted)));
    assert(!strcmp(interrupted,"/sdcard/books/a.epub"));
    pico_boot_ready();assert(pico_boot_book_begin(interrupted)==ESP_OK);pico_boot_book_end(true);
    pico_boot_init(false);assert(!pico_boot_recovery());pico_boot_ready();
    assert(rename(BOOK_STATE_SD_ROOT,"removed-card")==0);
    assert(pico_boot_book_begin(interrupted)!=ESP_OK);
    assert(rename("removed-card",BOOK_STATE_SD_ROOT)==0);pico_boot_book_end(false);
    pico_resume_t resume={.tab=1,.reader=1};strcpy(resume.path,interrupted);
    assert(pico_boot_save_resume(&resume)==ESP_OK);
    puts("PASS checkpoint independent of NVS writes, interrupted-open protection and lost-media errors");return 0;
}
