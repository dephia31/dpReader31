/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct book_dict_file book_dict_file_t;
/* Read-only bounded file view: plain, streamed gzip or dictzip RA chunks.
 * Compressed expanded size is capped at 512 MiB. No SD cache/extraction writes. */
book_dict_file_t *book_dict_file_open(const char *path,bool compressed,bool random);
void book_dict_file_close(book_dict_file_t *file);
uint32_t book_dict_file_size(const book_dict_file_t *file);
int book_dict_file_getc(book_dict_file_t *file);
bool book_dict_file_read(book_dict_file_t *file,void *out,size_t n);
bool book_dict_file_rewind(book_dict_file_t *file);
bool book_dict_file_end(book_dict_file_t *file);
/* Range read is cancellable between steps: at most one 64KiB dictzip chunk or
 * 32KiB streamed output per step. Output is borrowed until completed/closed. */
bool book_dict_file_begin(book_dict_file_t *file,uint32_t offset,void *out,size_t n);
int book_dict_file_step(book_dict_file_t *file); /* -1 error,0 pending,1 complete */
