#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 kiikoread
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""检查 SVG 线段坐标与固件成品掩模。/ Check SVG line coordinates and firmware masks."""
import importlib.util
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("icon_generator", ROOT / "tools/gen_ui_icons.py")
icons = importlib.util.module_from_spec(spec)
spec.loader.exec_module(icons)

# 原 SVG 的 x1/y1 等含数字属性必须保留，不能退化成左上角的点。
# Preserve numeric attribute names such as x1/y1 instead of drawing a corner dot.
attrs = icons._attributes('<line x1="12" x2="12" y1="7" y2="13" />')
assert attrs == {"x1": "12", "x2": "12", "y1": "7", "y2": "13"}, attrs
assert icons.element_subpaths("line", attrs) == [([(12.0, 7.0), (12.0, 13.0)], False)]
empty = icons.pack_4bpp(icons.render_icon("bookmark"))
saved = icons.pack_4bpp(icons.render_icon("animate-bookmark-plus"))

def alpha(mask, x, y):
    value = mask[(y * icons.MASK_SIZE + x) // 2]
    return (value >> (0 if x & 1 else 4)) & 15

for x, y in [(36, 30), (30, 30), (42, 30), (36, 24), (36, 36)]:
    assert alpha(empty, x, y) == 0, (x, y)
    assert alpha(saved, x, y) >= 14, (x, y)
assert alpha(saved, 0, 0) == 0
assert alpha(saved, 36, 9) >= 14  # 顶边保留。/ Retain the outer top edge.

# 检查实际编译头文件，防止只改 SVG 却忘记更新成品。
# Inspect the compiled header too, catching stale generated artwork.
header = (ROOT / "main/assets/ui_icons.h").read_text()
blocks = re.findall(r'\{([^{}]+)\}', header[header.index('static const uint8_t ui_icon_masks'):])
assert len(blocks) == len(icons.ICONS)
for name, block in zip(icons.ICONS, blocks):
    actual = bytes.fromhex(''.join(re.findall(r'0x([0-9a-fA-F]{2})', block)))
    assert actual == bytes(icons.pack_4bpp(icons.render_icon(name))), name
print(f"PASS: SVG numeric coordinates, visible bookmark plus, no corner dot, {len(blocks)} generated masks")
