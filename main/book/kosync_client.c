/*
 * SPDX-FileCopyrightText: 2026 dpReader
 * SPDX-License-Identifier: Apache-2.0
 *
 * Triển khai KOSyncClient (KOReader Sync) cho dpReader trên nền ESP32-S31.
 */
#include "kosync_client.h"
#include "settings.h"
#include "read_pico_transfer.h"
#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char *TAG = "kosync";

/* ---- Thuật toán MD5 chuẩn RFC 1321 gọn nhẹ, không phụ thuộc cấu hình mbedTLS ---- */
typedef struct {
    uint32_t state[4];
    uint32_t count[2];
    uint8_t buffer[64];
} md5_ctx_t;

static void md5_transform(uint32_t state[4], const uint8_t block[64]) {
    static const uint32_t s[64] = {
        7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
        5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
        4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
        6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
    };
    static const uint32_t k[64] = {
        0xd76aa478u, 0xe8c7b756u, 0x242070dbu, 0xc1bdceeeu,
        0xf57c0fafu, 0x4787c62au, 0xa8304613u, 0xfd469501u,
        0x698098d8u, 0x8b44f7afu, 0xffff5bb1u, 0x895cd7beu,
        0x6b901122u, 0xfd987193u, 0xa679438eu, 0x49b40821u,
        0xf61e2562u, 0xc040b340u, 0x265e5a51u, 0xe9b6c7aau,
        0xd62f105du, 0x02441453u, 0xd8a1e681u, 0xe7d3fbc8u,
        0x21e1cde6u, 0xc33707d6u, 0xf4d50d87u, 0x455a14edu,
        0xa9e3e905u, 0xfcefa3f8u, 0x676f02d9u, 0x8d2a4c8au,
        0xfffa3942u, 0x8771f681u, 0x6d9d6122u, 0xfde5380cu,
        0xa4beea44u, 0x4bdecfa9u, 0xf6bb4b60u, 0xbebfbc70u,
        0x289b7ec6u, 0xeaa127fau, 0xd4ef3085u, 0x04881d05u,
        0xd9d4d039u, 0xe6db99e5u, 0x1fa27cf8u, 0xc4ac5665u,
        0xf4292244u, 0x432aff97u, 0xab9423a7u, 0xfc93a039u,
        0x655b59c3u, 0x8f0ccc92u, 0xffeff47du, 0x85845dd1u,
        0x6fa87e4fu, 0xfe2ce6e0u, 0xa3014314u, 0x4e0811a1u,
        0xf7537e82u, 0xbd3af235u, 0x2ad7d2bbu, 0xeb86d391u
    };
    uint32_t m[16];
    for (int i = 0; i < 16; ++i) {
        m[i] = (uint32_t)block[i * 4] |
               ((uint32_t)block[i * 4 + 1] << 8) |
               ((uint32_t)block[i * 4 + 2] << 16) |
               ((uint32_t)block[i * 4 + 3] << 24);
    }
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    for (uint32_t i = 0; i < 64; ++i) {
        uint32_t f, g;
        if (i < 16) { f = (b & c) | ((~b) & d); g = i; }
        else if (i < 32) { f = (d & b) | ((~d) & c); g = (5 * i + 1) & 15; }
        else if (i < 48) { f = b ^ c ^ d; g = (3 * i + 5) & 15; }
        else { f = c ^ (b | (~d)); g = (7 * i) & 15; }
        uint32_t temp = d;
        d = c;
        c = b;
        uint32_t x = a + f + k[i] + m[g];
        b = b + ((x << s[i]) | (x >> (32 - s[i])));
        a = temp;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

static void md5_init(md5_ctx_t *ctx) {
    ctx->count[0] = ctx->count[1] = 0;
    ctx->state[0] = 0x67452301u;
    ctx->state[1] = 0xefcdab89u;
    ctx->state[2] = 0x98badcfeu;
    ctx->state[3] = 0x10325476u;
}

static void md5_update(md5_ctx_t *ctx, const uint8_t *input, size_t len) {
    size_t idx = (ctx->count[0] >> 3) & 0x3F;
    if ((ctx->count[0] += (uint32_t)(len << 3)) < (uint32_t)(len << 3)) ctx->count[1]++;
    ctx->count[1] += (uint32_t)(len >> 29);
    size_t part_len = 64 - idx;
    size_t i = 0;
    if (len >= part_len) {
        memcpy(&ctx->buffer[idx], input, part_len);
        md5_transform(ctx->state, ctx->buffer);
        for (i = part_len; i + 63 < len; i += 64) md5_transform(ctx->state, &input[i]);
        idx = 0;
    }
    memcpy(&ctx->buffer[idx], &input[i], len - i);
}

static void md5_final(md5_ctx_t *ctx, char out_hex[33]) {
    static const uint8_t padding[64] = { 0x80 };
    uint8_t bits[8];
    for (int i = 0; i < 4; ++i) {
        bits[i] = (uint8_t)((ctx->count[0] >> (i * 8)) & 0xFF);
        bits[i + 4] = (uint8_t)((ctx->count[1] >> (i * 8)) & 0xFF);
    }
    size_t idx = (ctx->count[0] >> 3) & 0x3F;
    size_t pad_len = (idx < 56) ? (56 - idx) : (120 - idx);
    md5_update(ctx, padding, pad_len);
    md5_update(ctx, bits, 8);
    uint8_t digest[16];
    for (int i = 0; i < 4; ++i) {
        digest[i * 4]     = (uint8_t)(ctx->state[i] & 0xFF);
        digest[i * 4 + 1] = (uint8_t)((ctx->state[i] >> 8) & 0xFF);
        digest[i * 4 + 2] = (uint8_t)((ctx->state[i] >> 16) & 0xFF);
        digest[i * 4 + 3] = (uint8_t)((ctx->state[i] >> 24) & 0xFF);
    }
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < 16; ++i) {
        out_hex[i * 2]     = hex[digest[i] >> 4];
        out_hex[i * 2 + 1] = hex[digest[i] & 0x0F];
    }
    out_hex[32] = 0;
}

void kosync_md5_string(const char *input, char out_hex[33]) {
    if (!out_hex) return;
    if (!input) input = "";
    md5_ctx_t ctx;
    md5_init(&ctx);
    md5_update(&ctx, (const uint8_t *)input, strlen(input));
    md5_final(&ctx, out_hex);
}

esp_err_t kosync_compute_doc_md5(const char *file_path, char out_hex[33]) {
    if (!file_path || !file_path[0] || !out_hex) return ESP_ERR_INVALID_ARG;
    out_hex[0] = 0;
    struct stat st;
    if (stat(file_path, &st) != 0 || st.st_size <= 0) return ESP_ERR_NOT_FOUND;
    long file_size = (long)st.st_size;

    FILE *f = fopen(file_path, "rb");
    if (!f) return ESP_ERR_NOT_FOUND;

    uint8_t *buf = heap_caps_malloc(1024, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) buf = malloc(1024);
    if (!buf) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    md5_ctx_t ctx;
    md5_init(&ctx);
    /* KOReader frontend/util.lua: util.partialMD5
     * i = -1..10 -> offset = 0, 1024, 4096, 16384, ..., 1073741824
     */
    for (int i = -1; i <= 10; ++i) {
        long offset = (i < 0) ? 0L : (1024L << (2 * i));
        if (offset >= file_size) break;
        if (fseek(f, offset, SEEK_SET) != 0) break;
        size_t to_read = (file_size - offset < 1024) ? (size_t)(file_size - offset) : 1024;
        size_t n = fread(buf, 1, to_read, f);
        if (n > 0) md5_update(&ctx, buf, n);
        else break;
    }
    free(buf);
    fclose(f);
    md5_final(&ctx, out_hex);
    return ESP_OK;
}

/* ---- Quản lý kết nối mạng WiFi tạm thời hoặc tái sử dụng STA đang bật ---- */
static esp_err_t kosync_acquire_network(bool *started_by_us, char *msg, size_t msg_cap) {
    *started_by_us = false;
    read_pico_transfer_status_t st = {0};
    read_pico_transfer_get_status(&st);
    if (st.mode == READ_PICO_TRANSFER_MODE_STA && st.network_ready) return ESP_OK;
    char ssid[33] = {0};
    bool configured = false;
    if (read_pico_transfer_get_saved_wifi(ssid, &configured) != ESP_OK || !configured) {
        if (msg && msg_cap) snprintf(msg, msg_cap, "Chưa cấu hình mạng WiFi");
        return ESP_ERR_NOT_FOUND;
    }
    // 阅读中同步不该要求用户先离开热点/空闲会话：直接抢占射频并连回已保存网络。
    // 只有上传在写卡时不能抢断，此时才提示用户。
    // A sync while reading must not force the user out of a hotspot or idle session: take the
    // radio and reconnect to the saved network. Only an in-flight upload (file mid-write) is
    // exempt, and only then does the user see a prompt.
    esp_err_t err = read_pico_transfer_preempt_for_network();
    if (err == ESP_ERR_INVALID_STATE) {
        read_pico_transfer_status_t busy = {0};
        read_pico_transfer_get_status(&busy);
        if (msg && msg_cap) snprintf(msg, msg_cap, busy.state == READ_PICO_TRANSFER_UPLOADING
            ? "Đang nhận tệp, hãy đợi truyền xong rồi đồng bộ"
            : "WiFi đang được trang truyền tệp giữ, hãy thoát trang đó rồi thử lại");
        return err;
    }
    if (err != ESP_OK) {
        if (msg && msg_cap) snprintf(msg, msg_cap, "Không thể bật WiFi (%s)", esp_err_to_name(err));
        return err;
    }
    *started_by_us = true;
    int64_t deadline = esp_timer_get_time() + 12000000LL;
    while (esp_timer_get_time() < deadline) {
        read_pico_transfer_service_poll();
        read_pico_transfer_get_status(&st);
        if (st.network_ready) {
            uint32_t utc = 0;
            (void)read_pico_transfer_sync_time_online(&utc);
            return ESP_OK;
        }
        if (st.state == READ_PICO_TRANSFER_ERROR) break;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    read_pico_transfer_stop();
    *started_by_us = false;
    if (msg && msg_cap) snprintf(msg, msg_cap, "Không thể kết nối WiFi đã lưu");
    return ESP_ERR_TIMEOUT;
}

static void kosync_release_network(bool started_by_us) {
    if (started_by_us) read_pico_transfer_stop();
}

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} http_resp_t;

static esp_err_t kosync_http_event(esp_http_client_event_t *evt) {
    if (evt->event_id == HTTP_EVENT_ON_DATA && evt->user_data && evt->data && evt->data_len > 0) {
        http_resp_t *r = (http_resp_t *)evt->user_data;
        size_t avail = (r->cap > r->len + 1) ? (r->cap - r->len - 1) : 0;
        size_t copy = (size_t)evt->data_len < avail ? (size_t)evt->data_len : avail;
        if (copy > 0) {
            memcpy(r->buf + r->len, evt->data, copy);
            r->len += copy;
            r->buf[r->len] = 0;
        }
    }
    return ESP_OK;
}

static esp_err_t kosync_request(esp_http_client_method_t method, const char *path,
                                const char *body, int *out_status,
                                char *resp_buf, size_t resp_cap,
                                char *msg, size_t msg_cap) {
    const char *base = app_settings_kosync_url();
    const char *user = app_settings_kosync_user();
    const char *key  = app_settings_kosync_key();
    if (!base || !base[0]) base = DPREADER_DEFAULT_KOSYNC_URL;
    if (!user || !user[0] || !key || !key[0]) {
        if (msg && msg_cap) snprintf(msg, msg_cap, "Chưa nhập tài khoản KOSync");
        return ESP_ERR_INVALID_STATE;
    }

    char url[256];
    size_t blen = strlen(base);
    while (blen > 0 && base[blen - 1] == '/') --blen;
    snprintf(url, sizeof(url), "%.*s%s", (int)blen, base, path);

    bool started_wifi = false;
    esp_err_t err = kosync_acquire_network(&started_wifi, msg, msg_cap);
    if (err != ESP_OK) return err;

    if (resp_buf && resp_cap) resp_buf[0] = 0;
    http_resp_t resp = { .buf = resp_buf, .len = 0, .cap = resp_cap };
    esp_http_client_config_t cfg = {
        .url = url,
        .method = method,
        .timeout_ms = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = kosync_http_event,
        .user_data = resp_buf ? &resp : NULL,
        .buffer_size = 2048,
        .buffer_size_tx = 1024,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        kosync_release_network(started_wifi);
        return ESP_ERR_NO_MEM;
    }
    esp_http_client_set_header(client, "Accept", "application/vnd.koreader.v1+json");
    esp_http_client_set_header(client, "Content-Type", "application/vnd.koreader.v1+json");
    esp_http_client_set_header(client, "User-Agent", "dpReader/1.0 (Metalio47; ESP32-S31)");
    esp_http_client_set_header(client, "x-auth-user", user);
    esp_http_client_set_header(client, "x-auth-key", key);
    if (body && body[0]) {
        esp_http_client_set_post_field(client, body, (int)strlen(body));
    }

    err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    if (out_status) *out_status = status;
    esp_http_client_cleanup(client);
    kosync_release_network(started_wifi);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "HTTP %s failed: %s", url, esp_err_to_name(err));
        if (msg && msg_cap) snprintf(msg, msg_cap, "Lỗi kết nối máy chủ KOSync");
        return err;
    }
    return ESP_OK;
}

