/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define BOOK_ANNOTATION_MAX 32
#define BOOK_NOTE_CAP 128
#define BOOK_QUOTE_CAP 256
typedef struct { uint32_t start,end; char note[BOOK_NOTE_CAP],quote[BOOK_QUOTE_CAP]; } book_annotation_t;
typedef struct {
    uint64_t text_hash, path_hash;
    uint32_t chapter, length, count, sequence;
    book_annotation_t items[BOOK_ANNOTATION_MAX];
} book_annotations_t;
/* Returns false on corrupt/unreadable data; missing file is an empty valid store.
 * Content identity is part of the filename, preserving older book revisions. */
bool book_annotations_load(book_annotations_t *a,const char *root,const char *path,
                            uint32_t chapter,const char *text,size_t length);
bool book_annotations_save(book_annotations_t *a,const char *root);
bool book_annotations_put(book_annotations_t *a,const char *text,size_t start,size_t end,const char *note);
