/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

bool read_pico_transfer_wifi_profiles_backup_valid(
    const read_pico_transfer_wifi_backup_t profiles[READ_PICO_TRANSFER_WIFI_PROFILES_MAX]) {
    if (!profiles) return false;
    bool ended = false;
    for (unsigned i = 0; i < WIFI_PROFILES_LIMIT; ++i) {
        if (!read_pico_transfer_wifi_backup_valid(&profiles[i])) return false;
        if (!profiles[i].configured) ended = true;
        else if (ended) return false;
        else for (unsigned j = 0; j < i; ++j)
            if (!strcmp(profiles[i].ssid, profiles[j].ssid)) return false;
    }
    return true;
}

esp_err_t read_pico_transfer_export_wifi_profiles_backup(
    read_pico_transfer_wifi_backup_t out[READ_PICO_TRANSFER_WIFI_PROFILES_MAX]) {
    if (!out) return ESP_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out) * WIFI_PROFILES_LIMIT);
    if (!claim_config()) return ESP_ERR_INVALID_STATE;
    FILE *file;
    unsigned count;
    esp_err_t err = wifi_profiles_open(&file, &count);
    transfer_credentials_t saved = {0};
    if (err == ESP_OK && !file) {
        err = load_legacy_credentials(&saved);
        count = saved.version == 1;
    }
    for (unsigned i = 0; i < count && err == ESP_OK; ++i) {
        if (file && (fread(&saved, sizeof(saved), 1, file) != 1 || !credentials_valid(&saved))) {
            err = ESP_ERR_INVALID_STATE;
            break;
        }
        out[i].configured = 1;
        memcpy(out[i].ssid, saved.ssid, sizeof(saved.ssid));
        memcpy(out[i].password, saved.password, sizeof(saved.password));
    }
    if (file) fclose(file);
    clear_secret(&saved, sizeof(saved));
    release_config();
    if (err != ESP_OK) clear_secret(out, sizeof(*out) * WIFI_PROFILES_LIMIT);
    return err;
}

esp_err_t read_pico_transfer_import_wifi_profiles_backup(
    const read_pico_transfer_wifi_backup_t profiles[READ_PICO_TRANSFER_WIFI_PROFILES_MAX]) {
    TRANSFER_NETWORK_GUARD_OR_RETURN(ESP_ERR_INVALID_STATE);
    if (!read_pico_transfer_wifi_profiles_backup_valid(profiles)) return ESP_ERR_INVALID_ARG;
    if (!claim_config()) return ESP_ERR_INVALID_STATE;
    esp_err_t err = ESP_FAIL;
    if (mkdir(TRANSFER_WIFI_DIRECTORY, 0700) && errno != EEXIST) goto done;
    FILE *file = fopen(WIFI_PROFILES_FILE ".tmp", "wb");
    if (!file) goto done;
    bool ok = fwrite("WFL1", 1, 4, file) == 4;
    transfer_credentials_t next = {0};
    for (unsigned i = 0; i < WIFI_PROFILES_LIMIT && ok && profiles[i].configured; ++i) {
        next.version = 1;
        memcpy(next.ssid, profiles[i].ssid, sizeof(next.ssid));
        memcpy(next.password, profiles[i].password, sizeof(next.password));
        ok = fwrite(&next, sizeof(next), 1, file) == 1;
    }
    if (ok) ok = fflush(file) == 0 && fsync(fileno(file)) == 0;
    if (fclose(file)) ok = false;
    if (ok) err = wifi_profiles_publish();
    if (err == ESP_OK && !s_wifi) {
        memcpy(next.ssid, profiles[0].ssid, sizeof(next.ssid));
        memcpy(next.password, profiles[0].password, sizeof(next.password));
        publish_credentials(profiles[0].configured ? &next : NULL);
    }
    clear_secret(&next, sizeof(next));
    if (err != ESP_OK) (void)remove(WIFI_PROFILES_FILE ".tmp");
done:
    release_config();
    return err;
}
