#!/usr/bin/env python3
"""Generate main/assets/builtin.ttf (Noto Sans Medium + Vietnamese + UI Symbols) for dpReader."""

from __future__ import annotations

import argparse
import copy
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CHARSET = ROOT / "main/font/charset.txt"
OUT = ROOT / "main/assets/builtin.ttf"
SCAN_DIRS = [
    ROOT / "main/ui",
    ROOT / "main/app",
    ROOT / "main/apps",
    ROOT / "main/book",
    ROOT / "main/factory",
    ROOT / "boards/metalio47/src",
    ROOT / "main/app_main.c",
    ROOT / "main/display.c",
    ROOT / "main/sleep.c",
    ROOT / "main/settings.c",
    ROOT / "main/ota_update.c",
    ROOT / "main/ota_online.c",
    ROOT / "components/read_pico",
    ROOT / "components/read_pico_pmu",
    ROOT / "components/read_pico_transfer",
    ROOT / "components/ble_page_turner",
]
MAX_BYTES = 700 * 1024
BUILTIN_WEIGHT = 500
STRING_RE = re.compile(r'"(?:\\.|[^"\\])*"')
COMMENT_RE = re.compile(r"//.*?$|/\*.*?\*/", re.DOTALL | re.MULTILINE)


def ascii_printable() -> str:
    return "".join(chr(i) for i in range(0x20, 0x7F))


def load_preset(path: Path) -> str:
    chars: list[str] = []
    if not path.is_file():
        return ""
    for line in path.read_text(encoding="utf-8").splitlines():
        s = line.strip()
        if not s or s.startswith("#"):
            continue
        chars.append(s)
    return "".join(chars)


def unescape(literal: str) -> str:
    body = literal[1:-1]
    return (
        body.replace("\\\\", "\0")
        .replace("\\n", "\n")
        .replace("\\t", "\t")
        .replace('\\"', '"')
        .replace("\0", "\\")
    )


def scan_sources() -> str:
    texts: list[str] = []
    reading_md = ROOT / "main/assets/reading.md"
    if reading_md.is_file():
        texts.append(reading_md.read_text(encoding="utf-8", errors="ignore"))
    files: list[Path] = []
    for item in SCAN_DIRS:
        if item.is_file():
            files.append(item)
        elif item.is_dir():
            files.extend(item.rglob("*.c"))
            files.extend(item.rglob("*.h"))
    for path in files:
        data = COMMENT_RE.sub("", path.read_text(encoding="utf-8", errors="ignore"))
        for match in STRING_RE.finditer(data):
            texts.append(unescape(match.group(0)))
    return "".join(texts)


def unique_text(*parts: str) -> str:
    seen: set[str] = set()
    out: list[str] = []
    for part in parts:
        for ch in part:
            if ch in seen or ch in "\n\r\t":
                continue
            seen.add(ch)
            out.append(ch)
    return "".join(sorted(out, key=lambda c: ord(c)))


def copy_glyphs_from(dst_font, src_path: Path, cps: list[int]) -> None:
    if not src_path.is_file():
        return
    from fontTools.ttLib import TTFont

    src = TTFont(src_path)
    src_cmap = src.getBestCmap()
    dst_cmap = dst_font.getBestCmap()
    glyph_order = list(dst_font.getGlyphOrder())
    for cp in cps:
        if cp in dst_cmap or cp not in src_cmap:
            continue
        src_gname = src_cmap[cp]
        g = copy.deepcopy(src["glyf"][src_gname])
        new_gname = f"uni{cp:04X}"
        while new_gname in glyph_order:
            new_gname += "_"
        glyph_order.append(new_gname)
        dst_font["glyf"][new_gname] = g
        dst_font["hmtx"][new_gname] = src["hmtx"][src_gname]
        for table in dst_font["cmap"].tables:
            if table.isUnicode():
                table.cmap[cp] = new_gname
    dst_font.setGlyphOrder(glyph_order)
    src.close()


def default_src() -> Path:
    for path in (
        ROOT / "main/assets/Bitter-Medium.ttf",
        Path("/usr/share/fonts/truetype/noto/NotoSans-Medium.ttf"),
        ROOT / "sdcard/fonts/Hei.ttf",
        ROOT / "flash/fonts/Hei.ttf",
    ):
        if path.is_file():
            return path
    return ROOT / "main/assets/Bitter-Medium.ttf"


