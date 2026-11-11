/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stddef.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include "online_font_names.h"

// 文件名只决定同目录内的字体族；最长样式后缀优先。/ Match sibling families within one directory, longest style suffix first.
static inline unsigned font_family_identity(const char *path, char *name, size_t cap) {
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    if (!cap) return 0;
    size_t bytes=strlen(base);
    if (bytes>=cap) bytes=cap-1;
    memcpy(name,base,bytes);
    name[bytes]=0;
    char *dot = strrchr(name, '.');
    if (dot) *dot = 0;
    const char *catalog_name = online_font_catalog_name(name);
    if (catalog_name) {
        snprintf(name, cap, "%s", catalog_name);
        return 0;
    }
    static const struct { const char *suffix; unsigned style; } suffixes[] = {
        {"SemiBoldItalic",3},{"BoldItalic",3},{"BoldOblique",3},{"Bold-Italic",3},{"Bold_Italic",3},{"Bold Italic",3},
        {"Regular",0},{"Roman",0},{"Book",0},
        {"SemiBold",1},{"Bold",1},{"Italic",2},{"Oblique",2},
    };
    for (size_t i=0;i<sizeof(suffixes)/sizeof(suffixes[0]);++i) {
        size_t n=strlen(name), tail=strlen(suffixes[i].suffix);
        if (n<=tail || strcasecmp(name+n-tail,suffixes[i].suffix)) continue;
        name[n-tail]=0;
        n-=tail;
        while (n && (name[n-1]=='-' || name[n-1]=='_' || name[n-1]==' ')) name[--n]=0;
        return suffixes[i].style;
    }
    return 0;
}

static inline int font_family_same_directory(const char *a, const char *b) {
    const char *aa=strrchr(a,'/'), *bb=strrchr(b,'/');
    size_t an=aa?(size_t)(aa-a):0, bn=bb?(size_t)(bb-b):0;
    return an==bn && !strncmp(a,b,an);
}