esp_err_t kosync_verify_account(bool register_new, char *message, size_t message_cap) {
    char resp[512] = {0};
    int status = 0;
    if (register_new) {
        cJSON *root = cJSON_CreateObject();
        if (!root) return ESP_ERR_NO_MEM;
        cJSON_AddStringToObject(root, "username", app_settings_kosync_user());
        cJSON_AddStringToObject(root, "password", app_settings_kosync_key());
        char *body = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        if (!body) return ESP_ERR_NO_MEM;
        esp_err_t err = kosync_request(HTTP_METHOD_POST, "/users/create", body, &status,
                                       resp, sizeof(resp), message, message_cap);
        cJSON_free(body);
        if (err != ESP_OK) return err;
        if (status == 201 || status == 200) {
            if (message && message_cap) snprintf(message, message_cap, "Đăng ký tài khoản KOSync thành công");
            return ESP_OK;
        }
        if (status == 402) {
            if (message && message_cap) snprintf(message, message_cap, "Tài khoản đã tồn tại trên máy chủ");
            return ESP_ERR_INVALID_STATE;
        }
        if (message && message_cap) snprintf(message, message_cap, "Đăng ký thất bại (HTTP %d)", status);
        return ESP_FAIL;
    }

    esp_err_t err = kosync_request(HTTP_METHOD_GET, "/users/auth", NULL, &status,
                                   resp, sizeof(resp), message, message_cap);
    if (err != ESP_OK) return err;
    if (status == 200) {
        if (message && message_cap) snprintf(message, message_cap, "Xác thực KOSync thành công!");
        return ESP_OK;
    }
    if (status == 401) {
        if (message && message_cap) snprintf(message, message_cap, "Sai tài khoản hoặc mật khẩu KOSync");
        return ESP_ERR_INVALID_ARG;
    }
    if (message && message_cap) snprintf(message, message_cap, "Máy chủ trả lỗi HTTP %d", status);
    return ESP_FAIL;
}

