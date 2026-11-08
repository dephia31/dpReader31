/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef TRANSFER_WIFI_DIRECTORY
#define TRANSFER_WIFI_DIRECTORY "/sdcard/DPRWIFI"
#endif
#define WIFI_PROFILES_FILE TRANSFER_WIFI_DIRECTORY "/WIFI.BIN"
#define WIFI_PROFILES_LIMIT READ_PICO_TRANSFER_WIFI_PROFILES_MAX

static esp_err_t wifi_profiles_open(FILE **file, unsigned *count) {
    *count = 0;
    *file = fopen(WIFI_PROFILES_FILE, "rb");
    if (!*file && errno == ENOENT)
        *file = fopen(WIFI_PROFILES_FILE ".old", "rb");
    if (!*file) return errno == ENOENT ? ESP_OK : ESP_FAIL;
    setvbuf(*file, NULL, _IONBF, 0);
    struct stat info;
    char magic[4];
    if (fstat(fileno(*file), &info) == 0 && info.st_size >= (off_t)sizeof(magic) &&
        (size_t)info.st_size <= sizeof(magic) + WIFI_PROFILES_LIMIT * sizeof(transfer_credentials_t) &&
        (info.st_size - sizeof(magic)) % sizeof(transfer_credentials_t) == 0 &&
        fread(magic, 1, sizeof(magic), *file) == sizeof(magic) && !memcmp(magic, "WFL1", 4)) {
        *count = (info.st_size - sizeof(magic)) / sizeof(transfer_credentials_t);
        return ESP_OK;
    }
    fclose(*file);
    *file = NULL;
    return ESP_ERR_INVALID_STATE;
}

static esp_err_t wifi_profile_read(unsigned index, transfer_credentials_t *out, unsigned *count) {
    memset(out, 0, sizeof(*out));
    FILE *file;
    esp_err_t err = wifi_profiles_open(&file, count);
    if (err != ESP_OK) return err;
    if (!file) {
        if (index) return ESP_ERR_NOT_FOUND;
        err = load_legacy_credentials(out);
        *count = out->version == 1;
        return err;
    }
    if (index < *count &&
        (fseek(file, 4 + index * sizeof(*out), SEEK_SET) ||
         fread(out, sizeof(*out), 1, file) != 1 || !credentials_valid(out))) {
        clear_secret(out, sizeof(*out));
        err = ESP_ERR_INVALID_STATE;
    }
    fclose(file);
    return err;
}

static esp_err_t load_credentials(transfer_credentials_t *out) {
    unsigned count;
    return wifi_profile_read(0, out, &count);
}

static esp_err_t wifi_profiles_publish(void) {
    struct stat info;
    if (stat(WIFI_PROFILES_FILE, &info) == 0) {
        if (remove(WIFI_PROFILES_FILE ".old") && errno != ENOENT) return ESP_FAIL;
        if (rename(WIFI_PROFILES_FILE, WIFI_PROFILES_FILE ".old")) return ESP_FAIL;
    } else if (errno != ENOENT) return ESP_FAIL;
    if (rename(WIFI_PROFILES_FILE ".tmp", WIFI_PROFILES_FILE)) return ESP_FAIL;
    (void)remove(WIFI_PROFILES_FILE ".old");
    return ESP_OK;
}

static esp_err_t store_credentials(const transfer_credentials_t *next) {
    if (next && !credentials_valid(next)) return ESP_ERR_INVALID_ARG;
    FILE *existing;
    unsigned count;
    esp_err_t err = wifi_profiles_open(&existing, &count);
    if (err != ESP_OK) return err;
    if (!next && !existing) return store_legacy_credentials(NULL);
    if (mkdir(TRANSFER_WIFI_DIRECTORY, 0700) && errno != EEXIST) {
        if (existing) fclose(existing);
        return ESP_FAIL;
    }
    FILE *file = fopen(WIFI_PROFILES_FILE ".tmp", "wb");
    if (!file) {
        if (existing) fclose(existing);
        return ESP_FAIL;
    }
    setvbuf(file, NULL, _IONBF, 0);
    bool ok = fwrite("WFL1", 1, 4, file) == 4;
    unsigned written = 0;
    if (ok && next) {
        ok = fwrite(next, sizeof(*next), 1, file) == 1;
        written = 1;
    }
    transfer_credentials_t previous = {0};
    if (ok && next && !existing) {
        err = load_legacy_credentials(&previous);
        ok = err == ESP_OK;
        count = previous.version == 1;
    }
    for (unsigned i = 0; ok && next && i < count; ++i) {
        if (existing && (fread(&previous, sizeof(previous), 1, existing) != 1 ||
                         !credentials_valid(&previous))) {
            ok = false;
            break;
        }
        if (!strcmp(previous.ssid, next->ssid)) continue;
        if (written == WIFI_PROFILES_LIMIT) {
            err = ESP_ERR_INVALID_SIZE;
            ok = false;
            break;
        }
        ok = fwrite(&previous, sizeof(previous), 1, file) == 1;
        ++written;
    }
    clear_secret(&previous, sizeof(previous));
    if (existing) fclose(existing);
    if (ok) ok = fflush(file) == 0 && fsync(fileno(file)) == 0;
    if (fclose(file)) ok = false;
    if (ok && !next) {
        err = store_legacy_credentials(NULL);
        ok = err == ESP_OK;
    }
    if (ok) err = wifi_profiles_publish();
    else if (err == ESP_OK) err = ESP_FAIL;
    if (err != ESP_OK) (void)remove(WIFI_PROFILES_FILE ".tmp");
    return err;
}
