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
#define BLE_PROFILE_V1_MAGIC 0x31454c42u
#define BLE_PROFILE_MAGIC 0x32454c42u

typedef struct {
    uint8_t actions[BLE_PT_MAX_BINDINGS];
    ble_pt_frame_t frames[BLE_PT_MAX_BINDINGS];
} ble_profile_map_t;

typedef struct {
    uint32_t magic;
    uint32_t sequence;
    uint32_t count;
    ble_pt_bond_t peers[BLE_PT_MAX_BONDS];
    ble_pt_bindings_backup_t bindings;
    uint32_t checksum;
} ble_profile_v1_t;

typedef struct {
    uint32_t magic;
    uint32_t sequence;
    uint32_t count;
    ble_pt_bond_t peers[BLE_PT_MAX_BONDS];
    ble_pt_bindings_backup_t bindings;
    ble_profile_map_t maps[BLE_PT_MAX_BONDS];
    uint32_t checksum;
} ble_profile_disk_t;

static uint32_t profile_hash(const void *record, size_t bytes) {
    const uint8_t *data = record;
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < bytes; ++i) hash = (hash ^ data[i]) * 16777619u;
    return hash;
}

static uint32_t profile_checksum(const ble_profile_disk_t *value) {
    return profile_hash(value, offsetof(ble_profile_disk_t, checksum));
}

static bool profile_valid(const ble_profile_disk_t *value) {
    if (value->magic != BLE_PROFILE_MAGIC || !value->sequence ||
        value->count > BLE_PT_MAX_BONDS || value->checksum != profile_checksum(value) ||
        !ble_pt_bindings_backup_valid(&value->bindings)) return false;
    for (unsigned i = 0; i < BLE_PT_MAX_BONDS; ++i) {
        const ble_profile_map_t *map = &value->maps[i];
        for (unsigned j = 0; j < BLE_PT_MAX_BINDINGS; ++j) {
            const ble_pt_frame_t *frame = &map->frames[j];
            if (map->actions[j] >= BLE_PT_ACTION_COUNT ||
                (frame->len != 0) != (map->actions[j] != BLE_PT_ACTION_NONE) ||
                (frame->len && !value->bindings.entries[i].key[0]) ||
                !binding_backup_frame_valid((const uint8_t *)frame)) return false;
            for (unsigned k = 0; frame->len && k < j; ++k) {
                if (frame->len == map->frames[k].len &&
                    !memcmp(frame->bytes, map->frames[k].bytes, frame->len)) return false;
            }
        }
    }
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
    size_t size = fread(value, 1, sizeof(*value), file);
    bool complete = fgetc(file) == EOF && !ferror(file);
    fclose(file);
    if (complete && size == sizeof(ble_profile_v1_t)) {
        ble_profile_v1_t old;
        memcpy(&old, value, sizeof(old));
        if (old.magic != BLE_PROFILE_V1_MAGIC ||
            old.checksum != profile_hash(&old, offsetof(ble_profile_v1_t, checksum)))
            return ESP_ERR_INVALID_STATE;
        memset(value, 0, sizeof(*value));
        value->magic = BLE_PROFILE_MAGIC;
        value->sequence = old.sequence;
        value->count = old.count;
        memcpy(value->peers, old.peers, sizeof(old.peers));
        value->bindings = old.bindings;
        value->checksum = profile_checksum(value);
        size = sizeof(*value);
    }
    bool ok = complete && size == sizeof(*value) && profile_valid(value);
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