esp_err_t kosync_push_book_progress(const char *file_path, const book_progress_t *local_prog,
                                    double exact_ratio, char *message, size_t message_cap) {
    if (!file_path || !local_prog) return ESP_ERR_INVALID_ARG;
    char doc_md5[33] = {0};
    esp_err_t err = kosync_compute_doc_md5(file_path, doc_md5);
    if (err != ESP_OK) {
        if (message && message_cap) snprintf(message, message_cap, "Không thể tính MD5 của sách");
        return err;
    }

    uint8_t mac[6] = {0};
    (void)esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char device_id[32];
    snprintf(device_id, sizeof(device_id), "dpReader-%02X%02X%02X%02X", mac[2], mac[3], mac[4], mac[5]);

    char progress_str[96];
    snprintf(progress_str, sizeof(progress_str), "/body/DocFragment[%u]/body/p[1].%lu",
             (unsigned)(local_prog->chapter + 1), (unsigned long)local_prog->byte_off);

    double pct = (exact_ratio >= 0.0 && exact_ratio <= 1.0)
                     ? exact_ratio
                     : ((double)local_prog->pct / 100.0);
    if (pct < 0.0) pct = 0.0;
    if (pct > 1.0) pct = 1.0;

    cJSON *root = cJSON_CreateObject();
    if (!root) return ESP_ERR_NO_MEM;
    cJSON_AddStringToObject(root, "document", doc_md5);
    cJSON_AddStringToObject(root, "progress", progress_str);
    cJSON_AddNumberToObject(root, "percentage", pct);
    cJSON_AddStringToObject(root, "device", app_settings_device_name());
    cJSON_AddStringToObject(root, "device_id", device_id);

    time_t now = time(NULL);
    if (now < 1700000000) now = (time_t)(1730040000 + (esp_timer_get_time() / 1000000ULL));
    cJSON_AddNumberToObject(root, "timestamp", (double)now);

    char *body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!body) return ESP_ERR_NO_MEM;

    char resp[512] = {0};
    int status = 0;
    err = kosync_request(HTTP_METHOD_PUT, "/syncs/progress", body, &status,
                         resp, sizeof(resp), message, message_cap);
    cJSON_free(body);
    if (err != ESP_OK) return err;

    if (status == 200) {
        if (message && message_cap)
            snprintf(message, message_cap, "Đã đẩy tiến độ KOSync (%.1f%%)", pct * 100.0);
        return ESP_OK;
    }
    if (status == 401) {
        if (message && message_cap) snprintf(message, message_cap, "Sai tài khoản/mật khẩu KOSync");
        return ESP_ERR_INVALID_ARG;
    }
    if (message && message_cap) snprintf(message, message_cap, "Đẩy tiến độ thất bại (HTTP %d)", status);
    return ESP_FAIL;
}

