#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
dpReader Firmware Screenshot Renderer
Generates authentic 684x1216 4-bit E-ink screenshots for dpReader firmware
using real firmware assets: builtin.ttf and ui_icons.h 4bpp alpha masks.
"""

import os
import re
import sys
import math
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont, ImageFilter

REPO_ROOT = Path(__file__).resolve().parents[1]
FONT_PATH = str(REPO_ROOT / 'main/assets/builtin.ttf')
ICONS_PATH = str(REPO_ROOT / 'boards/metalio47/src/main/assets/ui_icons.h')
OUTPUT_DIR = REPO_ROOT / 'docs/screenshots'
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

# Screen Resolution: 684x1216 Portrait Metalio E-ink 4.7"
SCREEN_W = 684
SCREEN_H = 1216

# Grayscale palette (16 levels of E-ink gray)
INK_BLACK = 0x00
INK_DARK = 0x33
INK_MID = 0x77
INK_LIGHT = 0xBB
INK_BORDER = 0x99
INK_BG = 0xF5
INK_WHITE = 0xFF

# Load and parse firmware icons from ui_icons.h
ICON_MAP = {}
def load_firmware_icons():
    with open(ICONS_PATH, 'r', encoding='utf-8') as f:
        text = f.read()

    clean = re.sub(r'//.*', '', text)
    clean = re.sub(r'/\*.*?\*/', '', clean, flags=re.DOTALL)

    enum_part = re.search(r'typedef enum\s*\{(.*?)\}\s*ui_icon_t;', clean, re.DOTALL).group(1)
    names = [x.strip() for x in enum_part.split(',') if x.strip() and x.strip() != 'UI_ICON_COUNT']

    masks_match = re.search(r'ui_icon_masks\[UI_ICON_COUNT\]\[UI_ICON_MASK_BYTES\]\s*=\s*\{(.*)\};', clean, re.DOTALL)
    blocks = re.findall(r'\{([0-9a-fA-Fx\s,\n]+)\}', masks_match.group(1))

    for idx, (name, block) in enumerate(zip(names, blocks)):
        hex_bytes = [int(x.strip(), 16) for x in block.split(',') if x.strip()]
        pixels = []
        for b in hex_bytes:
            pixels.append((b >> 4) * 17)
            pixels.append((b & 15) * 17)

        img = Image.new('L', (72, 72), 0)
        img.putdata(pixels)
        ICON_MAP[name] = img

load_firmware_icons()

def get_font(size):
    return ImageFont.truetype(FONT_PATH, int(size))

def draw_round_rect(draw, box, radius, fill=None, outline=None, width=1):
    x0, y0, x1, y1 = box
    draw.rounded_rectangle([x0, y0, x1, y1], radius=radius, fill=fill, outline=outline, width=width)

def draw_fw_icon(base_img, icon_name, cx, cy, size_px, tint_gray=INK_BLACK):
    """Draws an authentic firmware icon centered at (cx, cy) resized to size_px."""
    if icon_name not in ICON_MAP:
        return
    mask = ICON_MAP[icon_name].resize((size_px, size_px), Image.Resampling.LANCZOS)
    color_img = Image.new('L', (size_px, size_px), tint_gray)
    x = cx - size_px // 2
    y = cy - size_px // 2
    base_img.paste(color_img, (x, y), mask)

def draw_battery(draw, x, y, w=38, h=18, pct=88, fill_color=INK_DARK):
    """Draws a clean battery indicator."""
    draw_round_rect(draw, [x, y, x + w - 4, y + h], 4, fill=None, outline=fill_color, width=2)
    # Terminal nipple
    draw.rectangle([x + w - 3, y + h // 4, x + w, y + 3 * h // 4], fill=fill_color)
    # Level bar
    inner_w = int((w - 10) * (pct / 100.0))
    if inner_w > 0:
        draw_round_rect(draw, [x + 3, y + 3, x + 3 + inner_w, y + h - 3], 2, fill=fill_color)

def draw_status_bar(img, draw, wifi_name="vreader-home", battery_pct=88, time_str="09:41"):
    f_stat = get_font(20)
    # Left: WiFi icon + network name
    draw_fw_icon(img, "UI_ICON_WIFI", 48, 28, 22, INK_DARK)
    draw.text((66, 17), wifi_name, fill=INK_DARK, font=f_stat)
    
    # Center: Clock
    bbox = f_stat.getbbox(time_str)
    cw = bbox[2] - bbox[0]
    draw.text(((SCREEN_W - cw) // 2, 17), time_str, fill=INK_BLACK, font=f_stat)
    
    # Right: Battery indicator + device name
    draw_battery(draw, SCREEN_W - 130, 19, 32, 16, battery_pct, INK_DARK)
    draw.text((SCREEN_W - 90, 17), f"{battery_pct}%", fill=INK_DARK, font=f_stat)
    
    # Hairline separator
    draw.line([(36, 52), (SCREEN_W - 36, 52)], fill=INK_BORDER, width=1)

def draw_bottom_nav(img, draw, active_tab=0):
    nav_y = 1120
    draw.line([(0, nav_y), (SCREEN_W, nav_y)], fill=INK_BORDER, width=2)
    draw.rectangle([0, nav_y + 1, SCREEN_W, SCREEN_H], fill=INK_WHITE)
    
    tabs = [
        ("UI_ICON_HOUSE", "Trang chủ"),
        ("UI_ICON_LIBRARY_BIG", "Kệ sách"),
        ("UI_ICON_FOLDER", "Tệp tin"),
        ("UI_ICON_SLIDERS_HORIZONTAL", "Cài đặt")
    ]
    tab_w = SCREEN_W // 4
    for i, (icon_name, label) in enumerate(tabs):
        cx = i * tab_w + tab_w // 2
        is_active = (i == active_tab)
        if is_active:
            # Active pill
            draw_round_rect(draw, [cx - 64, nav_y + 10, cx + 64, nav_y + 86], 20, fill=0xE2, outline=0x22, width=2)
            color = INK_BLACK
            f_lbl = get_font(20)
        else:
            color = INK_MID
            f_lbl = get_font(19)
            
        draw_fw_icon(img, icon_name, cx, nav_y + 35, 32, color)
        
        bbox_l = f_lbl.getbbox(label)
        lw = bbox_l[2] - bbox_l[0]
        draw.text((cx - lw // 2, nav_y + 57), label, fill=color, font=f_lbl)

# ==============================================================================
# SCREEN 1: Home Dashboard
# ==============================================================================
def render_01_home_dashboard():
    img = Image.new('L', (SCREEN_W, SCREEN_H), INK_WHITE)
    draw = ImageDraw.Draw(img)
    draw_status_bar(img, draw)
    
    # Header greeting
    draw.text((36, 75), "Xin chào, dephia31", fill=INK_BLACK, font=get_font(42))
    draw.text((36, 128), "Thứ Hai, 26 tháng 10 · Giờ Việt Nam (GMT+7)", fill=INK_MID, font=get_font(21))
    
    # Card 1: Continue Reading Card
    c1_y = 168
    draw_round_rect(draw, [36, c1_y, SCREEN_W - 36, c1_y + 240], 24, fill=0xF8, outline=0x40, width=2)
    # Badge
    draw_round_rect(draw, [56, c1_y + 20, 160, c1_y + 50], 12, fill=0x22)
    draw.text((70, c1_y + 24), "ĐANG ĐỌC", fill=0xFF, font=get_font(18))
    
    draw.text((56, c1_y + 62), "Truyện Kiều", fill=INK_BLACK, font=get_font(34))
    draw.text((56, c1_y + 106), "Nguyễn Du · Thơ lục bát kinh điển", fill=INK_DARK, font=get_font(22))
    
    # Progress bar
    draw_round_rect(draw, [56, c1_y + 148, SCREEN_W - 170, c1_y + 160], 6, fill=0xDD)
    prog_w = int((SCREEN_W - 170 - 56) * 0.42)
    draw_round_rect(draw, [56, c1_y + 148, 56 + prog_w, c1_y + 160], 6, fill=0x22)
    
    draw.text((56, c1_y + 172), "Đã đọc 42% · Chương 3 / 15 (Trang 42)", fill=INK_DARK, font=get_font(20))
    draw.text((56, c1_y + 200), "Vừa đọc hôm nay · 09:15 · KOSync đã đồng bộ", fill=INK_MID, font=get_font(18))
    
    # Reading button on right
    draw_round_rect(draw, [SCREEN_W - 150, c1_y + 120, SCREEN_W - 56, c1_y + 180], 18, fill=0x22)
    draw.text((SCREEN_W - 138, c1_y + 137), "Đọc tiếp ›", fill=0xFF, font=get_font(22))
    
    # Card 2: Clock & Reading tracker widget
    c2_y = 430
    draw_round_rect(draw, [36, c2_y, SCREEN_W - 36, c2_y + 320], 24, fill=0xF8, outline=0x70, width=2)
    
    # Clock circle on left
    clk_cx = 140
    clk_cy = c2_y + 160
    clk_r = 90
    draw.ellipse([clk_cx - clk_r, clk_cy - clk_r, clk_cx + clk_r, clk_cy + clk_r], outline=0x33, width=3)
    for hour in range(12):
        rad = hour * math.pi / 6
        x1 = clk_cx + int((clk_r - 12) * math.sin(rad))
        y1 = clk_cy - int((clk_r - 12) * math.cos(rad))
        x2 = clk_cx + int(clk_r * math.sin(rad))
        y2 = clk_cy - int(clk_r * math.cos(rad))
        draw.line([(x1, y1), (x2, y2)], fill=0x33, width=2 if hour % 3 == 0 else 1)
    hr_rad = (9 + 41/60.0) * math.pi / 6
    draw.line([(clk_cx, clk_cy), (clk_cx + int(48 * math.sin(hr_rad)), clk_cy - int(48 * math.cos(hr_rad)))], fill=0x11, width=5)
    min_rad = 41 * math.pi / 30
    draw.line([(clk_cx, clk_cy), (clk_cx + int(68 * math.sin(min_rad)), clk_cy - int(68 * math.cos(min_rad)))], fill=0x11, width=3)
    draw.ellipse([clk_cx - 6, clk_cy - 6, clk_cx + 6, clk_cy + 6], fill=0x11)
    
    # Right side: Reading streak & calendar stats
    draw.text((260, c2_y + 24), "Thống kê đọc sách (Hà Nội GMT+7)", fill=INK_BLACK, font=get_font(24))
    draw.text((260, c2_y + 60), "Hôm nay: 45 phút · Mục tiêu: 30p (150%)", fill=INK_DARK, font=get_font(19))
    draw.text((260, c2_y + 90), "Tuần này: 5 giờ 20 phút · Chuỗi: 14 ngày", fill=INK_DARK, font=get_font(19))
    
    # Mini Bar chart for 7 days
    days = ["T2", "T3", "T4", "T5", "T6", "T7", "CN"]
    hours = [45, 60, 30, 50, 75, 90, 45]
    chart_x = 260
    chart_y = c2_y + 240
    for idx, (d, val) in enumerate(zip(days, hours)):
        bx = chart_x + idx * 52
        bh = int(val * 1.1)
        draw_round_rect(draw, [bx, chart_y - bh, bx + 36, chart_y], 6, fill=0x33 if idx == 6 else 0x88)
        draw.text((bx + 8, chart_y + 8), d, fill=INK_DARK, font=get_font(16))
        
    # Card 3: Quick Bookshelf overview
    c3_y = 770
    draw_round_rect(draw, [36, c3_y, SCREEN_W - 36, c3_y + 325], 24, fill=0xF8, outline=0x70, width=2)
    draw.text((56, c3_y + 20), "Tủ sách gần đây (Thẻ nhớ TF)", fill=INK_BLACK, font=get_font(24))
    draw.text((SCREEN_W - 160, c3_y + 22), "Xem tất cả ›", fill=INK_MID, font=get_font(20))
    
    books = [
        ("Số Đỏ", "Vũ Trọng", "18%"),
        ("Tắt Đèn", "Ngô Tất Tố", "Mới"),
        ("Dế Mèn", "Tô Hoài", "85%"),
        ("Vợ Nhặt", "Kim Lân", "Đã đọc")
    ]
    card_w = (SCREEN_W - 72 - 30) // 4
    for i, (bname, bauth, bpct) in enumerate(books):
        bx = 52 + i * (card_w + 10)
        by = c3_y + 68
        draw_round_rect(draw, [bx, by, bx + card_w, by + 180], 12, fill=0xEE, outline=0x44, width=2)
        draw.line([(bx + 8, by), (bx + 8, by + 180)], fill=0x99, width=2)
        draw.text((bx + 14, by + 20), bname, fill=INK_BLACK, font=get_font(20))
        draw.text((bx + 14, by + 115), bauth, fill=INK_MID, font=get_font(16))
        draw_round_rect(draw, [bx + 10, by + 145, bx + card_w - 10, by + 170], 6, fill=0x22 if "Đã" in bpct else 0x88)
        draw.text((bx + 16, by + 150), bpct, fill=0xFF, font=get_font(14))

    draw_bottom_nav(img, draw, active_tab=0)
    img.save(OUTPUT_DIR / "01_home_dashboard.png")
    print("Rendered 01_home_dashboard.png")

# ==============================================================================
# SCREEN 2: Bookshelf with Import Modal
# ==============================================================================
def render_02_bookshelf():
    img = Image.new('L', (SCREEN_W, SCREEN_H), INK_WHITE)
    draw = ImageDraw.Draw(img)
    draw_status_bar(img, draw)
    
    # Header: "Tủ sách"
    draw.text((36, 75), "Tủ sách", fill=INK_BLACK, font=get_font(48))
    draw_fw_icon(img, "UI_ICON_CHEVRON_DOWN", 230, 105, 26, INK_BLACK)
    
    # Header buttons
    draw_round_rect(draw, [SCREEN_W - 320, 75, SCREEN_W - 180, 130], 18, fill=0xF0, outline=0x44, width=2)
    draw.text((SCREEN_W - 305, 90), "Quản lý", fill=INK_BLACK, font=get_font(22))
    
    draw_round_rect(draw, [SCREEN_W - 165, 75, SCREEN_W - 36, 130], 18, fill=0x22)
    draw.text((SCREEN_W - 153, 90), "+ Nhập sách", fill=0xFF, font=get_font(22))
    
    draw.line([(36, 145), (SCREEN_W - 36, 145)], fill=INK_BORDER, width=1)
    
    # 3x3 Bookshelf Grid
    books_data = [
        ("Truyện Kiều", "Nguyễn Du", "42%"),
        ("Số Đỏ", "Vũ Trọng Phụng", "18%"),
        ("Tắt Đèn", "Ngô Tất Tố", "Mới"),
        ("Dế Mèn Phiêu Lưu Ký", "Tô Hoài", "85%"),
        ("Vợ Nhặt", "Kim Lân", "100%"),
        ("Chí Phèo", "Nam Cao", "Mới"),
        ("Lão Hạc", "Nam Cao", "Mới"),
        ("Đất Rừng Phương Nam", "Đoàn Giỏi", "12%"),
        ("Những Ngày Thơ Ấu", "Nguyên Hồng", "Mới")
    ]
    
    col_w = (SCREEN_W - 72 - 40) // 3
    row_h = 240
    for idx, (bname, bauth, bpct) in enumerate(books_data):
        c = idx % 3
        r = idx // 3
        bx = 36 + c * (col_w + 20)
        by = 165 + r * (row_h + 30)
        
        draw_round_rect(draw, [bx, by, bx + col_w, by + 210], 16, fill=0xF4, outline=0x33, width=2)
        draw.line([(bx + 12, by), (bx + 12, by + 210)], fill=0x99, width=3)
        
        f_title = get_font(22)
        words = bname.split(" ")
        if len(words) > 2:
            line1 = " ".join(words[:2])
            line2 = " ".join(words[2:])
            draw.text((bx + 20, by + 24), line1, fill=INK_BLACK, font=f_title)
            draw.text((bx + 20, by + 54), line2, fill=INK_BLACK, font=f_title)
        else:
            draw.text((bx + 20, by + 30), bname, fill=INK_BLACK, font=f_title)
            
        draw.text((bx + 20, by + 115), bauth, fill=INK_DARK, font=get_font(18))
        draw_round_rect(draw, [bx + 16, by + 165, bx + col_w - 16, by + 196], 8, fill=0x22 if "100" in bpct else 0x77)
        draw.text((bx + 24, by + 170), f"Đã đọc {bpct}" if "%" in bpct else bpct, fill=0xFF, font=get_font(16))
        
        shelf_y = by + 216
        draw.rectangle([24, shelf_y, SCREEN_W - 24, shelf_y + 12], fill=0x33)
        draw.rectangle([30, shelf_y + 12, SCREEN_W - 30, shelf_y + 18], fill=0x88)
        
    # Modal Dialog (Bottom sheet for "Nhập sách mới")
    sheet_y = 660
    overlay = Image.new('L', (SCREEN_W, SCREEN_H), 0)
    draw_ov = ImageDraw.Draw(overlay)
    draw_ov.rectangle([0, 0, SCREEN_W, sheet_y], fill=70)
    img = Image.blend(img, overlay, 0.40)
    draw = ImageDraw.Draw(img)
    
    draw_round_rect(draw, [0, sheet_y, SCREEN_W, SCREEN_H - 96], 32, fill=INK_WHITE, outline=0x22, width=3)
    draw_round_rect(draw, [SCREEN_W // 2 - 40, sheet_y + 16, SCREEN_W // 2 + 40, sheet_y + 22], 3, fill=0x66)
    
    draw.text((42, sheet_y + 36), "Nhập sách mới vào dpReader", fill=INK_BLACK, font=get_font(32))
    draw.text((42, sheet_y + 80), "Chọn phương thức nạp sách vào thẻ nhớ (/sdcard/books):", fill=INK_DARK, font=get_font(21))
    
    options = [
        ("UI_ICON_WIFI", "Truyền qua WiFi (Cùng mạng LAN)", "Mở trình duyệt truy cập http://dpreader.local hoặc IP máy"),
        ("UI_ICON_RADIO_TOWER", "Điểm phát sóng WiFi (Direct AP)", "Kết nối WiFi 'dpReader-AP' do máy phát để truyền nhanh"),
        ("UI_ICON_DOWNLOAD", "Thư viện OPDS (Calibre-Web)", "Duyệt danh mục sách & tải trực tiếp từ sach.vreader.cloud:8443"),
        ("UI_ICON_REFRESH_CW", "Quét lại thẻ nhớ microSD", "Tìm tệp sách mới chép thủ công qua đầu đọc thẻ hoặc USB")
    ]
    
    opt_y = sheet_y + 120
    for idx, (icon, title, sub) in enumerate(options):
        oy = opt_y + idx * 82
        is_highlight = (idx == 2)
        draw_round_rect(draw, [42, oy, SCREEN_W - 42, oy + 72], 16, fill=0xF0 if not is_highlight else 0xE0, outline=0x22 if is_highlight else 0x88, width=2 if is_highlight else 1)
        draw_fw_icon(img, icon, 68, oy + 36, 32, INK_BLACK)
        draw.text((96, oy + 12), title, fill=INK_BLACK, font=get_font(22))
        draw.text((96, oy + 42), sub, fill=INK_DARK, font=get_font(16))
        draw_fw_icon(img, "UI_ICON_CHEVRON_RIGHT", SCREEN_W - 70, oy + 36, 24, INK_DARK)
        
    draw_bottom_nav(img, draw, active_tab=1)
    img.save(OUTPUT_DIR / "02_bookshelf.png")
    print("Rendered 02_bookshelf.png")

# ==============================================================================
# SCREEN 3: Reader Screen with KOSync integration & Vietnamese Typography
# ==============================================================================
def render_03_reader_kosync():
    img = Image.new('L', (SCREEN_W, SCREEN_H), INK_WHITE)
    draw = ImageDraw.Draw(img)
    draw_status_bar(img, draw)
    
    # Reader Header
    header_y = 62
    draw_fw_icon(img, "UI_ICON_CHEVRON_LEFT", 50, header_y + 18, 30, INK_BLACK)
    draw.text((75, header_y + 5), "Truyện Kiều — Nguyễn Du", fill=INK_BLACK, font=get_font(25))
    draw_fw_icon(img, "UI_ICON_BOOKMARK_CHECK", SCREEN_W - 55, header_y + 18, 30, INK_BLACK)
    draw.line([(36, header_y + 44), (SCREEN_W - 36, header_y + 44)], fill=INK_BORDER, width=1)
    
    # Vietnamese Poetry & Novel layout
    content_y = 125
    draw.text((SCREEN_W // 2 - 160, content_y), "ĐOẠN TRƯỜNG TÂN THANH", fill=INK_BLACK, font=get_font(30))
    draw.text((SCREEN_W // 2 - 80, content_y + 40), "Tiết 1: Gặp gỡ Kim Trọng", fill=INK_MID, font=get_font(21))
    
    # KOSync Toast floating notification
    toast_y = 200
    draw_round_rect(draw, [84, toast_y, SCREEN_W - 84, toast_y + 60], 30, fill=0x18, outline=0x40, width=2)
    draw_fw_icon(img, "UI_ICON_REFRESH_CW", 115, toast_y + 30, 24, 0xFF)
    draw.text((135, toast_y + 17), "Đã đồng bộ KOSync (42%) · sync.vreader.cloud", fill=0xFF, font=get_font(19))
    
    verses = [
        "Trăm năm trong cõi người ta,",
        "Chữ tài chữ mệnh khéo là ghét nhau.",
        "Trải qua một cuộc bể dâu,",
        "Những điều trông thấy mà đau đớn lòng.",
        "Lạ gì bỉ sắc tư phong,",
        "Trời xanh quen thói má hồng đánh ghen.",
        "",
        "Cảo thơm lần giở trước đèn,",
        "Phong tình có lục còn truyền sử xanh.",
        "Rằng: Năm Gia Tĩnh triều Minh,",
        "Bốn phương phẳng lặng, hai kinh vững vàng.",
        "Có nhà viên ngoại họ Vương,",
        "Gia tư nghĩ cũng thường thường bực trung.",
        "Một trai con thứ rốt lòng,",
        "Vương Quan là chữ, nối dòng nho gia.",
        "Đầu lòng hai ả tố nga,",
        "Thúy Kiều là chị, em là Thúy Vân."
    ]
    
    text_y = 280
    f_verse = get_font(26)
    for line in verses:
        if line == "":
            text_y += 18
            continue
        is_six = len(line.strip().split()) == 6
        indent = 75 if is_six else 125
        draw.text((indent, text_y), line, fill=INK_BLACK, font=f_verse)
        text_y += 42
        
    # Reader footer progress
    foot_y = 1040
    draw.line([(36, foot_y), (SCREEN_W - 36, foot_y)], fill=INK_BORDER, width=1)
    draw_round_rect(draw, [36, foot_y + 10, SCREEN_W - 36, foot_y + 18], 4, fill=0xDD)
    draw_round_rect(draw, [36, foot_y + 10, 36 + int((SCREEN_W - 72) * 0.42), foot_y + 18], 4, fill=0x22)
    
    draw.text((36, foot_y + 26), "Chương 3 · Gặp gỡ Kim Trọng", fill=INK_DARK, font=get_font(20))
    draw.text((SCREEN_W - 200, foot_y + 26), "Trang 42 / 100 (42%)", fill=INK_DARK, font=get_font(20))
    
    # Reader Tool Sheet on bottom
    tool_y = 1100
    draw.rectangle([0, tool_y, SCREEN_W, SCREEN_H], fill=0xF6)
    draw.line([(0, tool_y), (SCREEN_W, tool_y)], fill=0x44, width=2)
    
    tools = [
        ("UI_ICON_LIST", "Mục lục"),
        ("UI_ICON_BOOKMARK", "Đánh dấu"),
        ("UI_ICON_REFRESH_CW", "KOSync"),
        ("UI_ICON_BOOK_OPEN", "Cài đặt"),
        ("UI_ICON_TYPE", "Phông chữ")
    ]
    tw = SCREEN_W // 5
    for i, (icon_name, lbl) in enumerate(tools):
        cx = i * tw + tw // 2
        is_kosync = (i == 2)
        if is_kosync:
            draw_round_rect(draw, [cx - 48, tool_y + 10, cx + 48, tool_y + 86], 16, fill=0x22)
            draw_fw_icon(img, icon_name, cx, tool_y + 35, 30, 0xFF)
            draw.text((cx - 28, tool_y + 56), lbl, fill=0xFF, font=get_font(18))
        else:
            draw_fw_icon(img, icon_name, cx, tool_y + 35, 30, INK_BLACK)
            draw.text((cx - 28, tool_y + 56), lbl, fill=INK_BLACK, font=get_font(18))
            
    img.save(OUTPUT_DIR / "03_reader_kosync.png")
    print("Rendered 03_reader_kosync.png")

# ==============================================================================
# SCREEN 4: OPDS Catalog Browser
# ==============================================================================
def render_04_opds_browser():
    img = Image.new('L', (SCREEN_W, SCREEN_H), INK_WHITE)
    draw = ImageDraw.Draw(img)
    draw_status_bar(img, draw)
    
    # Header
    draw_fw_icon(img, "UI_ICON_CHEVRON_LEFT", 50, 98, 30, INK_BLACK)
    draw.text((75, 75), "Thư viện OPDS", fill=INK_BLACK, font=get_font(42))
    
    # Server badge
    draw_round_rect(draw, [SCREEN_W - 290, 80, SCREEN_W - 36, 126], 14, fill=0xE8, outline=0x40, width=1)
    draw.text((SCREEN_W - 275, 92), "sach.vreader.cloud:8443", fill=INK_BLACK, font=get_font(18))
    
    # Search bar
    draw_round_rect(draw, [36, 145, SCREEN_W - 36, 205], 16, fill=0xF6, outline=0x66, width=1)
    draw.text((56, 162), "Tìm kiếm tựa sách, tác giả, thể loại...", fill=INK_MID, font=get_font(22))
    draw.line([(36, 225), (SCREEN_W - 36, 225)], fill=INK_BORDER, width=1)
    
    # Section: Catalog Feeds
    draw.text((36, 245), "DANH MỤC PHÂN LOẠI", fill=INK_DARK, font=get_font(20))
    categories = [
        ("Sách mới cập nhật", "240 tựa sách mới được thêm tuần này"),
        ("Văn học Việt Nam kinh điển", "156 tác phẩm văn học chọn lọc thế kỷ XX"),
        ("Văn học dịch & Tiểu thuyết", "320 đầu sách văn học kinh điển thế giới"),
        ("Khoa học, Lịch sử & Kỹ năng", "95 cuốn sách kiến thức & tư duy")
    ]
    
    cat_y = 280
    for idx, (cat_name, cat_desc) in enumerate(categories):
        cy = cat_y + idx * 78
        draw_round_rect(draw, [36, cy, SCREEN_W - 36, cy + 68], 14, fill=0xF9, outline=0x99, width=1)
        draw_fw_icon(img, "UI_ICON_FOLDER", 64, cy + 34, 28, INK_BLACK)
        draw.text((94, cy + 10), cat_name, fill=INK_BLACK, font=get_font(22))
        draw.text((94, cy + 38), cat_desc, fill=INK_MID, font=get_font(16))
        draw_fw_icon(img, "UI_ICON_CHEVRON_RIGHT", SCREEN_W - 65, cy + 34, 24, INK_DARK)
        
    # Section: Featured OPDS Books
    book_sec_y = 620
    draw.text((36, book_sec_y), "SÁCH NỔI BẬT ĐƯỢC TẢI NHIỀU", fill=INK_DARK, font=get_font(20))
    
    opds_books = [
        ("Hoàng Tử Bé", "Antoine de Saint-Exupéry", "EPUB · 1.4 MB", "Đã tải về", 100),
        ("Rừng Na Uy", "Haruki Murakami", "EPUB · 2.8 MB", "Tải xuống", 0),
        ("Không Gia Đình", "Hector Malot", "EPUB · 3.5 MB", "Đang tải 78%", 78),
        ("Hai Số Phận", "Jeffrey Archer", "EPUB · 2.1 MB", "Tải xuống", 0)
    ]
    
    b_start_y = book_sec_y + 35
    for idx, (bname, bauth, bmeta, bbtn, bprog) in enumerate(opds_books):
        by = b_start_y + idx * 110
        draw_round_rect(draw, [36, by, SCREEN_W - 36, by + 98], 16, fill=0xF8, outline=0x66, width=1)
        draw_round_rect(draw, [50, by + 12, 105, by + 86], 8, fill=0xDD, outline=0x44, width=1)
        draw_fw_icon(img, "UI_ICON_BOOK_OPEN", 77, by + 49, 28, INK_BLACK)
        
        draw.text((120, by + 12), bname, fill=INK_BLACK, font=get_font(24))
        draw.text((120, by + 42), f"{bauth} · {bmeta}", fill=INK_DARK, font=get_font(18))
        
        if bprog > 0 and bprog < 100:
            draw_round_rect(draw, [120, by + 72, SCREEN_W - 210, by + 82], 4, fill=0xDD)
            draw_round_rect(draw, [120, by + 72, 120 + int((SCREEN_W - 330) * (bprog / 100.0)), by + 82], 4, fill=0x22)
            
        btn_x = SCREEN_W - 190
        btn_y = by + 24
        is_dl = (bprog > 0 and bprog < 100)
        is_done = (bprog == 100)
        draw_round_rect(draw, [btn_x, btn_y, btn_x + 135, btn_y + 50], 14, fill=0x33 if is_dl or not is_done else 0xCC)
        draw.text((btn_x + 16, btn_y + 12), bbtn, fill=0xFF if is_dl or not is_done else 0x22, font=get_font(18))

    draw_bottom_nav(img, draw, active_tab=1)
    img.save(OUTPUT_DIR / "04_opds_browser.png")
    print("Rendered 04_opds_browser.png")

# ==============================================================================
# SCREEN 5: Files Manager & CrossPoint SD OTA update
# ==============================================================================
def render_05_files_ota():
    img = Image.new('L', (SCREEN_W, SCREEN_H), INK_WHITE)
    draw = ImageDraw.Draw(img)
    draw_status_bar(img, draw)
    
    # Header
    draw.text((36, 75), "Tệp tin", fill=INK_BLACK, font=get_font(48))
    
    # Storage Card Info
    sc_y = 145
    draw_round_rect(draw, [36, sc_y, SCREEN_W - 36, sc_y + 115], 20, fill=0xF8, outline=0x40, width=2)
    draw.text((56, sc_y + 18), "Thẻ nhớ microSD (TF Card)", fill=INK_BLACK, font=get_font(24))
    draw.text((SCREEN_W - 200, sc_y + 20), "15.8 GB FAT32", fill=INK_DARK, font=get_font(20))
    
    draw_round_rect(draw, [56, sc_y + 54, SCREEN_W - 56, sc_y + 68], 7, fill=0xDD)
    draw_round_rect(draw, [56, sc_y + 54, 56 + int((SCREEN_W - 112) * 0.12), sc_y + 68], 7, fill=0x22)
    draw.text((56, sc_y + 78), "Đã dùng 1.6 GB · Còn trống 14.2 GB", fill=INK_MID, font=get_font(18))
    
    # 3 Transfer Methods
    tr_y = 280
    draw.text((36, tr_y), "Truyền tệp & Đồng bộ", fill=INK_DARK, font=get_font(20))
    
    trans_w = (SCREEN_W - 72 - 24) // 3
    transfers = [
        ("UI_ICON_WIFI", "Truyền WiFi", "Cùng mạng LAN"),
        ("UI_ICON_RADIO_TOWER", "Điểm phát", "dpReader-AP"),
        ("UI_ICON_CPU", "Cáp USB", "Kết nối PC")
    ]
    for idx, (icon_name, name, desc) in enumerate(transfers):
        tx = 36 + idx * (trans_w + 12)
        ty = tr_y + 30
        draw_round_rect(draw, [tx, ty, tx + trans_w, ty + 100], 18, fill=0xF6, outline=0x66, width=1)
        draw_fw_icon(img, icon_name, tx + trans_w // 2, ty + 28, 30, INK_BLACK)
        draw.text((tx + 18, ty + 48), name, fill=INK_BLACK, font=get_font(20))
        draw.text((tx + 18, ty + 72), desc, fill=INK_MID, font=get_font(15))
        
    # File List
    fl_y = 440
    draw.text((36, fl_y), "Thư mục gốc thẻ nhớ · 5 mục", fill=INK_DARK, font=get_font(20))
    
    files = [
        ("UI_ICON_FOLDER", "books/", "Thư mục sách · 18 tệp sách", "Thư mục sách"),
        ("UI_ICON_FOLDER", "fonts/", "Thư mục phông · 4 tệp phông chữ", "Thư mục phông"),
        ("UI_ICON_FOLDER", "pictures/", "Thư mục hình ảnh & hình nền khóa", ""),
        ("UI_ICON_FILE", "dpReader_metalio47_firmware.bin", "2.4 MB · 26/10/2026 23:15 · Bản v1.0.1-vn", "Gói cập nhật"),
        ("UI_ICON_FILE", "Metalio-E-Ink4-Plus.bin", "2.1 MB · Firmware gốc CloudZao", "")
    ]
    
    f_start = fl_y + 35
    for idx, (icon_name, name, detail, tag) in enumerate(files):
        fy = f_start + idx * 82
        is_update = "dpReader_metalio47" in name
        draw_round_rect(draw, [36, fy, SCREEN_W - 36, fy + 72], 16, fill=0xEE if is_update else 0xFA, outline=0x22 if is_update else 0x99, width=2 if is_update else 1)
        draw_fw_icon(img, icon_name, 66, fy + 36, 32, INK_BLACK)
        draw.text((100, fy + 12), name, fill=INK_BLACK, font=get_font(21))
        draw.text((100, fy + 42), detail, fill=INK_MID, font=get_font(16))
        
        if tag:
            draw_round_rect(draw, [SCREEN_W - 170, fy + 20, SCREEN_W - 56, fy + 52], 8, fill=0x33 if is_update else 0xDD)
            draw.text((SCREEN_W - 160, fy + 26), tag, fill=0xFF if is_update else INK_BLACK, font=get_font(15))

    # CrossPoint style update confirmation dialog popped up
    pop_y = 880
    draw_round_rect(draw, [36, pop_y, SCREEN_W - 36, pop_y + 215], 24, fill=0xF2, outline=0x22, width=3)
    draw_fw_icon(img, "UI_ICON_ZAP", 66, pop_y + 35, 28, INK_BLACK)
    draw.text((90, pop_y + 20), "Cập nhật Firmware dpReader v1.0.1-vn", fill=INK_BLACK, font=get_font(26))
    draw.text((56, pop_y + 60), "Phát hiện gói nâng cấp trực tiếp từ thẻ nhớ TF (CrossPoint style).", fill=INK_DARK, font=get_font(19))
    draw.text((56, pop_y + 88), "Hệ thống sẽ nạp vào phân vùng OTA và tự khởi động lại an toàn.", fill=INK_MID, font=get_font(17))
    
    # Dialog action buttons
    draw_round_rect(draw, [56, pop_y + 135, SCREEN_W // 2 - 15, pop_y + 195], 16, fill=0xDD, outline=0x77, width=1)
    draw.text((SCREEN_W // 4 - 30, pop_y + 152), "Hủy bỏ", fill=INK_BLACK, font=get_font(22))
    
    draw_round_rect(draw, [SCREEN_W // 2 + 15, pop_y + 135, SCREEN_W - 56, pop_y + 195], 16, fill=0x22)
    draw.text((SCREEN_W * 3 // 4 - 85, pop_y + 152), "Cài đặt ngay (OTA)", fill=0xFF, font=get_font(22))

    draw_bottom_nav(img, draw, active_tab=2)
    img.save(OUTPUT_DIR / "05_files_ota.png")
    print("Rendered 05_files_ota.png")

# ==============================================================================
# SCREEN 6: System Settings (Tiếng Việt)
# ==============================================================================
def render_06_settings_vn():
    img = Image.new('L', (SCREEN_W, SCREEN_H), INK_WHITE)
    draw = ImageDraw.Draw(img)
    draw_status_bar(img, draw)
    
    # Header
    draw.text((36, 75), "Cài đặt", fill=INK_BLACK, font=get_font(48))
    
    # Profile Card
    p_y = 145
    draw_round_rect(draw, [36, p_y, SCREEN_W - 36, p_y + 115], 20, fill=0xF6, outline=0x44, width=2)
    draw.ellipse([56, p_y + 18, 136, p_y + 98], fill=0x33)
    draw.text((78, p_y + 36), "dp", fill=0xFF, font=get_font(34))
    
    draw.text((156, p_y + 24), "dpReader Metalio 4.7", fill=INK_BLACK, font=get_font(28))
    draw.text((156, p_y + 64), "Firmware Tiếng Việt · v1.0.1-vn", fill=INK_MID, font=get_font(20))
    draw.text((SCREEN_W - 100, p_y + 42), "Sửa ›", fill=INK_DARK, font=get_font(20))
    
    groups = [
        ("KẾT NỐI & THỜI GIAN", [
            ("UI_ICON_WIFI", "WiFi", "vreader-home (Đã kết nối)"),
            ("UI_ICON_BLUETOOTH", "Điều khiển lật trang BLE", "Đã bật (Chế độ đọc)"),
            ("UI_ICON_CLOCK", "Ngày & Giờ", "NTP GMT+7 (Hà Nội, VST)")
        ]),
        ("HIỂN THỊ & PHẦN CỨNG", [
            ("UI_ICON_TYPE", "Phông chữ hệ thống", "builtin.ttf · 100%"),
            ("UI_ICON_CONTRAST", "Độ tương phản hệ thống", "100% (Sắc nét)"),
            ("UI_ICON_REFRESH_CW", "Chế độ làm mới Trang chủ", "Làm mới nhanh"),
            ("UI_ICON_LIGHTBULB", "Đèn nền & Rung", "Độ sáng 40% · Nhiệt màu 30%")
        ]),
        ("CẬP NHẬT & BẢO TRÌ", [
            ("UI_ICON_DOWNLOAD", "Cập nhật hệ thống", "OTA / Thẻ nhớ microSD"),
            ("UI_ICON_FOLDER", "Khôi phục thẻ nhớ", "Kiểm tra thư mục & Phông"),
            ("UI_ICON_CPU", "Nạp ROM (BOOT)", "Chế độ nạp qua PC")
        ])
    ]
    
    cur_y = 285
    for grp_title, items in groups:
        draw.text((36, cur_y), grp_title, fill=INK_MID, font=get_font(18))
        cur_y += 30
        
        card_h = len(items) * 68
        draw_round_rect(draw, [36, cur_y, SCREEN_W - 36, cur_y + card_h], 20, fill=0xFA, outline=0x77, width=1)
        
        for idx, (icon_name, label, val) in enumerate(items):
            iy = cur_y + idx * 68
            if idx > 0:
                draw.line([(56, iy), (SCREEN_W - 56, iy)], fill=0xDD, width=1)
            draw_fw_icon(img, icon_name, 72, iy + 34, 26, INK_BLACK)
            draw.text((96, iy + 20), label, fill=INK_BLACK, font=get_font(22))
            draw.text((SCREEN_W - 100 - get_font(19).getbbox(val)[2], iy + 22), val, fill=INK_DARK, font=get_font(19))
            draw_fw_icon(img, "UI_ICON_CHEVRON_RIGHT", SCREEN_W - 65, iy + 34, 20, INK_MID)
            
        cur_y += card_h + 30

    draw_bottom_nav(img, draw, active_tab=3)
    img.save(OUTPUT_DIR / "06_settings_vn.png")
    print("Rendered 06_settings_vn.png")

# ==============================================================================
# SCREEN 7: Web Management Portal (Desktop / Browser View)
# ==============================================================================
def render_07_web_portal():
    w, h = 1200, 780
    img = Image.new('L', (w, h), 0xFA)
    draw = ImageDraw.Draw(img)
    
    # Browser chrome window
    draw.rectangle([0, 0, w, 80], fill=0xEE)
    draw.line([(0, 80), (w, 80)], fill=0x99, width=1)
    
    # Traffic lights
    draw.ellipse([20, 28, 36, 44], fill=0x66)
    draw.ellipse([46, 28, 62, 44], fill=0x88)
    draw.ellipse([72, 28, 88, 44], fill=0xAA)
    
    # URL bar
    draw_round_rect(draw, [140, 20, w - 140, 60], 12, fill=0xFF, outline=0x88, width=1)
    draw.text((160, 28), "http://192.168.1.100 (hoặc http://dpreader.local) — dpReader Web Portal", fill=0x22, font=get_font(20))
    
    # Portal header
    draw.text((50, 110), "dpReader Management Portal", fill=0x11, font=get_font(36))
    draw.text((50, 155), "Metalio E-ink 4 Plus · Firmware v1.0.1-vn · Bộ nhớ thẻ trống 14.2 GB", fill=0x66, font=get_font(19))
    
    # Navigation tabs
    draw_round_rect(draw, [50, 195, 230, 245], 10, fill=0xEE, outline=0x88, width=1)
    draw.text((70, 208), "Truyền sách WiFi", fill=0x44, font=get_font(20))
    
    draw_round_rect(draw, [245, 195, 520, 245], 10, fill=0x22)
    draw.text((265, 208), "Cấu hình KOSync & OPDS", fill=0xFF, font=get_font(20))
    
    draw_round_rect(draw, [535, 195, 715, 245], 10, fill=0xEE, outline=0x88, width=1)
    draw.text((555, 208), "Cài đặt hệ thống", fill=0x44, font=get_font(20))
    
    draw.line([(50, 245), (w - 50, 245)], fill=0x99, width=2)
    
    # Box 1: KOSync Configuration (Left side)
    box1_x = 50
    box_w = 520
    draw_round_rect(draw, [box1_x, 275, box1_x + box_w, 730], 16, fill=0xFF, outline=0x55, width=2)
    draw.text((box1_x + 30, 300), "Đồng bộ tiến độ đọc KOReader (KOSync)", fill=0x11, font=get_font(24))
    draw.text((box1_x + 30, 335), "Tự động đồng bộ vị trí đọc với máy đọc sách khác.", fill=0x66, font=get_font(16))
    
    draw.text((box1_x + 30, 375), "KOSync Server URL:", fill=0x22, font=get_font(18))
    draw_round_rect(draw, [box1_x + 30, 402, box1_x + box_w - 30, 445], 8, fill=0xF6, outline=0x44, width=1)
    draw.text((box1_x + 45, 414), "https://sync.vreader.cloud", fill=0x11, font=get_font(19))
    
    draw.text((box1_x + 30, 465), "Tên đăng nhập (Username):", fill=0x22, font=get_font(18))
    draw_round_rect(draw, [box1_x + 30, 492, box1_x + box_w - 30, 535], 8, fill=0xF6, outline=0x44, width=1)
    draw.text((box1_x + 45, 504), "dephia31", fill=0x11, font=get_font(19))
    
    draw.text((box1_x + 30, 555), "Mật khẩu (User Key / Token):", fill=0x22, font=get_font(18))
    draw_round_rect(draw, [box1_x + 30, 582, box1_x + box_w - 30, 625], 8, fill=0xF6, outline=0x44, width=1)
    draw.text((box1_x + 45, 594), "••••••••••••••••", fill=0x11, font=get_font(19))
    
    draw.text((box1_x + 30, 645), "[X] Tự động đồng bộ khi mở & đóng sách", fill=0x11, font=get_font(18))
    draw_round_rect(draw, [box1_x + 30, 675, box1_x + 250, 715], 8, fill=0x22)
    draw.text((box1_x + 45, 685), "Lưu cấu hình KOSync", fill=0xFF, font=get_font(18))
    
    # Box 2: OPDS Catalog Configuration (Right side)
    box2_x = 610
    draw_round_rect(draw, [box2_x, 275, box2_x + box_w, 730], 16, fill=0xFF, outline=0x55, width=2)
    draw.text((box2_x + 30, 300), "Danh mục sách Calibre-Web (OPDS)", fill=0x11, font=get_font(24))
    draw.text((box2_x + 30, 335), "Duyệt và tải sách trực tiếp từ kho thư viện online.", fill=0x66, font=get_font(16))
    
    draw.text((box2_x + 30, 375), "OPDS Server URL:", fill=0x22, font=get_font(18))
    draw_round_rect(draw, [box2_x + 30, 402, box2_x + box_w - 30, 445], 8, fill=0xF6, outline=0x44, width=1)
    draw.text((box2_x + 45, 414), "https://sach.vreader.cloud:8443/opds", fill=0x11, font=get_font(19))
    
    draw.text((box2_x + 30, 465), "Thư mục lưu sách tải về:", fill=0x22, font=get_font(18))
    draw_round_rect(draw, [box2_x + 30, 492, box2_x + box_w - 30, 535], 8, fill=0xF6, outline=0x44, width=1)
    draw.text((box2_x + 45, 504), "/sdcard/books", fill=0x11, font=get_font(19))
    
    draw.text((box2_x + 30, 555), "Kéo thả sách lên thiết bị nhanh:", fill=0x22, font=get_font(18))
    draw_round_rect(draw, [box2_x + 30, 582, box2_x + box_w - 30, 650], 10, fill=0xFA, outline=0x88, width=1)
    draw.text((box2_x + 70, 604), "Kéo thả file .epub, .txt vào đây để nạp vào máy", fill=0x55, font=get_font(18))
    
    draw_round_rect(draw, [box2_x + 30, 675, box2_x + 250, 715], 8, fill=0x22)
    draw.text((box2_x + 50, 685), "Lưu cấu hình OPDS", fill=0xFF, font=get_font(18))
    
    img.save(OUTPUT_DIR / "07_web_portal.png")
    print("Rendered 07_web_portal.png")

# ==============================================================================
# SCREEN 0: Overview Composite Banner
# ==============================================================================
def render_00_overview_banner():
    bw, bh = 1200, 630
    banner = Image.new('RGB', (bw, bh), (22, 27, 34))
    draw = ImageDraw.Draw(banner)
    
    # Background subtle grid
    for x in range(0, bw, 40):
        draw.line([(x, 0), (x, bh)], fill=(28, 34, 42), width=1)
    for y in range(0, bh, 40):
        draw.line([(0, y), (bw, y)], fill=(28, 34, 42), width=1)
        
    # Title & Subtitle on Top Left
    draw.text((60, 36), "dpReader", fill=(255, 255, 255), font=get_font(48))
    draw.text((275, 48), "v1.0.1-vn", fill=(88, 166, 255), font=get_font(26))
    draw.text((60, 95), "Firmware đọc sách E-ink tối ưu cho Metalio E-ink 4 Plus (4.7 inch)", fill=(201, 209, 217), font=get_font(22))
    draw.text((60, 125), "Việt hóa 100% · Font Unicode builtin.ttf sắc nét · KOSync & OPDS · OTA Thẻ nhớ", fill=(139, 148, 158), font=get_font(18))
    
    # Feature Pills
    pills = [
        "Tiếng Việt 100%",
        "KOSync: sync.vreader.cloud",
        "OPDS: sach.vreader.cloud",
        "CrossPoint SD OTA",
        "Giờ Việt Nam GMT+7"
    ]
    px = 60
    py = 158
    for pill in pills:
        bbox_p = get_font(15).getbbox(pill)
        pw = (bbox_p[2] - bbox_p[0]) + 24
        draw.rounded_rectangle([px, py, px + pw, py + 30], radius=8, fill=(33, 38, 45), outline=(48, 54, 61), width=1)
        draw.text((px + 12, py + 5), pill, fill=(88, 166, 255), font=get_font(15))
        px += pw + 12
        
    s1 = Image.open(OUTPUT_DIR / "01_home_dashboard.png").convert('RGB')
    s2 = Image.open(OUTPUT_DIR / "03_reader_kosync.png").convert('RGB')
    s3 = Image.open(OUTPUT_DIR / "02_bookshelf.png").convert('RGB')
    
    target_h = 365
    target_w = int(SCREEN_W * (target_h / float(SCREEN_H)))
    
    r1 = s1.resize((target_w, target_h), Image.Resampling.LANCZOS)
    r2 = s2.resize((target_w + 24, target_h + 38), Image.Resampling.LANCZOS)
    r3 = s3.resize((target_w, target_h), Image.Resampling.LANCZOS)
    
    def paste_device(base_img, screen_img, dx, dy, is_center=False):
        dw, dh = screen_img.size
        bezel_x = dx - 12
        bezel_y = dy - 16
        bezel_w = dw + 24
        bezel_h = dh + 32
        
        draw_b = ImageDraw.Draw(base_img)
        draw_b.rounded_rectangle([bezel_x + 6, bezel_y + 10, bezel_x + bezel_w + 6, bezel_y + bezel_h + 10], radius=16, fill=(10, 12, 16))
        draw_b.rounded_rectangle([bezel_x, bezel_y, bezel_x + bezel_w, bezel_y + bezel_h], radius=16, fill=(40, 44, 52), outline=(100, 110, 125) if is_center else (60, 65, 75), width=2 if is_center else 1)
        draw_b.rounded_rectangle([dx - 2, dy - 2, dx + dw + 2, dy + dh + 2], radius=4, fill=(15, 15, 15))
        base_img.paste(screen_img, (dx, dy))

    paste_device(banner, r1, 140, 230, is_center=False)
    paste_device(banner, r3, 860, 230, is_center=False)
    paste_device(banner, r2, 490, 212, is_center=True)
    
    banner.save(OUTPUT_DIR / "00_overview_banner.png")
    print("Rendered 00_overview_banner.png")

if __name__ == "__main__":
    print(f"Generating screenshots into {OUTPUT_DIR}...")
    render_01_home_dashboard()
    render_02_bookshelf()
    render_03_reader_kosync()
    render_04_opds_browser()
    render_05_files_ota()
    render_06_settings_vn()
    render_07_web_portal()
    render_00_overview_banner()
    print("All screenshots generated successfully!")
