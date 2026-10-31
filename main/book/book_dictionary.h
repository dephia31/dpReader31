/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "book_dict_file.h"
#define BOOK_DICT_TEXT 8192
#define BOOK_DICT_PATH 512
/* One bounded, cancellable SD lookup. 32-bit StarDict plain/gzip/dictzip textual fields. */
typedef struct {
    book_dict_file_t *index, *data;
    FILE *syn;
    char base[BOOK_DICT_PATH], query[256], text[BOOK_DICT_TEXT], name[128];
    uint32_t words, scanned, target, data_size;
    uint32_t definition_length;
    size_t definition_bytes;
    char types[9];
    bool truncated;
    int state; /* 0 closed, 1 index, 2 synonyms, 3 alias index, 4 result, 5 reading definition, -1 error, -2 missing */
} book_dictionary_t;
bool book_dictionary_open(book_dictionary_t *d, const char *ifo, const char *query);
int book_dictionary_step(book_dictionary_t *d); /* <=4096 records/call; ESP yields after ~8ms between record groups */
void book_dictionary_close(book_dictionary_t *d);
