/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
 * 中文：显式白名单恢复系统设置；不擦NVS分区，不触及VCOM、OTA元数据或TF文件。
 * English: Reset an explicit namespace allowlist, never whole NVS, VCOM, OTA metadata or TF files.
 */
#include "factory_reset.h"
#include "kiiko_board_config.h"
#include "nvs.h"

esp_err_t app_factory_reset(void) {
    const char *names[] = {
        KIIKO_BOARD_SETTINGS_NAMESPACE, KIIKO_BOARD_STARTUP_NAMESPACE,
#if KIIKO_BOARD_METALIO47
        "metalio_wifi", "metalio_light", "metalio_haptic",
#else
        "rp_wifi",
#endif
        "rp_blestate", "rp_blebind", "rp_blebond", "nimble_bond",
        // 凭据最后处理；上面的存储失败不提前移除密码。/ Process credentials last, after other namespaces succeed.
        KIIKO_BOARD_LOCK_NAMESPACE
    };
    for (unsigned i=0; i<sizeof(names)/sizeof(names[0]); ++i) {
        nvs_handle_t h;
        esp_err_t error=nvs_open(names[i],NVS_READONLY,&h);
        if(error==ESP_ERR_NVS_NOT_FOUND)continue;
        if(error!=ESP_OK)return error;
        nvs_close(h);
        error=nvs_open(names[i],NVS_READWRITE,&h);
        if(error!=ESP_OK)return error;
        error=nvs_erase_all(h);
        if(error==ESP_OK)error=nvs_commit(h);
        nvs_close(h);
        if(error!=ESP_OK)return error;
    }
    return ESP_OK;
}
