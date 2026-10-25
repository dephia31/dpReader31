/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
 * 中文：固定开源字体目录及可取消的TF下载，不修改任何已选字体。
 * English: Curated open fonts and cancellable TF downloads, without changing selected faces.
 * 冻结：只接受内置HTTPS来源和完整SHA256；保留原版权与许可证，不写NVS或应用分区。
 * Frozen: compiled HTTPS sources and full SHA256 only; preserve notices, never write NVS or app slots.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#define ONLINE_FONT_COUNT 5
#define ONLINE_FONT_DIRECTORY "/sdcard/assets/fonts"

typedef struct {
    const char *name, *ascii_name, *filename, *url, *license;
    uint32_t size, source_size, deflate_offset, deflate_size;
    uint8_t sha256[32], source_sha256[32];
} online_font_item_t;

typedef enum {
    ONLINE_FONT_IDLE, ONLINE_FONT_CONNECTING, ONLINE_FONT_DOWNLOADING,
    ONLINE_FONT_VERIFYING, ONLINE_FONT_DONE, ONLINE_FONT_FAILED, ONLINE_FONT_CANCELLED,
} online_font_phase_t;

typedef struct {
    online_font_phase_t phase;
    unsigned item;
    uint32_t received, total, revision;
    bool busy;
    char message[96];
} online_font_status_t;

/// 静态目录，无网络或文件I/O。/ Static catalog, without networking or file IO.
const online_font_item_t *online_font_item(unsigned index);
/// 本地已安装文件快照；只在UI事件或tick调用。/ Installed-file snapshot; UI events/ticks only.
bool online_font_installed(unsigned index);
/// 唯一下载任务；所有权属于启动该任务的页面。/ One worker, owned by the initiating page.
esp_err_t online_font_start(unsigned index);
void online_font_status(online_font_status_t *out);
/// 取消并等待释放网络/卡句柄；离页、锁屏和拔卡前调用。/ Cancel and join network/card handles on exit, lock or removal.
void online_font_cancel_join(void);
