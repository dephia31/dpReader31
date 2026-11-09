/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0 */
#pragma once

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    const char *current;
    const char *previous;
    const char *temporary;
} font_settings_files_t;

static bool font_settings_read_file(const char *file, char *path, size_t capacity) {
    FILE *stream = fopen(file, "rb");
    if (!stream) return false;
    size_t size = fread(path, 1, capacity, stream);
    bool valid = size > 0 && !ferror(stream) && fgetc(stream) == EOF && !ferror(stream) &&
        path[size - 1] == '\0' && memchr(path, '\0', size - 1) == NULL;
    return fclose(stream) == 0 && valid;
}

static bool font_settings_read(const font_settings_files_t *files, char *path, size_t capacity) {
    return font_settings_read_file(files->current, path, capacity) ||
        font_settings_read_file(files->previous, path, capacity);
}

static bool font_settings_remove(const char *file) {
    return unlink(file) == 0 || errno == ENOENT;
}

static bool font_settings_clear(const font_settings_files_t *files) {
    // Remove the recovery copy first so an interrupted clear cannot revive it.
    return font_settings_remove(files->temporary) && font_settings_remove(files->previous) &&
        font_settings_remove(files->current);
}

static bool font_settings_write(const font_settings_files_t *files, const char *path,
                                char *scratch, size_t capacity) {
    FILE *stream = fopen(files->temporary, "wb");
    if (!stream) return false;
    size_t size = strlen(path) + 1;
    bool ok = fwrite(path, 1, size, stream) == size && fflush(stream) == 0 &&
        fsync(fileno(stream)) == 0;
    if (fclose(stream) != 0) ok = false;

    if (ok && font_settings_read_file(files->current, scratch, capacity)) {
        // FAT rename requires a free destination. Keep a valid previous choice until
        // the new file is in place, including when power is lost between renames.
        ok = font_settings_remove(files->previous) && rename(files->current, files->previous) == 0;
    } else if (ok) {
        ok = font_settings_remove(files->current);
    }
    if (ok) ok = rename(files->temporary, files->current) == 0;
    if (!ok) (void)font_settings_remove(files->temporary);
    return ok;
}
