/* SPDX-License-Identifier: Apache-2.0
 * 中文：Metalio 独立本地测试版关闭联网升级。
 * English: Independent Metalio local test build has no online update feed.
 */
#include "ota_online.h"
#include <string.h>
static pico_update_status_t status={.state=PICO_UPDATE_IDLE};
static esp_err_t unavailable(void) {
    status.state=PICO_UPDATE_FAILED; status.revision++;
    strcpy(status.message,"Metalio 本地测试版，请使用专用 USB 或 TF 固件");
    return ESP_ERR_NOT_SUPPORTED;
}
esp_err_t pico_online_check(void) { return unavailable(); }
esp_err_t pico_online_download(void) { return unavailable(); }
bool pico_online_busy(void) { return false; }
void pico_online_get_status(pico_update_status_t *out) { if(out)*out=status; }
void pico_online_cancel_join(void) {}
bool pico_online_display_begin(void) { return false; }
void pico_online_display_end(bool held) { (void)held; }
esp_err_t pico_online_commit(void) { return ESP_ERR_NOT_SUPPORTED; }