// 解析进度串里的章节号与章内偏移。两种格式偏移含义不同，不能混用：
// dpReader 自己写入 /body/DocFragment[N]/body/p[1].<章内UTF-8字节偏移>；
// KOReader/crengine 写入 /body/DocFragment[N]/body/div/p[7]/text().<元素内字符序号>。
// 后者的末尾数字是元素内字符序号，不是章内字节偏移，当作偏移用会跳到错误页，
// 因此 crengine 串只取章节号，不报告精确偏移。
// Parse the chapter and in-chapter offset from a progress pointer. The two shapes carry
// different units: dpReader writes /body/DocFragment[N]/body/p[1].<chapter byte offset>,
// while KOReader/crengine writes .../text().<character index inside one element>. The
// trailing number is not a chapter byte offset, so a crengine pointer yields the chapter
// only and never claims an exact offset.
static void kosync_parse_pointer(const char *progress, uint16_t *out_chapter,
                                 uint32_t *out_byte_off, bool *has_exact_offset) {
    if (out_chapter) *out_chapter = 0;
    if (out_byte_off) *out_byte_off = 0;
    if (has_exact_offset) *has_exact_offset = false;
    if (!progress || !progress[0]) return;

    unsigned frag = 0;
    unsigned long off = 0;
    const char *df = strstr(progress, "DocFragment[");
    if (!df || sscanf(df, "DocFragment[%u]", &frag) != 1 || frag < 1) return;
    if (out_chapter) *out_chapter = (uint16_t)(frag - 1);

    if (strstr(progress, "text()")) return;
    unsigned parsed_frag = 0;
    if (sscanf(progress, "/body/DocFragment[%u]/body/p[1].%lu", &parsed_frag, &off) != 2) return;
    if (out_byte_off) *out_byte_off = (uint32_t)off;
    if (has_exact_offset) *has_exact_offset = true;
}

