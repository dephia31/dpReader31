/* SPDX-License-Identifier: Apache-2.0
 * Shared HTTP download handler for both board backends.
 * Uses the transfer session's existing buffer; never loads a whole file.
 */
static esp_err_t file_download_handler(httpd_req_t *req) {
    if (!management_request(req))
        return wifi_response(req, "403 Forbidden", "{\"error\":\"Vui lòng mở từ địa chỉ hiển thị trên máy\"}");
    if (s_cfg.is_flash)
        return wifi_response(req, "400 Bad Request", "{\"error\":\"Thẻ nhớ chưa sẵn sàng\"}");
    char query[800], encoded[720], relative[240], path[256];
    if (req->content_len ||
        httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "path", encoded, sizeof(encoded)) != ESP_OK ||
        !sd_decode_relative(encoded, relative, false) || !sd_absolute(relative, path, sizeof(path)))
        return respond_error(req, 400);
    portENTER_CRITICAL(&s_lock);
    bool accepted = admission_begin(s_stopping, &s_upload_active);
    portEXIT_CRITICAL(&s_lock);
    if (!accepted)
        return wifi_response(req, "409 Conflict", "{\"error\":\"Đang truyền tệp hoặc chuyển mạng, vui lòng thử lại sau\"}");
    struct stat st;
    FILE *file = NULL;
    esp_err_t result;
    if (stat(path, &st) || !S_ISREG(st.st_mode) || !(file = fopen(path, "rb"))) {
        result = wifi_response(req, "404 Not Found", "{\"error\":\"Không tìm thấy tệp. Nếu tải log, hãy xuất log ra thẻ nhớ trên máy đọc trước.\"}");
        goto done;
    }
    if (!s_buffer || !s_buffer_capacity) { result = respond_error(req, 500); goto done; }
    // Percent-encode UTF-8 bytes for mobile browsers; never put raw names in headers.
    const char *name = strrchr(relative, '/');
    name = name ? name + 1 : relative;
    static const char hex[] = "0123456789ABCDEF";
    size_t fallback = 0;
    for (const unsigned char *p = (const unsigned char *)name; *p && fallback < 32; ++p)
        encoded[fallback++] = *p < 127 ? (char)*p : '_';
    encoded[fallback] = 0;
    size_t used = (size_t)snprintf(query, sizeof(query), "attachment; filename=\"%s\"; filename*=UTF-8''", encoded);
    for (const unsigned char *p = (const unsigned char *)name; *p; ++p) {
        query[used++] = '%'; query[used++] = hex[*p >> 4]; query[used++] = hex[*p & 15];
    }
    query[used] = 0; // At most 238*3 bytes plus prefix and 32 fallback bytes fit query.
    httpd_resp_set_type(req, "application/octet-stream");
    httpd_resp_set_hdr(req, "Content-Disposition", query);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
    result = ESP_OK;
    for (;;) {
        size_t n = fread(s_buffer, 1, s_buffer_capacity, file);
        if (n && (result = httpd_resp_send_chunk(req, s_buffer, n)) != ESP_OK) break;
        if (ferror(file)) { result = ESP_FAIL; break; }
        if (!n) break;
    }
    if (fclose(file)) result = ESP_FAIL;
    file = NULL;
    // Only terminate a complete response; a read/send failure closes the connection.
    if (result == ESP_OK) result = httpd_resp_send_chunk(req, NULL, 0);
done:
    if (file) fclose(file);
    portENTER_CRITICAL(&s_lock);
    s_upload_active = false;
    portEXIT_CRITICAL(&s_lock);
    return result;
}
