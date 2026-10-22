/* SPDX-License-Identifier: Apache-2.0
 * 中文：Metalio 独立本地测试版关闭联网升级。
 * English: Independent Metalio local test build has no online update feed.
 */
#include "ota_online.h"
#include <string.h>
#include "esp_private/cache_utils.h"
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
// 供屏前后使用SDK同一个递归Flash操作锁；缓存停顿不能穿插物理相位。/ Share the SDK recursive Flash-operation lock across output; cache stalls must not interrupt physical phases.
bool pico_online_display_begin(void) { spi_flash_op_lock();return true; }
void pico_online_display_end(bool held) { if(held)spi_flash_op_unlock(); }
esp_err_t pico_online_commit(void) { return ESP_ERR_NOT_SUPPORTED; }