// 判断远端进度是否应覆盖本地。两端都是全书比例；dpReader 本地只有 1% 粒度，
// 远端（尤其 KOReader）有四位小数，先量化到 1% 会把 42.0% 对 42.4% 判成相等，
// 于是走“本地不落后”分支，远端更新拉不下来。这里直接比较原始比例。
// 比例相同时仅当远端确实指向更靠后的位置才拉；否则由本地推送刷新时间戳。
// Decide whether the remote progress should replace the local one. Both sides are whole-book
// ratios; dpReader's local value only has 1% granularity while the remote (KOReader especially)
// carries four decimals, so quantising to 1% makes 42.0% tie against 42.4% and the pull never
// happens. Compare the raw ratios instead. On an equal ratio, pull only when the remote
// provably points further; otherwise the local push refreshes the server timestamp.
static bool kosync_remote_wins(double local_ratio, double remote_ratio, bool has_exact_offset,
                               uint16_t cur_chapter, uint32_t cur_byte_off,
                               uint16_t rem_chapter, uint32_t rem_byte_off) {
    if (remote_ratio > local_ratio + 0.0005) return true;
    if (remote_ratio < local_ratio - 0.0005 || !has_exact_offset) return false;
    return rem_chapter > cur_chapter ||
        (rem_chapter == cur_chapter && rem_byte_off > cur_byte_off);
}

