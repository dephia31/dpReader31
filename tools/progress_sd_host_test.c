/* SPDX-License-Identifier: Apache-2.0 */
#define main legacy_progress_tests
#include "book_storage_host_test.c"
#undef main
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
static bool visit_fallback(const char *path, const book_progress_t *progress, void *ctx) {
    (void)path;
    assert(progress->byte_off == 840);
    ++*(unsigned *)ctx;
    return true;
}
int main(int argc, char **argv) {
    (void)argv;
    book_progress_t value = {.file_size = 900, .chapter = 3, .byte_off = 420, .px = 32, .pct = 40},
                    restored = {0};
    const char *path = "/sdcard/books/fallback.epub";
    if (argc > 1) {
        unsigned count = 0;
        assert(book_progress_list(visit_fallback, &count) == ESP_OK && count == 1);
        assert(book_progress_load(path, 900, &restored) && restored.byte_off == 840);
        char out[288];
        assert(book_progress_last_path(out, sizeof(out)) && !strcmp(out, path));
        assert(!book_progress_load(path, 901, &restored));
        assert(book_progress_clear(path) == ESP_OK && !book_progress_load(path, 900, &restored));
        assert(book_progress_save(path, &value) == ESP_OK);
        assert(book_progress_forget(path) == ESP_OK && !book_progress_load(path, 900, &restored));
        assert(!book_progress_last_path(out, sizeof(out)));
        puts("PASS restart, size guard, clear, save and forget");
        return 0;
    }
    seq_failure = ESP_ERR_NVS_NOT_ENOUGH_SPACE;
    assert(book_progress_save(path, &value) == ESP_OK);
    last_failure = ESP_ERR_NVS_NOT_ENOUGH_SPACE;
    assert(book_progress_set_last_path(path) == ESP_OK);
    value.byte_off = 840;
    assert(book_progress_save(path, &value) == ESP_OK);
    assert(book_progress_load(path, 900, &restored) && restored.byte_off == 840);
    // Keep a verified prior snapshot while simulating interrupted publication.
    assert(remove("progress/progress.bin.old") == 0);
    assert(rename("progress/progress.bin", "progress/progress.bin.old") == 0);
    FILE *broken = fopen("progress/progress.bin", "wb");
    assert(broken);
    fputs("broken", broken);
    fclose(broken);
    assert(book_progress_load(path, 900, &restored) && restored.byte_off == 840);
    assert(book_progress_save(path, &value) == ESP_OK);
    assert(rename("progress", "removed-card") == 0);
    FILE *block = fopen("progress", "wb");
    assert(block);
    fclose(block);
    assert(book_progress_save(path, &value) != ESP_OK);
    assert(remove("progress") == 0 && rename("removed-card", "progress") == 0);
    puts("PASS NVS full, fallback, checksum recovery, interrupted rename and unavailable storage");
    return 0;
}
