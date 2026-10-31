/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "book_annotations.h"
/* JSON transport below /dpReader_note/, with legacy sidecar import. KOReader resolves its own
 * XPointers using the supplied plugin; firmware never invents CRE positions. */
bool book_annotation_directory(const char *book,char *out,size_t cap);
bool book_annotation_sidecar(const char *book,char *out,size_t cap);
bool book_annotation_catalog(const char *book,size_t count,
                             bool (*title)(size_t,char*,size_t));
bool book_annotation_import(book_annotations_t *a,const char *book,const char *text,
                            unsigned *unresolved);
bool book_annotation_publish(const book_annotations_t *a,const char *book,const char *text);