esp_err_t kosync_pull_book_progress(const char *file_path, kosync_progress_t *out_remote,
                                    uint16_t *out_chapter, uint32_t *out_byte_off,
                                    bool *has_exact_offset, char *message, size_t message_cap) {
    if (!file_path || !out_remote) return ESP_ERR_INVALID_ARG;
    memset(out_remote, 0, sizeof(*out_remote));
    if (out_chapter) *out_chapter = 0;
    if (out_byte_off) *out_byte_off = 0;
    if (has_exact_offset) *has_exact_offset = false;

    char doc_md5[33] = {0};
    esp_err_t err = kosync_compute_doc_md5(file_path, doc_md5);
    if (err != ESP_OK) {
        if (message && message_cap) snprintf(message, message_cap, "Không thể tính MD5 của sách");
        return err;
    }

    char path[96];
    snprintf(path, sizeof(path), "/syncs/progress/%s", doc_md5);

    char *resp = heap_caps_malloc(2048, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!resp) resp = malloc(2048);
    if (!resp) return ESP_ERR_NO_MEM;

    int status = 0;
    err = kosync_request(HTTP_METHOD_GET, path, NULL, &status, resp, 2048, message, message_cap);
    if (err != ESP_OK) {
        free(resp);
        return err;
    }
    if (status == 401) {
        free(resp);
        if (message && message_cap) snprintf(message, message_cap, "Sai tài khoản/mật khẩu KOSync");
        return ESP_ERR_INVALID_ARG;
    }
    if (status != 200 || !resp[0]) {
        free(resp);
        if (message && message_cap) snprintf(message, message_cap, "Chưa có tiến độ sách này trên KOSync");
        return ESP_ERR_NOT_FOUND;
    }

    cJSON *json = cJSON_Parse(resp);
    free(resp);
    if (!json) {
        if (message && message_cap) snprintf(message, message_cap, "Dữ liệu KOSync không hợp lệ");
        return ESP_ERR_INVALID_RESPONSE;
    }

    const cJSON *j_doc = cJSON_GetObjectItemCaseSensitive(json, "document");
    const cJSON *j_prog = cJSON_GetObjectItemCaseSensitive(json, "progress");
    const cJSON *j_pct = cJSON_GetObjectItemCaseSensitive(json, "percentage");
    const cJSON *j_dev = cJSON_GetObjectItemCaseSensitive(json, "device");
    const cJSON *j_dev_id = cJSON_GetObjectItemCaseSensitive(json, "device_id");
    const cJSON *j_ts = cJSON_GetObjectItemCaseSensitive(json, "timestamp");

    if (!cJSON_IsNumber(j_pct) && !cJSON_IsString(j_prog)) {
        cJSON_Delete(json);
        if (message && message_cap) snprintf(message, message_cap, "Chưa có tiến độ sách này trên KOSync");
        return ESP_ERR_NOT_FOUND;
    }

    if (cJSON_IsString(j_doc) && j_doc->valuestring)
        strlcpy(out_remote->document, j_doc->valuestring, sizeof(out_remote->document));
    if (cJSON_IsString(j_prog) && j_prog->valuestring)
        strlcpy(out_remote->progress, j_prog->valuestring, sizeof(out_remote->progress));
    if (cJSON_IsNumber(j_pct))
        out_remote->percentage = j_pct->valuedouble;
    if (cJSON_IsString(j_dev) && j_dev->valuestring)
        strlcpy(out_remote->device, j_dev->valuestring, sizeof(out_remote->device));
    if (cJSON_IsString(j_dev_id) && j_dev_id->valuestring)
        strlcpy(out_remote->device_id, j_dev_id->valuestring, sizeof(out_remote->device_id));
    if (cJSON_IsNumber(j_ts))
        out_remote->timestamp = (int64_t)j_ts->valuedouble;

    cJSON_Delete(json);

    kosync_parse_pointer(out_remote->progress, out_chapter, out_byte_off, has_exact_offset);

    if (message && message_cap) {
        snprintf(message, message_cap, "Đã tải tiến độ: %.1f%% (%s)",
                 out_remote->percentage * 100.0,
                 out_remote->device[0] ? out_remote->device : "KOSync");
    }
    return ESP_OK;
}

