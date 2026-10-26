/*
 * SPDX-FileCopyrightText: 2026 dpReader
 * SPDX-License-Identifier: Apache-2.0
 *
 * Triển khai OPDSBrowser (Calibre-Web Atom XML parser & Book Downloader) cho dpReader.
 */
#include "opds_client.h"
#include "settings.h"
#include "read_pico_sd.h"
#include "read_pico_transfer.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *TAG = "opds";

/* ---- Mã hóa Base64 nhỏ gọn cho HTTP Basic Auth ---- */
static void base64_encode(const uint8_t *in, size_t in_len, char *out, size_t out_cap) {
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t o = 0;
    for (size_t i = 0; i < in_len && o + 4 < out_cap; i += 3) {
        uint32_t v = (uint32_t)in[i] << 16;
        if (i + 1 < in_len) v |= (uint32_t)in[i + 1] << 8;
        if (i + 2 < in_len) v |= (uint32_t)in[i + 2];
        out[o++] = b64[(v >> 18) & 0x3F];
        out[o++] = b64[(v >> 12) & 0x3F];
        out[o++] = (i + 1 < in_len) ? b64[(v >> 6) & 0x3F] : '=';
        out[o++] = (i + 2 < in_len) ? b64[v & 0x3F] : '=';
    }
    if (out_cap) out[o] = 0;
}

static void opds_attach_auth(esp_http_client_handle_t client) {
    const char *user = app_settings_opds_user();
    const char *pass = app_settings_opds_pass();
    if (!user || !user[0]) return;
    char raw[132];
    snprintf(raw, sizeof(raw), "%s:%s", user, pass ? pass : "");
    char b64[192];
    base64_encode((const uint8_t *)raw, strlen(raw), b64, sizeof(b64));
    char hdr[210];
    snprintf(hdr, sizeof(hdr), "Basic %s", b64);
    esp_http_client_set_header(client, "Authorization", hdr);
}

