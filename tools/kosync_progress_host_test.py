#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""中文：从真实源码抽出 KOSync 进度解析与方向判定，覆盖两种指针格式与亚百分比差距。
English: Extract KOSync's real pointer parser and direction rule; cover both pointer shapes and sub-percent gaps.
"""
from pathlib import Path
import subprocess, tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'main/book/kosync_client.c'

PREFIX = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

static int failures;
#define CHECK(cond, ...) do { if (!(cond)) { printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); ++failures; } } while (0)
'''

SUFFIX = r'''
int main(void) {
    uint16_t chap; uint32_t off; bool exact;

    puts("-- pointer parsing --");
    kosync_parse_pointer("/body/DocFragment[3]/body/p[1].1024", &chap, &off, &exact);
    CHECK(chap == 2 && off == 1024 && exact,
          "dpReader pointer: chap=%u off=%u exact=%d", chap, off, exact);

    kosync_parse_pointer("/body/DocFragment[12]/body/p[1].0", &chap, &off, &exact);
    CHECK(chap == 11 && off == 0 && exact,
          "zero offset stays exact: chap=%u off=%u exact=%d", chap, off, exact);

    // crengine 串只提供章节号；末尾字符序号绝不能当章内字节偏移。
    // A crengine pointer yields the chapter only; its trailing index is never a byte offset.
    kosync_parse_pointer("/body/DocFragment[3]/body/div/p[7]/text().42", &chap, &off, &exact);
    CHECK(chap == 2, "crengine chapter: chap=%u", chap);
    CHECK(!exact && off == 0,
          "crengine trailing index must not become an offset: off=%u exact=%d", off, exact);

    kosync_parse_pointer("/body/DocFragment[7]/body/div/div[2]/p[3]/span/text().55",
                         &chap, &off, &exact);
    CHECK(chap == 6 && !exact && off == 0,
          "nested crengine pointer: chap=%u off=%u exact=%d", chap, off, exact);

    kosync_parse_pointer("", &chap, &off, &exact);
    CHECK(chap == 0 && off == 0 && !exact, "empty pointer is inert");

    kosync_parse_pointer("garbage", &chap, &off, &exact);
    CHECK(chap == 0 && off == 0 && !exact, "unrecognised pointer is inert");

    kosync_parse_pointer("/body/DocFragment[0]/body/p[1].5", &chap, &off, &exact);
    CHECK(chap == 0 && !exact, "fragment 0 is rejected");

    puts("-- sync direction --");
    // 42.4% 与 42.6% 曾量化成同一字节而被判相等，远端永远拉不下来。
    // 42.4% vs 42.6% used to quantise to the same byte, so the remote never pulled.
    CHECK(kosync_remote_wins(0.424, 0.426, false, 3, 100, 3, 0),
          "sub-percent remote lead must still pull");
    CHECK(!kosync_remote_wins(0.426, 0.424, false, 3, 100, 3, 0),
          "local lead must push");

    CHECK(!kosync_remote_wins(0.5, 0.5, false, 3, 100, 3, 0),
          "equal ratio without offset must not pull");
    CHECK(kosync_remote_wins(0.5, 0.5, true, 3, 100, 4, 0),
          "later chapter wins at equal ratio");
    CHECK(kosync_remote_wins(0.5, 0.5, true, 3, 100, 3, 200),
          "later offset wins at equal ratio");
    CHECK(!kosync_remote_wins(0.5, 0.5, true, 3, 200, 3, 100),
          "earlier offset must not pull");

    CHECK(!kosync_remote_wins(1.0, 1.0, true, 9, 500, 9, 500),
          "both finished must not pull");
    CHECK(!kosync_remote_wins(0.3, 0.0, false, 2, 0, 0, 0),
          "empty remote must not pull");
    CHECK(kosync_remote_wins(0.0, 0.05, false, 0, 0, 0, 0),
          "remote with real progress pulls over an unread local");

    if (failures) { printf("\n%d check(s) failed\n", failures); return 1; }
    puts("\nPASS: pointer shapes stay separate and sub-percent leads still pull");
    return 0;
}
'''


def function(name: str) -> str:
    """Extract one complete function body from the maintained source."""
    import re
    text = SOURCE.read_text()
    scan = re.sub(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"',
                  lambda m: ' ' * len(m[0]), text)
    start = re.search(r'^(?:static )?(?:[A-Za-z_]\w*(?:\s+|\s*\*\s*))+' + name +
                      r'\([^;{}]*?\)\s*\{', scan, re.M)
    assert start, f'function not found: {name}'
    at, depth = start.end(), 1
    while depth:
        depth += (scan[at] == '{') - (scan[at] == '}')
        at += 1
    return text[start.start():at]


unit = PREFIX + function('kosync_parse_pointer') + '\n' + \
    function('kosync_remote_wins') + '\n' + SUFFIX

with tempfile.TemporaryDirectory() as folder:
    work = Path(folder)
    src = work / 'test.c'
    src.write_text(unit)
    binary = work / 'test'
    subprocess.run(['cc', '-std=gnu11', '-Wall', '-Wextra', '-Werror',
                    str(src), '-lm', '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