esp_err_t kosync_sync_book(const char *file_path, uint16_t *cur_chapter, uint32_t *cur_byte_off,
                           uint8_t *cur_pct, bool *pulled_newer, char *message, size_t message_cap) {
    if (!file_path || !file_path[0] || !cur_chapter || !cur_byte_off || !cur_pct || !pulled_newer) {
        return ESP_ERR_INVALID_ARG;
    }
    *pulled_newer = false;

    if (!app_settings_kosync_user()[0]) {
        if (message && message_cap) snprintf(message, message_cap, "Chưa đăng nhập KoSync");
        return ESP_ERR_INVALID_STATE;
    }

    bool started_wifi = false;
    esp_err_t net_err = kosync_acquire_network(&started_wifi, message, message_cap);
    if (net_err != ESP_OK) return net_err;

    // 1. Kéo tiến độ từ máy chủ về kiểm tra
    kosync_progress_t remote = {0};
    uint16_t rem_chap = 0;
    uint32_t rem_off = 0;
    bool has_exact = false;
    char pull_msg[96] = {0};
    esp_err_t pull_err = kosync_pull_book_progress(file_path, &remote, &rem_chap, &rem_off, &has_exact, pull_msg, sizeof(pull_msg));

    // 两端都是全书比例，按原始比例比较，不在 1% 上量化。
    // Both sides are whole-book ratios; compare them unquantised rather than at 1% steps.
    const double local_ratio = (double)*cur_pct / 100.0;
    if (pull_err == ESP_OK &&
        kosync_remote_wins(local_ratio, remote.percentage, has_exact,
                           *cur_chapter, *cur_byte_off, rem_chap, rem_off)) {
        uint8_t rem_pct = (uint8_t)(remote.percentage * 100.0 + 0.5);
        if (rem_pct > 100) rem_pct = 100;
        *cur_chapter = rem_chap;
        *cur_byte_off = has_exact ? rem_off : 0;
        *cur_pct = rem_pct;
        *pulled_newer = true;
        if (message && message_cap) {
            snprintf(message, message_cap, "Đã kéo tiến độ KoSync: %u%% (%s)",
                     (unsigned)rem_pct, remote.device[0] ? remote.device : "Thiết bị khác");
        }
        kosync_release_network(started_wifi);
        return ESP_OK;
    }

    // 2. Nếu máy cục bộ bằng hoặc xa hơn máy chủ (hoặc máy chủ chưa có), đẩy tiến độ lên máy chủ
    book_progress_t local_prog = {
        .chapter = *cur_chapter,
        .byte_off = *cur_byte_off,
        .pct = *cur_pct,
    };
    esp_err_t push_err = kosync_push_book_progress(file_path, &local_prog, (double)*cur_pct / 100.0, message, message_cap);
    kosync_release_network(started_wifi);
    return push_err;
}