def build_vietnamese_builtin(src: Path, text: str, dest: Path) -> int:
    from fontTools.subset import Subsetter, Options
    from fontTools.ttLib import TTFont

    existing_cjk_backup = Path("/tmp/orig_cjk_backup.ttf")
    if dest.is_file() and not existing_cjk_backup.is_file():
        existing_cjk_backup.write_bytes(dest.read_bytes())

    f_base = TTFont(src)
    base_cmap = set(f_base.getBestCmap().keys())
    keep_cps = set(
        cp
        for cp in base_cmap
        if (
            0x0020 <= cp <= 0x024F
            or 0x02B0 <= cp <= 0x036F
            or 0x1E00 <= cp <= 0x1EFF
            or 0x2000 <= cp <= 0x2BFF
        )
    )
    vietnamese_all = (
        "aàảãáạăằẳẵắặâầẩẫấậ"
        "bcdđeèẻẽéẹêềểễếệ"
        "ghikìỉĩíị"
        "lmnoòỏõóọôồổỗốộơờởỡớợ"
        "pqrstuùủũúụưừửữứự"
        "vxyỳỷỹýỵ"
        "AÀẢÃÁẠĂẰẲẴẮẶÂẦẨẪẤẬ"
        "BCDĐEÈẺẼÉẸÊỀỂỄẾỆ"
        "GHIKÌỈĨÍỊ"
        "LMNOÒỎÕÓỌÔỒỔỖỐỘƠỜỞỠỚỢ"
        "PQRSTUÙỦŨÚỤƯỪỬỮỨỰ"
        "VXYỲỶỸÝỴ"
        "←↑→↓⇧−√∞∩≈≠≤≥⌫■□▲▼○●★☆✓✔✕✖"
    )
    for ch in vietnamese_all:
        if ord(ch) in base_cmap:
            keep_cps.add(ord(ch))
    for ch in text:
        if ord(ch) in base_cmap:
            keep_cps.add(ord(ch))

    opts = Options()
    opts.layout_features = ["*"]
    opts.notdef_outline = True
    opts.recommended_glyphs = True
    opts.ignore_missing_glyphs = True
    sub = Subsetter(options=opts)
    sub.populate(unicodes=sorted(keep_cps))
    sub.subset(f_base)

    # Adjust vertical metrics to prevent clipping Vietnamese double-diacritic glyphs (yMax ~1064)
    if "hhea" in f_base:
        f_base["hhea"].ascent = 1120
        f_base["hhea"].descent = -300
    if "OS/2" in f_base:
        f_base["OS/2"].sTypoAscender = 1120
        f_base["OS/2"].sTypoDescender = -300
        f_base["OS/2"].usWinAscent = 1120
        f_base["OS/2"].usWinDescent = 300

    # Inject UI math/arrow/dingbat symbols and internal CJK chapter/width tokens (all UPM=1000)
    needed_cps = [ord(ch) for ch in text] + [
        ord(ch) for ch in "←↑→↓⇧−√∞∩≈≠≤≥⌫■□▲▼○●★☆✓✔✕✖一第章：、零二三四五六七八九十百千万两节卷回集部篇"
    ]
    copy_glyphs_from(f_base, Path("/usr/share/fonts/truetype/noto/NotoSansMath-Regular.ttf"), needed_cps)
    copy_glyphs_from(f_base, Path("/usr/share/fonts/truetype/noto/NotoSansSymbols2-Regular.ttf"), needed_cps)
    if existing_cjk_backup.is_file():
        copy_glyphs_from(f_base, existing_cjk_backup, needed_cps)

    for name_id, value in (
        (1, "Bitter"),
        (2, "Medium"),
        (4, "Bitter Medium"),
        (6, "Bitter-Medium"),
    ):
        f_base["name"].setName(value, name_id, 3, 1, 0x409)

    dest.parent.mkdir(parents=True, exist_ok=True)
    f_base.save(dest)
    f_base.close()
    return dest.stat().st_size


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--src", type=Path, default=default_src())
    parser.add_argument("--out", type=Path, default=OUT)
    args = parser.parse_args()
    if not args.src.is_file():
        print(f"missing source font: {args.src}", file=sys.stderr)
        return 1

    text = unique_text(ascii_printable(), load_preset(CHARSET), scan_sources())
    size = build_vietnamese_builtin(args.src, text, args.out)
    print(f"wrote {args.out} ({size} bytes, Medium wght={BUILTIN_WEIGHT})")
    if size > MAX_BYTES:
        print("subset still too large", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