/* ---- Giải mã thực thể XML (&amp;, &lt;, &gt;, &quot;, &apos;, &#...;) và gom khoảng trắng ---- */
static void xml_unescape_trim(const char *src, size_t src_len, char *dst, size_t dst_cap) {
    if (!dst || !dst_cap) return;
    size_t out = 0;
    bool prev_space = true;
    for (size_t i = 0; i < src_len && out + 1 < dst_cap;) {
        if (src[i] == '<') {
            /* Bỏ qua thẻ HTML lồng bên trong summary */
            while (i < src_len && src[i] != '>') ++i;
            if (i < src_len) ++i;
            continue;
        }
        char c = src[i];
        if (c == '&') {
            if (i + 5 <= src_len && !strncmp(src + i, "&amp;", 5)) { c = '&'; i += 5; }
            else if (i + 4 <= src_len && !strncmp(src + i, "&lt;", 4)) { c = '<'; i += 4; }
            else if (i + 4 <= src_len && !strncmp(src + i, "&gt;", 4)) { c = '>'; i += 4; }
            else if (i + 6 <= src_len && !strncmp(src + i, "&quot;", 6)) { c = '"'; i += 6; }
            else if (i + 6 <= src_len && !strncmp(src + i, "&apos;", 6)) { c = '\''; i += 6; }
            else if (i + 3 < src_len && src[i + 1] == '#') {
                const char *semi = memchr(src + i, ';', src_len - i);
                if (semi && (size_t)(semi - (src + i)) <= 10) {
                    uint32_t cp = (src[i + 2] == 'x' || src[i + 2] == 'X')
                                      ? (uint32_t)strtoul(src + i + 3, NULL, 16)
                                      : (uint32_t)strtoul(src + i + 2, NULL, 10);
                    i = (size_t)(semi - src) + 1;
                    if (cp >= 32 && cp < 128) {
                        c = (char)cp;
                    } else if (cp >= 0x80 && cp <= 0x7FF && out + 2 < dst_cap) {
                        dst[out++] = (char)(0xC0 | (cp >> 6));
                        dst[out++] = (char)(0x80 | (cp & 0x3F));
                        prev_space = false;
                        continue;
                    } else if (cp >= 0x800 && cp <= 0xFFFF && out + 3 < dst_cap) {
                        dst[out++] = (char)(0xE0 | (cp >> 12));
                        dst[out++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                        dst[out++] = (char)(0x80 | (cp & 0x3F));
                        prev_space = false;
                        continue;
                    } else {
                        continue;
                    }
                } else {
                    ++i;
                }
            } else {
                ++i;
            }
        } else {
            ++i;
        }
        if (c == '\r' || c == '\n' || c == '\t' || c == ' ') {
            if (!prev_space && out + 1 < dst_cap) {
                dst[out++] = ' ';
                prev_space = true;
            }
        } else {
            dst[out++] = c;
            prev_space = false;
        }
    }
    while (out > 0 && dst[out - 1] == ' ') --out;
    dst[out] = 0;
}

/* ---- Chuẩn hóa đường dẫn tương đối trong OPDS thành URL tuyệt đối ---- */
static void opds_resolve_url(const char *base_url, const char *href, char *out, size_t out_cap) {
    if (!out || !out_cap) return;
    out[0] = 0;
    if (!href || !href[0]) return;
    char clean_href[256];
    xml_unescape_trim(href, strlen(href), clean_href, sizeof(clean_href));
    if (!strncmp(clean_href, "http://", 7) || !strncmp(clean_href, "https://", 8)) {
        strlcpy(out, clean_href, out_cap);
        return;
    }
    if (!base_url || !base_url[0]) base_url = app_settings_opds_url();
    const char *scheme = strstr(base_url, "://");
    if (!scheme) {
        strlcpy(out, clean_href, out_cap);
        return;
    }
    const char *host_start = scheme + 3;
    const char *path_start = strchr(host_start, '/');
    size_t origin_len = path_start ? (size_t)(path_start - base_url) : strlen(base_url);
    if (clean_href[0] == '/') {
        size_t copy_n = origin_len < out_cap - 1 ? origin_len : out_cap - 1;
        memcpy(out, base_url, copy_n);
        out[copy_n] = 0;
        strlcat(out, clean_href, out_cap);
    } else {
        const char *last_slash = path_start ? strrchr(path_start, '/') : NULL;
        size_t prefix_len = last_slash ? (size_t)(last_slash - base_url + 1) : origin_len;
        size_t copy_n = prefix_len < out_cap - 1 ? prefix_len : out_cap - 1;
        memcpy(out, base_url, copy_n);
        out[copy_n] = 0;
        if (!last_slash) strlcat(out, "/", out_cap);
        strlcat(out, clean_href, out_cap);
    }
}

/* ---- Trích xuất nội dung thẻ XML và thuộc tính thẻ ---- */
static bool xml_extract_tag(const char *start, const char *end, const char *tag,
                            char *out, size_t out_cap) {
    if (out && out_cap) out[0] = 0;
    char open_pat[32], close_pat[32];
    snprintf(open_pat, sizeof(open_pat), "<%s", tag);
    snprintf(close_pat, sizeof(close_pat), "</%s>", tag);
    size_t open_len = strlen(open_pat);
    const char *p = start;
    while (p < end) {
        const char *found = strstr(p, open_pat);
        if (!found || found >= end) return false;
        char next_c = found[open_len];
        if (next_c == '>' || next_c == ' ' || next_c == '\t' || next_c == '\r' || next_c == '\n') {
            const char *gt = memchr(found + open_len, '>', (size_t)(end - (found + open_len)));
            if (!gt || gt >= end) return false;
            if (gt > found && gt[-1] == '/') return false; /* Thẻ tự đóng */
            const char *close_tag = strstr(gt + 1, close_pat);
            if (!close_tag || close_tag > end) return false;
            xml_unescape_trim(gt + 1, (size_t)(close_tag - (gt + 1)), out, out_cap);
            return true;
        }
        p = found + open_len;
    }
    return false;
}

static bool xml_extract_attr(const char *tag_start, const char *tag_end, const char *attr,
                             char *out, size_t out_cap) {
    if (out && out_cap) out[0] = 0;
    size_t attr_len = strlen(attr);
    const char *p = tag_start;
    while (p < tag_end) {
        const char *found = strstr(p, attr);
        if (!found || found >= tag_end) return false;
        bool boundary_ok = (found == tag_start || isspace((unsigned char)found[-1]));
        const char *eq = found + attr_len;
        while (eq < tag_end && isspace((unsigned char)*eq)) ++eq;
        if (boundary_ok && eq < tag_end && *eq == '=') {
            ++eq;
            while (eq < tag_end && isspace((unsigned char)*eq)) ++eq;
            if (eq < tag_end && (*eq == '"' || *eq == '\'')) {
                char quote = *eq++;
                const char *val_end = memchr(eq, quote, (size_t)(tag_end - eq));
                if (!val_end) return false;
                xml_unescape_trim(eq, (size_t)(val_end - eq), out, out_cap);
                return true;
            }
        }
        p = found + attr_len;
    }
    return false;
}

static bool str_contains_icase(const char *haystack, const char *needle) {
    if (!haystack || !needle || !needle[0]) return false;
    size_t nlen = strlen(needle);
    for (const char *p = haystack; *p; ++p) {
        if (!strncasecmp(p, needle, nlen)) return true;
    }
    return false;
}

static void parse_opds_xml(const char *base_url, const char *xml, size_t xml_len, opds_feed_t *feed) {
    memset(feed, 0, sizeof(*feed));
    const char *xml_end = xml + xml_len;
    const char *first_entry = strstr(xml, "<entry");
    const char *header_end = first_entry ? first_entry : xml_end;

    if (!xml_extract_tag(xml, header_end, "title", feed->feed_title, sizeof(feed->feed_title)) ||
        !feed->feed_title[0]) {
        strlcpy(feed->feed_title, "Thư viện OPDS", sizeof(feed->feed_title));
    }

    /* Tìm link phân trang rel="next" ở phần đầu feed */
    const char *lp = xml;
    while (lp < header_end) {
        const char *ltag = strstr(lp, "<link");
        if (!ltag || ltag >= header_end) break;
        const char *lgt = memchr(ltag, '>', (size_t)(header_end - ltag));
        if (!lgt) break;
        char rel[96] = {0}, href[256] = {0};
        xml_extract_attr(ltag, lgt, "rel", rel, sizeof(rel));
        xml_extract_attr(ltag, lgt, "href", href, sizeof(href));
        if (!strcasecmp(rel, "next") && href[0]) {
            opds_resolve_url(base_url, href, feed->next_url, sizeof(feed->next_url));
            break;
        }
        lp = lgt + 1;
    }

    /* Duyệt từng <entry> ... </entry> */
    const char *ep = first_entry;
    while (ep && ep < xml_end && feed->count < OPDS_MAX_ENTRIES) {
        const char *entry_open = strstr(ep, "<entry");
        if (!entry_open || entry_open >= xml_end) break;
        const char *entry_gt = memchr(entry_open, '>', (size_t)(xml_end - entry_open));
        if (!entry_gt) break;
        const char *entry_close = strstr(entry_gt + 1, "</entry>");
        if (!entry_close || entry_close > xml_end) break;

        opds_entry_t item = {0};
        xml_extract_tag(entry_gt + 1, entry_close, "title", item.title, sizeof(item.title));
        const char *author_open = strstr(entry_gt + 1, "<author");
        if (author_open && author_open < entry_close) {
            const char *author_close = strstr(author_open, "</author>");
            if (author_close && author_close <= entry_close) {
                xml_extract_tag(author_open, author_close, "name", item.author, sizeof(item.author));
            }
        }
        if (!xml_extract_tag(entry_gt + 1, entry_close, "summary", item.summary, sizeof(item.summary))) {
            xml_extract_tag(entry_gt + 1, entry_close, "content", item.summary, sizeof(item.summary));
        }

        char best_book_href[256] = {0};
        char best_book_ext[8] = {0};
        char best_nav_href[256] = {0};

        const char *cur = entry_gt + 1;
        while (cur < entry_close) {
            const char *ltag = strstr(cur, "<link");
            if (!ltag || ltag >= entry_close) break;
            const char *lgt = memchr(ltag, '>', (size_t)(entry_close - ltag));
            if (!lgt) break;

            char rel[128] = {0}, type[128] = {0}, href[256] = {0};
            xml_extract_attr(ltag, lgt, "rel", rel, sizeof(rel));
            xml_extract_attr(ltag, lgt, "type", type, sizeof(type));
            xml_extract_attr(ltag, lgt, "href", href, sizeof(href));

            if (href[0]) {
                bool is_epub = str_contains_icase(type, "epub") || str_contains_icase(href, ".epub") ||
                               str_contains_icase(href, "/epub/");
                bool is_txt = str_contains_icase(type, "text/plain") || str_contains_icase(href, ".txt") ||
                              str_contains_icase(href, "/txt/");
                bool is_acq = str_contains_icase(rel, "acquisition");
                if (is_epub && (!best_book_href[0] || strcmp(best_book_ext, "epub") != 0)) {
                    opds_resolve_url(base_url, href, best_book_href, sizeof(best_book_href));
                    strlcpy(best_book_ext, "epub", sizeof(best_book_ext));
                } else if (is_txt && !best_book_href[0]) {
                    opds_resolve_url(base_url, href, best_book_href, sizeof(best_book_href));
                    strlcpy(best_book_ext, "txt", sizeof(best_book_ext));
                } else if (is_acq && !best_book_href[0] &&
                           (is_epub || is_txt || !type[0] || str_contains_icase(type, "octet-stream"))) {
                    opds_resolve_url(base_url, href, best_book_href, sizeof(best_book_href));
                    strlcpy(best_book_ext, is_txt ? "txt" : "epub", sizeof(best_book_ext));
                } else if (!best_nav_href[0] &&
                           (str_contains_icase(type, "atom+xml") ||
                            str_contains_icase(rel, "subsection") ||
                            str_contains_icase(rel, "navigation") ||
                            !rel[0])) {
                    opds_resolve_url(base_url, href, best_nav_href, sizeof(best_nav_href));
                }
            }
            cur = lgt + 1;
        }

        if (item.title[0] && (best_book_href[0] || best_nav_href[0])) {
            if (best_book_href[0]) {
                item.is_book = true;
                strlcpy(item.href, best_book_href, sizeof(item.href));
                strlcpy(item.ext, best_book_ext, sizeof(item.ext));
            } else {
                item.is_book = false;
                strlcpy(item.href, best_nav_href, sizeof(item.href));
            }
            feed->entries[feed->count++] = item;
        }
        ep = entry_close + 8;
    }
}

/* ---- Quản lý kết nối mạng WiFi cho OPDS ---- */
static esp_err_t opds_acquire_network(bool *started_by_us, char *msg, size_t msg_cap) {
    *started_by_us = false;
    read_pico_transfer_status_t st = {0};
    read_pico_transfer_get_status(&st);
    if (st.mode == READ_PICO_TRANSFER_MODE_STA && st.network_ready) return ESP_OK;
    if (st.state != READ_PICO_TRANSFER_STOPPED) {
        if (msg && msg_cap) snprintf(msg, msg_cap, "Đang bật phát WiFi nội bộ, hãy chuyển sang mạng WiFi");
        return ESP_ERR_INVALID_STATE;
    }
    char ssid[33] = {0};
    bool configured = false;
    if (read_pico_transfer_get_saved_wifi(ssid, &configured) != ESP_OK || !configured) {
        if (msg && msg_cap) snprintf(msg, msg_cap, "Chưa cấu hình mạng WiFi");
        return ESP_ERR_NOT_FOUND;
    }
    read_pico_transfer_cfg_t cfg = {
        .mode = READ_PICO_TRANSFER_MODE_STA,
        .network_only = true,
    };
    esp_err_t err = read_pico_transfer_start(&cfg);
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

static void opds_release_network(bool started_by_us) {
    if (started_by_us) read_pico_transfer_stop();
}

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} opds_xml_buf_t;

static esp_err_t opds_feed_http_event(esp_http_client_event_t *evt) {
    if (evt->event_id == HTTP_EVENT_ON_DATA && evt->user_data && evt->data && evt->data_len > 0) {
        opds_xml_buf_t *b = (opds_xml_buf_t *)evt->user_data;
        size_t avail = (b->cap > b->len + 1) ? (b->cap - b->len - 1) : 0;
        size_t copy = (size_t)evt->data_len < avail ? (size_t)evt->data_len : avail;
        if (copy > 0) {
            memcpy(b->buf + b->len, evt->data, copy);
            b->len += copy;
            b->buf[b->len] = 0;
        }
    }
    return ESP_OK;
}

esp_err_t opds_fetch_feed(const char *url, opds_feed_t *out_feed, char *message, size_t message_cap) {
    if (!out_feed) return ESP_ERR_INVALID_ARG;
    memset(out_feed, 0, sizeof(*out_feed));
    const char *target_url = (url && url[0]) ? url : app_settings_opds_url();
    if (!target_url || !target_url[0]) target_url = DPREADER_DEFAULT_OPDS_URL;

    bool started_wifi = false;
    esp_err_t err = opds_acquire_network(&started_wifi, message, message_cap);
    if (err != ESP_OK) return err;

    const size_t xml_cap = 131072; /* 128 KB trong PSRAM cho Atom XML */
    char *xml_buf = heap_caps_malloc(xml_cap, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!xml_buf) xml_buf = malloc(32768);
    if (!xml_buf) {
        opds_release_network(started_wifi);
        if (message && message_cap) snprintf(message, message_cap, "Không đủ bộ nhớ tải danh mục OPDS");
        return ESP_ERR_NO_MEM;
    }
    xml_buf[0] = 0;
    opds_xml_buf_t rx = { .buf = xml_buf, .len = 0, .cap = xml_cap };

    esp_http_client_config_t cfg = {
        .url = target_url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 12000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = opds_feed_http_event,
        .user_data = &rx,
        .buffer_size = 4096,
        .buffer_size_tx = 1024,
        .max_redirection_count = 4,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        free(xml_buf);
        opds_release_network(started_wifi);
        return ESP_ERR_NO_MEM;
    }
    esp_http_client_set_header(client, "User-Agent", "dpReader/1.0 (Metalio47; ESP32-S31; OPDS)");
    esp_http_client_set_header(client, "Accept", "application/atom+xml, application/xml, text/xml, */*");
    opds_attach_auth(client);

    err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    opds_release_network(started_wifi);

    if (err != ESP_OK) {
        free(xml_buf);
        ESP_LOGW(TAG, "OPDS fetch %s failed: %s", target_url, esp_err_to_name(err));
        if (message && message_cap) snprintf(message, message_cap, "Không kết nối được OPDS");
        return err;
    }
    if (status == 401) {
        free(xml_buf);
        if (message && message_cap) snprintf(message, message_cap, "OPDS yêu cầu đăng nhập (HTTP 401)");
        return ESP_ERR_INVALID_ARG;
    }
    if (status != 200 || rx.len == 0) {
        free(xml_buf);
        if (message && message_cap) snprintf(message, message_cap, "Máy chủ OPDS trả lỗi HTTP %d", status);
        return ESP_FAIL;
    }

    parse_opds_xml(target_url, xml_buf, rx.len, out_feed);
    free(xml_buf);
    if (message && message_cap) {
        snprintf(message, message_cap, "Đã tải %u mục từ %s",
                 (unsigned)out_feed->count, out_feed->feed_title);
    }
    return ESP_OK;
}

static void sanitize_filename(const char *title, const char *ext, char *out, size_t out_cap) {
    size_t j = 0;
    for (size_t i = 0; title && title[i] && j + 12 < out_cap; ++i) {
        unsigned char c = (unsigned char)title[i];
        if (c < 32 || c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            if (j > 0 && out[j - 1] != ' ') out[j++] = ' ';
        } else {
            out[j++] = (char)c;
        }
    }
    while (j > 0 && (out[j - 1] == ' ' || out[j - 1] == '.')) --j;
    out[j] = 0;
    if (!out[0]) strlcpy(out, "Sach-OPDS", out_cap);
    const char *use_ext = (ext && ext[0]) ? ext : "epub";
    size_t n = strlen(out), elen = strlen(use_ext);
    if (n <= elen + 1 || out[n - elen - 1] != '.' || strcasecmp(out + n - elen, use_ext) != 0) {
        snprintf(out + n, out_cap - n, ".%s", use_ext);
    }
}

esp_err_t opds_download_book(const opds_entry_t *entry, char *saved_path, size_t saved_path_cap,
                             opds_progress_cb_t progress_cb, void *user_ctx,
                             char *message, size_t message_cap) {
    if (!entry || !entry->is_book || !entry->href[0]) return ESP_ERR_INVALID_ARG;
    if (saved_path && saved_path_cap) saved_path[0] = 0;

    read_pico_sd_info_t sd = {0};
    const bool sd_ok = (read_pico_sd_get_info(&sd) == ESP_OK && sd.mounted);
    const char *books_dir = sd_ok ? app_settings_books_dir() : "/flash/books";
    (void)mkdir(books_dir, 0775);

    char filename[160];
    sanitize_filename(entry->title, entry->ext, filename, sizeof(filename));

    char final_path[288], tmp_path[296];
    snprintf(final_path, sizeof(final_path), "%s/%s", books_dir, filename);
    snprintf(tmp_path, sizeof(tmp_path), "%s.part", final_path);

    bool started_wifi = false;
    esp_err_t err = opds_acquire_network(&started_wifi, message, message_cap);
    if (err != ESP_OK) return err;

    esp_http_client_config_t cfg = {
        .url = entry->href,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 15000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .buffer_size = 4096,
        .buffer_size_tx = 1024,
        .max_redirection_count = 4,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        opds_release_network(started_wifi);
        return ESP_ERR_NO_MEM;
    }
    esp_http_client_set_header(client, "User-Agent", "dpReader/1.0 (Metalio47; ESP32-S31; OPDS)");
    opds_attach_auth(client);

    err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        esp_http_client_cleanup(client);
        opds_release_network(started_wifi);
        if (message && message_cap) snprintf(message, message_cap, "Không thể mở kết nối tải sách");
        return err;
    }

    int64_t content_len = esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    if (status != 200) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        opds_release_network(started_wifi);
        if (message && message_cap) snprintf(message, message_cap, "Tải sách thất bại (HTTP %d)", status);
        return ESP_FAIL;
    }

    FILE *f = fopen(tmp_path, "wb");
    if (!f) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        opds_release_network(started_wifi);
        if (message && message_cap) snprintf(message, message_cap, "Không thể ghi file vào bộ nhớ");
        return ESP_FAIL;
    }

    uint8_t *chunk = heap_caps_malloc(8192, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!chunk) chunk = malloc(4096);
    size_t chunk_cap = chunk ? 4096 : 0;
    if (!chunk) {
        fclose(f);
        unlink(tmp_path);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        opds_release_network(started_wifi);
        return ESP_ERR_NO_MEM;
    }

    size_t total = content_len > 0 ? (size_t)content_len : 0;
    size_t received = 0;
    int64_t last_ui_us = 0;
    bool write_ok = true;
    while (1) {
        int r = esp_http_client_read(client, (char *)chunk, (int)chunk_cap);
        if (r < 0) {
            write_ok = false;
            break;
        }
        if (r == 0) break;
        if (fwrite(chunk, 1, (size_t)r, f) != (size_t)r) {
            write_ok = false;
            break;
        }
        received += (size_t)r;
        int64_t now_us = esp_timer_get_time();
        if (progress_cb && (now_us - last_ui_us >= 350000 || (total && received >= total))) {
            last_ui_us = now_us;
            progress_cb(received, total, user_ctx);
        }
    }
    free(chunk);
    fflush(f);
    fclose(f);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    opds_release_network(started_wifi);

    if (!write_ok || received == 0) {
        unlink(tmp_path);
        if (message && message_cap) snprintf(message, message_cap, "Tải sách bị gián đoạn");
        return ESP_FAIL;
    }

    unlink(final_path);
    if (rename(tmp_path, final_path) != 0) {
        unlink(tmp_path);
        if (message && message_cap) snprintf(message, message_cap, "Không thể lưu file sách");
        return ESP_FAIL;
    }

    if (saved_path && saved_path_cap) strlcpy(saved_path, final_path, saved_path_cap);
    if (message && message_cap) {
        snprintf(message, message_cap, "Đã tải xong: %s (%u KB)",
                 filename, (unsigned)((received + 1023) / 1024));
    }
    return ESP_OK;
}
