#!/usr/bin/env python3
"""Check S31 collage thumbnail reuse and the SD checksum fallback."""
from pathlib import Path
import subprocess
import tempfile

from home_cover_cache_host_test import function

ROOT = Path(__file__).resolve().parents[1]
COLLAGE = ROOT / 'firmware/full/library/book_lock_collage.c'

unit = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "metalio_cover_cache.h"
#define DPREADER_BOARD_METALIO47 1
#define LOCK_IMAGE_PIXELS (128u * 1024u)
#define LOCK_CACHE_MAGIC UINT32_C(0x4c434431)
#define LOCK_CACHE_VERSION 1u
#define LOCK_CACHE_DIR "."
typedef struct {
    char path[64];
    uint32_t size;
    int64_t modified;
    uint64_t key;
    unsigned source_w, source_h, width, height;
} lock_book_t;
typedef struct {
    uint32_t magic, version;
    uint64_t key, checksum;
    uint32_t width, height, source_w, source_h, rotation, bytes;
} lock_cache_t;

void *heap_caps_malloc(size_t bytes, unsigned caps) { (void)caps; return malloc(bytes); }
void *heap_caps_calloc(size_t n, size_t bytes, unsigned caps) { (void)caps; return calloc(n, bytes); }
size_t heap_caps_get_free_size(unsigned caps) { (void)caps; return 8u * 1024u * 1024u; }
size_t heap_caps_get_largest_free_block(unsigned caps) { return heap_caps_get_free_size(caps); }
static void *lock_alloc(size_t bytes) { return calloc(1, bytes); }
'''
for name in (
    'hash_bytes', 'hash_text', 'cache_header', 'image_cache_path', 'cover_header_valid',
    'cover_dimensions_set', 'cover_ram_key', 'cover_ram_dimensions', 'cover_ram_store',
    'cover_dimensions_read', 'cover_read',
):
    unit += function(name, COLLAGE) + '\n'

unit += r'''
static void write_cover(const char *path, const lock_cache_t *header, const uint8_t *pixels) {
    FILE *file = fopen(path, "wb");
    assert(file && fwrite(header, sizeof(*header), 1, file) == 1);
    assert(fwrite(pixels, 1, header->bytes, file) == header->bytes && !fclose(file));
}

int main(void) {
    const uint8_t pixels[] = { 10, 20, 30, 40, 50, 60 };
    lock_book_t book = {.path="/books/example.epub",.size=100,.modified=7,.key=123};
    lock_cache_t header = {.magic=LOCK_CACHE_MAGIC,.version=LOCK_CACHE_VERSION,.key=book.key,
        .width=2,.height=3,.source_w=200,.source_h=300,.bytes=sizeof(pixels),
        .checksum=hash_bytes(UINT64_C(14695981039346656037),pixels,sizeof(pixels))};
    char path[112];
    image_cache_path(&book, path);
    write_cover(path, &header, pixels);
    assert(cover_dimensions_read(&book) && book.source_w == 200 && book.source_h == 300);
    assert(metalio_cover_cache_stats().entries == 0);
    uint8_t *out = cover_read(&book);
    assert(out && !memcmp(out, pixels, sizeof(pixels)));
    free(out);
    assert(!remove(path));

    book.width = book.height = book.source_w = book.source_h = 0;
    assert(cover_dimensions_read(&book) && book.width == 2 && book.height == 3);
    assert(book.source_w == 200 && book.source_h == 300);
    out = cover_read(&book);
    assert(out && !memcmp(out, pixels, sizeof(pixels)));
    free(out);
    ++book.key;
    assert(!cover_dimensions_read(&book) && !cover_read(&book));
    --book.key;
    metalio_cover_cache_clear();
    assert(!cover_read(&book));
    ++header.checksum;
    write_cover(path, &header, pixels);
    assert(cover_dimensions_read(&book) && !cover_read(&book));
    assert(metalio_cover_cache_stats().entries == 0);
    puts("PASS: collage dimensions and pixels reused without SD, changed keys miss, SD checksum still enforced");
}
'''

with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    source = work / 'test.c'
    source.write_text(unit)
    executable = work / 'test'
    subprocess.run(['cc', '-std=gnu11', '-Wall', '-Wextra', '-Werror', '-pthread',
                    '-fsanitize=address,undefined', '-DDPREADER_BOARD_METALIO47=1',
                    '-I' + str(ROOT / 'tools/metalio_cache_host_stubs'),
                    '-I' + str(ROOT / 'main/book'), str(source),
                    str(ROOT / 'main/book/metalio_cover_cache.c'), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], cwd=work, check=True)
