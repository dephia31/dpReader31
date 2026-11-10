/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0 */
/*
 * 中文：BLE 配置快照，主循环独占文件 I/O，两个校验槽保留上次成功记录。
 * English: BLE profile snapshots. The UI owner performs file I/O; two checked slots retain the last committed record.
 */
#pragma once
#include "read_pico_sd.h"
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stddef.h>

#ifndef BLE_PROFILE_DIRECTORY
#define BLE_PROFILE_DIRECTORY "/sdcard/DPRBLE"
#endif
#define BLE_PROFILE_MAGIC 0x31454c42u

typedef struct {
    uint32_t magic;
    uint32_t sequence;
    uint32_t count;
    ble_pt_bond_t peers[BLE_PT_MAX_BONDS];
    ble_pt_bindings_backup_t bindings;
    uint32_t checksum;
} ble_profile_disk_t;

static uint32_t profile_checksum(const ble_profile_disk_t *value) {
    const uint8_t *bytes = (const uint8_t *)value;
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < offsetof(ble_profile_disk_t, checksum); ++i)
        hash = (hash ^ bytes[i]) * 16777619u;
    return hash;
}

static bool profile_valid(const ble_profile_disk_t *value) {
    if (value->magic != BLE_PROFILE_MAGIC || !value->sequence ||
        value->count > BLE_PT_MAX_BONDS || value->checksum != profile_checksum(value) ||
        !ble_pt_bindings_backup_valid(&value->bindings)) return false;
    for (unsigned i = 0; i < value->count; ++i) {
        const ble_pt_bond_t *peer = &value->peers[i];
        if (strnlen(peer->addr, sizeof(peer->addr)) != 17 ||
            strnlen(peer->name, sizeof(peer->name)) == sizeof(peer->name) || peer->addr_type > 3) return false;
        for (unsigned j = 0; j < 17; ++j) {
            char c = peer->addr[j];
            if (j % 3 == 2) { if (c != ':') return false; }
            else if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
        }
        for (unsigned j = 0; j < i; ++j)
            if (!strcasecmp(peer->addr, value->peers[j].addr)) return false;
    }
    return true;
}

static esp_err_t profile_read_slot(unsigned slot, ble_profile_disk_t *value) {
    char path[128];
    snprintf(path, sizeof(path), BLE_PROFILE_DIRECTORY "/PROFILE%u.BIN", slot);
    FILE *file = fopen(path, "rb");
    if (!file) return errno == ENOENT ? ESP_ERR_NOT_FOUND : ESP_FAIL;
    memset(value, 0, sizeof(*value));
    bool ok = fread(value, 1, sizeof(*value), file) == sizeof(*value) &&
              fgetc(file) == EOF && !ferror(file) && profile_valid(value);
    fclose(file);
    return ok ? ESP_OK : ESP_ERR_INVALID_STATE;
}

static esp_err_t profile_read(ble_profile_disk_t *value) {
    if (!read_pico_sd_io_ready()) return ESP_ERR_INVALID_STATE;
    ble_profile_disk_t first, second;
    esp_err_t a = profile_read_slot(0, &first), b = profile_read_slot(1, &second);
    if (a == ESP_OK || b == ESP_OK) {
        *value = a == ESP_OK && (b != ESP_OK || first.sequence >= second.sequence) ? first : second;
        return ESP_OK;
    }
    return a == ESP_ERR_NOT_FOUND && b == ESP_ERR_NOT_FOUND ? ESP_ERR_NOT_FOUND : ESP_FAIL;
}

static esp_err_t profile_write(ble_profile_disk_t *value) {
    if (!read_pico_sd_io_ready() || value->sequence == UINT32_MAX) return ESP_ERR_INVALID_STATE;
    if (mkdir(BLE_PROFILE_DIRECTORY, 0775) && errno != EEXIST) return ESP_FAIL;
    ble_profile_disk_t next = *value;
    next.magic = BLE_PROFILE_MAGIC;
    ++next.sequence;
    next.checksum = profile_checksum(&next);
    if (!profile_valid(&next)) return ESP_ERR_INVALID_ARG;
    char path[128];
    snprintf(path, sizeof(path), BLE_PROFILE_DIRECTORY "/PROFILE%u.BIN", (unsigned)(next.sequence & 1u));
    FILE *file = fopen(path, "wb");
    if (!file) return ESP_FAIL;
    bool ok = fwrite(&next, 1, sizeof(next), file) == sizeof(next);
    if (fflush(file)) ok = false;
    if (ok && fsync(fileno(file))) ok = false;
    if (fclose(file)) ok = false;
    if (!ok) return ESP_FAIL;
    *value = next;
    return ESP_OK;
}
