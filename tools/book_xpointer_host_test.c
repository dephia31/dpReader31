/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "book_xpointer.h"
#include "html_text.h"
static void roundtrip(const char* html, const char* needle, const char* expected) {
    size_t source = (size_t)(strstr(html, needle) - html), back = SIZE_MAX;
    char pointer[BOOK_XPOINTER_CAP];
    assert(
        book_xpointer_export(html, strlen(html), 899, source, pointer, sizeof(pointer)) == ESP_OK);
    if (strcmp(pointer, expected)) {
        fprintf(stderr, "expected %s got %s\n", expected, pointer);
        abort();
    }
    assert(
        book_xpointer_import(html, strlen(html), 899, pointer, &back) == ESP_OK && back == source);
    html_source_map_t map = { .source_target = source, .text_target = SIZE_MAX };
    html_text_t text = { 0 };
    assert(html_to_blocks_mapped(html, strlen(html), &map, &text) == ESP_OK && map.source_found);
    size_t offset = map.text_result;
    html_text_free(&text);
    map = (html_source_map_t){ .source_target = SIZE_MAX, .text_target = offset };
    assert(
        html_to_blocks_mapped(html, strlen(html), &map, &text) == ESP_OK && map.text_found &&
        map.source_result == source);
    html_text_free(&text);
}
int main(void) {
    roundtrip(
        "<html><body><div><p>Xin chào Việt Nam</p></div></body></html>", "Việt",
        "/body/DocFragment[900]/body/div[1]/p[1]/text()[1].9");
    roundtrip(
        "<html><body><p>one</p><div>skip</div><p>a<span>b</span>cd</p></body></html>", "cd",
        "/body/DocFragment[900]/body/p[2]/text()[2].0");
    roundtrip(
        "<html><body><p>A &amp; &#x1F600; Việt</p></body></html>", "Việt",
        "/body/DocFragment[900]/body/p[1]/text()[1].6");
    roundtrip(
        "<html><body>\n <p>  a   Việt</p>\n</body></html>", "Việt",
        "/body/DocFragment[900]/body/p[1]/text()[1].2");
    roundtrip(
        "<?xml version='1.0'?><!DOCTYPE html [<!ENTITY x 'a>b'>]><html><body><!--ignore--><x:p "
        "xmlns:x='urn:x' a='>'>abc</x:p><p>x</p></body></html>",
        "abc", "/body/DocFragment[900]/body/p[1]/text()[1].0");
    roundtrip(
        "<html><body><p>A😀B🦊C</p></body></html>", "C",
        "/body/DocFragment[900]/body/p[1]/text()[1].4");
    const char* h = "<html><body><p>abc</p></body></html>";
    size_t source;
    uint16_t ch = 99;
    assert(!book_xpointer_chapter("/body/DocFragment[0]/body", &ch));
    assert(!book_xpointer_chapter("/body/DocFragment[9999999999999]/body", &ch));
    assert(
        book_xpointer_import(h, strlen(h), 0, "/body/DocFragment[900]/body/p/text().0", &source) !=
        ESP_OK);
    assert(
        book_xpointer_import(
            h, strlen(h), 899, "/body/DocFragment[900]/body/p/text().999", &source) != ESP_OK);
    assert(
        book_xpointer_import(
            h, strlen(h), 899, "/body/DocFragment[900]/body/p/text().1garbage", &source) != ESP_OK);
    assert(
        book_xpointer_import(
            "<body><p>abc", 12, 899, "/body/DocFragment[900]/body/p/text().1", &source) != ESP_OK);
    char small[8];
    assert(book_xpointer_export(h, strlen(h), 899, 15, small, sizeof(small)) != ESP_OK);
    const char* ambiguous = "<html><body><p>  a   Việt</p></body></html>";
    char pointer[BOOK_XPOINTER_CAP];
    size_t point = (size_t)(strstr(ambiguous, "Việt") - ambiguous);
    assert(
        book_xpointer_export_checked(
            ambiguous, strlen(ambiguous), 899, point, pointer, sizeof(pointer), true) ==
        ESP_ERR_NOT_SUPPORTED);
    assert(
        book_xpointer_export_element(
            ambiguous, strlen(ambiguous), 899, point, pointer, sizeof(pointer)) == ESP_OK);
    assert(!strcmp(pointer, "/body/DocFragment[900]/body/p[1]"));
    assert(book_xpointer_import(ambiguous, strlen(ambiguous), 899, pointer, &source) == ESP_OK);
    puts(
        "XPointer: Vietnamese Unicode code-point offsets, normalized spaces, entities, nested "
        "spans, "
        "sibling/text indexes, namespaces and fail-closed bounds PASS");
}
