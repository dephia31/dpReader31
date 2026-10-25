/* 中文：编译实际 OTA 出口，拒绝 Pico 和其他 S31 镜像，失败不切槽。
 * English: Exercise production OTA; reject Pico/other S31 images and retain the boot slot on failure. */
#include "ota_update.h"
#include "ota_online.h"
#include "esp_app_desc.h"
#include "esp_app_format.h"
#include "esp_ota_ops.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const char *path="build/metalio-tests/candidate.bin";
static esp_app_desc_t current={.version="0.3.3-rc90-metalio-test1",.project_name="Kiikoread_Metalio_S31"};
static esp_partition_t inactive={.address=0x510000,.size=0x500000,.label="ota_1"};
static bool missing,fail_write,fail_end;
static unsigned begins,aborts,commits;

static size_t written;
const esp_app_desc_t *esp_app_get_description(void) {return &current;}
const esp_partition_t *esp_ota_get_running_partition(void) {return NULL;}
const esp_partition_t *esp_ota_get_next_update_partition(void *x) {(void)x;return missing?NULL:&inactive;}
esp_err_t esp_ota_begin(const esp_partition_t *p,size_t size,esp_ota_handle_t *h) {
    assert(p==&inactive && size>0); ++begins; written=0; *h=1; return ESP_OK;
}
esp_err_t esp_ota_write(esp_ota_handle_t h,const void *p,size_t n) {(void)p;assert(h==1);written+=n;return fail_write?ESP_FAIL:ESP_OK;}
esp_err_t esp_ota_abort(esp_ota_handle_t h) {assert(h==1);++aborts;return ESP_OK;}
esp_err_t esp_ota_end(esp_ota_handle_t h) {assert(h==1);return fail_end?ESP_FAIL:ESP_OK;}
esp_err_t esp_ota_set_boot_partition(const esp_partition_t *p) {assert(p==&inactive);++commits;return ESP_OK;}
esp_err_t esp_ota_get_state_partition(const esp_partition_t *p,esp_ota_img_states_t *s) {(void)p;(void)s;return ESP_FAIL;}
esp_err_t esp_ota_mark_app_valid_cancel_rollback(void) {return ESP_OK;}
static void fixture(esp_chip_id_t chip,const char *project,const char *version) {
    FILE *f=fopen(path,"wb");assert(f);
    esp_image_header_t header={.magic=ESP_IMAGE_HEADER_MAGIC,.chip_id=chip};
    esp_image_segment_header_t segment={.data_len=4096};
    esp_app_desc_t desc={.magic_word=ESP_APP_DESC_MAGIC_WORD};
    snprintf(desc.project_name,sizeof(desc.project_name),"%s",project);
    snprintf(desc.version,sizeof(desc.version),"%s",version);
    assert(fwrite(&header,sizeof(header),1,f)==1&&fwrite(&segment,sizeof(segment),1,f)==1&&fwrite(&desc,sizeof(desc),1,f)==1);
    uint8_t padding[4096]={0}; assert(fwrite(padding,sizeof(padding),1,f)==1); assert(!fclose(f));
}
int main(void) {
    pico_ota_info_t info; char message[96];
    fixture(ESP_CHIP_ID_ESP32S3,"Read_Pico","0.3.3-rc90");
    assert(pico_ota_inspect(path,&info)==ESP_ERR_INVALID_RESPONSE&&!info.ready);
    fixture(ESP_CHIP_ID_ESP32S31,"Metalio-E-Ink4-Plus","2.0.76");
    assert(pico_ota_inspect(path,&info)==ESP_ERR_INVALID_RESPONSE&&!info.ready);
    fixture(ESP_CHIP_ID_ESP32S31,"Read_Pico","0.3.3-rc91");
    assert(pico_ota_inspect(path,&info)==ESP_ERR_INVALID_RESPONSE&&!info.ready);
    fixture(ESP_CHIP_ID_ESP32S31,current.project_name,current.version);
    assert(pico_ota_inspect(path,&info)==ESP_ERR_INVALID_STATE&&!info.ready);
    fixture(ESP_CHIP_ID_ESP32S31,current.project_name,"0.3.3-rc90-metalio-test2");
    assert(pico_ota_inspect(path,&info)==ESP_OK&&info.ready);
    size_t image_size=info.image_size;
    missing=true; assert(pico_ota_inspect(path,&info)==ESP_ERR_NOT_SUPPORTED); missing=false;
    inactive.size=100; assert(pico_ota_inspect(path,&info)==ESP_ERR_INVALID_SIZE); inactive.size=0x500000;
    fail_write=true; assert(pico_ota_install(path,message,sizeof(message))!=ESP_OK&&aborts==1&&commits==0); fail_write=false;
    fail_end=true; assert(pico_ota_install(path,message,sizeof(message))!=ESP_OK&&commits==0); fail_end=false;
    assert(pico_ota_install(path,message,sizeof(message))==ESP_OK&&written==image_size&&commits==1);
    FILE *f=fopen(path,"wb");assert(f);fputc(ESP_IMAGE_HEADER_MAGIC,f);fclose(f);
    unsigned before=begins; assert(pico_ota_install(path,message,sizeof(message))==ESP_ERR_INVALID_SIZE&&begins==before);
    remove(path);
    puts("Metalio OTA: rejects Pico S3, foreign S31, same version, truncation, oversize; inactive slot only; write/validation failures never select a new boot slot; local TF update");
}
