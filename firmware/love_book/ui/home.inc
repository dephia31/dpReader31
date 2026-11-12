/* SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：从原应用文件迁移，保留原许可。/ English: Moved from the original app under its existing license. */
/* 中文：I Love Book界面，Full可选复用。/ English: I Love Book UI, optionally reused by Full. */

static EpdRect simple_slot_rect(unsigned i) {return (EpdRect){52+(int)i*148,938,136,108};}

static EpdRect simple_add_rect(void) {return simple_slot_rect(1);}

static EpdRect simple_import_rect(unsigned i) {return (EpdRect){36,260+(int)i*180,612,140};}

static EpdRect simple_open_rect(void) {return simple_slot_rect(0);}

static void simple_utility(uint8_t *fb,EpdRect box,ui_icon_t icon,const char *title,bool configurable) {
    bool labels=!configurable||app_settings_simple_utility_labels();
    bool border=!configurable||app_settings_simple_utility_border();
    ui_fill_round_rect(fb,box,16,UI_GRAY_WHITE);if(border)home_frame(fb,box,16);
    ui_draw_icon_normalized(fb,box.x+box.width/2,box.y+(labels?27:box.height/2),labels?32:64,icon,0);
    if(labels)ui_text_caption(fb,(EpdRect){box.x+6,box.y+50,box.width-12,box.height-56},box.width>200?24:18,title,EPD_DRAW_ALIGN_CENTER,false);
}

static unsigned simple_title(uint8_t *fb,const char *text) {
    unsigned rows=0;int measure=ui_text_effective_px(36);
    while(*text&&rows<3) {
        while(*text==' ')++text;
        if(!*text)break;
        char line[128];size_t used=0,last_space=0;
        while(text[used]) {
            size_t next=used+1;
            while(text[next]&&((unsigned char)text[next]&0xc0)==0x80)++next;
            if(next>=sizeof(line))break;
            memcpy(line,text,next);line[next]=0;
            if(used&&ui_text_fixed_width_px(measure,line)>274)break;
            used=next;if(text[used-1]==' ')last_space=used-1;
        }
        if(text[used]&&last_space)used=last_space;
        memcpy(line,text,used);line[used]=0;text+=used;
        while(*text==' ')++text;
        if(rows==2&&*text) {
            for(;;) {
                while(used&&line[used-1]==' ')line[--used]=0;
                if(used+sizeof("…")<=sizeof(line)) {
                    memcpy(line+used,"…",sizeof("…"));
                    if(ui_text_fixed_width_px(measure,line)<=274||!used)break;
                }
                if(!used)break;
                --used;while(used&&((unsigned char)line[used]&0xc0)==0x80)--used;line[used]=0;
            }
        }
        ui_text_box(fb,(EpdRect){348,278+(int)rows*56,274,56},36,line,EPD_DRAW_ALIGN_LEFT,false);
        ++rows;
    }
    return rows;
}

static EpdRect simple_ticket_status_rect(void) {
    return (EpdRect){60, 80, 564, 58};
}

static void draw_simple_ticket_status(uint8_t *fb) {
    char clock[12] = "--:--", battery[12] = "--%", signature[96];
    int64_t minute = ui_nav_clock_minute();
    if (minute >= 0) {
        unsigned local = (unsigned)((minute + 7 * 60) % (24 * 60));
        snprintf(clock, sizeof(clock), "%02u:%02u", local / 60, local % 60);
    }
    s_ticket_battery = pmu_battery_percent(read_pico_pmu_get());
    if (s_ticket_battery >= 0) snprintf(battery, sizeof(battery), "%d%%", s_ticket_battery);
    snprintf(signature, sizeof(signature), "%s", app_settings_status_signature());
    const int px = 30;
    int clock_width = ui_text_fixed_width_px(px, clock);
    int battery_width = ui_text_fixed_width_px(px, battery);
    int side_width = clock_width > battery_width ? clock_width : battery_width;
    int signature_width = 540 - 2 * (side_width + 12);
    fit(signature, px, signature_width);
    ui_fill_rect_fast(simple_ticket_status_rect(), UI_GRAY_WHITE, fb);
    ui_text_fixed_vc(fb, 72, 109, px, clock, EPD_DRAW_ALIGN_LEFT, false);
    ui_text_fixed_vc(fb, 342, 109, px, signature, EPD_DRAW_ALIGN_CENTER, false);
    ui_text_fixed_vc(fb, 612, 109, px, battery, EPD_DRAW_ALIGN_RIGHT, false);
}

static void draw_simple_border(uint8_t *fb) {
    const EpdRect paper = simple_resume_rect();
    const int notch = 18;
    // The ticket covers the center, including all but its notched top and bottom edges.
    for (int y = 0; y < UI_LOCK_HEIGHT; ++y) {
        const bool solid_paper_row = y >= paper.y + notch && y < paper.y + paper.height - notch;
        for (int x = 0; x < UI_LOCK_WIDTH; ++x) {
            if (solid_paper_row && x == paper.x) x += paper.width;
            epd_draw_pixel(x, y, home_diamond_gray(x, y), fb);
        }
    }
}

static void render_simple_root(uint8_t *fb) {
    char greeting[96];
    snprintf(greeting,sizeof(greeting),"Xin Chào, %s",app_settings_device_name());
    const bool full=app_settings_simple_full_ui();
    home_book_t *b=featured();
    if(full) {
        if (b->cover && b->cover_width && b->cover_height)
            book_ticket_draw_background(fb,b->cover,b->cover_width,b->cover_height,b->cover_fast);
        else
            draw_simple_border(fb);
        book_ticket_draw_shadow(fb);
        book_ticket_draw_paper(fb);
        draw_simple_ticket_status(fb);
        ui_draw_separator(fb, 140, 72, 540, UI_GRAY_LIGHT);
        const char *prefix = "Xin Chào,";
        const int name_x = 72 + ui_text_fixed_width_px(36, prefix) + 10;
        char name[96];
        snprintf(name, sizeof(name), "%s", app_settings_device_name());
        fit(name, 42, 612 - name_x);
        home_bold_label(fb, 72, 185, 36, prefix, EPD_DRAW_ALIGN_LEFT, UI_GRAY_WHITE);
        home_bold_label(fb, name_x, 185, 42, name, EPD_DRAW_ALIGN_LEFT, UI_GRAY_WHITE);
    } else {
        ui_nav_status(fb);
        fit(greeting,54,612);
        ui_text(fb,36,91,52,greeting,EPD_DRAW_ALIGN_LEFT,false);
        home_tag_card(fb,simple_resume_rect(),68,14,UI_GRAY_WHITE);
        home_bold_label(fb,60,222,26,"Đọc tiếp",EPD_DRAW_ALIGN_LEFT,UI_GRAY_WHITE);
    }
    if(b->path[0]) {
        if(full)draw_open_book(fb,b,simple_cover_rect());
        else draw_cover(fb,b,simple_cover_rect());
        if(app_settings_simple_full_ui()) {
            int y=820,title_px=40,author_px=29;
            // Keep the lower text group above the progress row at large UI scales.
            while(title_px>18 && 2*(ui_text_effective_px(title_px)+6)+8+
                  2*(ui_text_effective_px(author_px)+6)>1050-y) {
                --title_px;
                if(author_px>18)--author_px;
            }
            unsigned rows=full_resume_lines(fb,b->title,y,title_px,2,false);
            y+=(int)rows*(ui_text_effective_px(title_px)+6)+8;
            rows=full_resume_lines(fb,b->author,y,author_px,2,false);
            y+=(int)rows*(ui_text_effective_px(author_px)+6)+16;
            if(s_excerpt[0] && y+ui_text_effective_px(24)+6<=1050) {
                char quote[BOOK_EXCERPT_CAP+12];snprintf(quote,sizeof(quote),s_excerpt_is_chapter?"%s":"“%s”",s_excerpt);
                unsigned lines=(1050-y)/(ui_text_effective_px(24)+6);
                if(lines>4)lines=4;
                (void)full_resume_lines(fb,quote,y,24,lines,!s_excerpt_is_chapter);
            }
        } else {
            unsigned title_rows=simple_title(fb,b->title);
            if(b->author[0])ui_text_caption(fb,(EpdRect){348,302+(int)title_rows*56,274,100},25,b->author,EPD_DRAW_ALIGN_LEFT,false);
        }
        char progress[48];snprintf(progress,sizeof(progress),"Đã đọc %u%%",b->has_progress?b->progress.pct:0);
        label_on(fb,60,app_settings_simple_full_ui()?1076:744,full?33:30,progress,EPD_DRAW_ALIGN_LEFT,0x20,UI_GRAY_WHITE);
        home_bold_label(fb,622,app_settings_simple_full_ui()?1076:744,full?31:28,"Đọc tiếp ›",EPD_DRAW_ALIGN_RIGHT,UI_GRAY_WHITE);
        EpdRect track={60,app_settings_simple_full_ui()?1127:790,562,4};
        if(app_settings_main_fast_refresh()) {
            for(int x=track.x;x<track.x+track.width;x+=3)ui_fill_rect_fast((EpdRect){x,track.y,1,track.height},UI_GRAY_BLACK,fb);
        } else ui_fill_round_rect(fb,track,2,UI_GRAY_LIGHT);
        unsigned pct=b->has_progress?b->progress.pct:0;if(pct>100)pct=100;
        int filled=track.width*(int)pct/100;
        if(filled)ui_fill_round_rect(fb,(EpdRect){track.x,track.y-3,filled,10},5,UI_GRAY_BLACK);
    } else {
        ui_draw_icon(fb,342,400,100,UI_ICON_BOOK_OPEN,0);
        ui_text_caption(fb,(EpdRect){66,536,552,130},30,"Chọn sách để bắt đầu đọc",EPD_DRAW_ALIGN_CENTER,false);
        ui_draw_button(fb,(EpdRect){140,704,404,80},"Mở sách",false);
    }
    if(!app_settings_simple_full_ui()) {
    home_tag_card(fb,(EpdRect){36,866,612,206},60,12,UI_GRAY_WHITE);
    label_on(fb,60,896,24,"Tiện Ích",EPD_DRAW_ALIGN_LEFT,0x30,UI_GRAY_WHITE);
    simple_utility(fb,simple_open_rect(),UI_ICON_FOLDER_OPEN,"Mở sách",true);
    simple_utility(fb,simple_add_rect(),UI_ICON_DOWNLOAD,"Thêm sách",true);
    ui_draw_icon(fb,608,896,28,UI_ICON_PENCIL,0);
    for(unsigned i=2;i<4;++i) {
        unsigned action=app_settings_simple_utility(i);
        simple_utility(fb,simple_slot_rect(i),utility_icons[action],action?utility_names[action]:"Thêm",true);
    }
    }
    if(s_utility_notice[0]) {
        EpdRect notice=utility_notice_rect();ui_fill_round_rect(fb,notice,20,UI_GRAY_WHITE);home_frame(fb,notice,20);
        ui_text_caption(fb,(EpdRect){notice.x+12,notice.y+12,notice.width-24,notice.height-24},20,s_utility_notice,EPD_DRAW_ALIGN_CENTER,false);
    }
    if(!app_settings_simple_full_ui())ui_nav_draw(fb,0);
}

static void render_add_books(uint8_t *fb) {
    header(fb,"Thêm sách");
    simple_utility(fb,simple_import_rect(0),UI_ICON_COPY,"Truyền file",false);
    simple_utility(fb,simple_import_rect(1),UI_ICON_RADIO_TOWER,"Thư viện OPDS",false);
}

static app_redraw_t simple_add_action(app_ctx_t *ctx,int x,int y) {
    for(unsigned i=0;i<2;++i) {
        EpdRect box=simple_import_rect(i);
        if(x>=box.x&&x<box.x+box.width&&y>=box.y&&y<box.y+box.height)
            return utility_run(ctx,i?APP_HOME_UTIL_OPDS:APP_HOME_UTIL_TRANSFER);
    }
    return APP_REDRAW_NONE;
}

static EpdRect pop_menu_rect(void) {
    return (EpdRect){0, 732, UI_LOCK_WIDTH, UI_LOCK_HEIGHT - 732};
}

static EpdRect pop_menu_row(unsigned i) {
    EpdRect panel = pop_menu_rect();
    return (EpdRect){panel.x + 24, panel.y + 28 + (int)i * 108, panel.width - 48, 108};
}

static void render_pop_menu(uint8_t *fb) {
    static const ui_icon_t icons[] = {UI_ICON_FOLDER_OPEN, UI_ICON_COPY, UI_ICON_RADIO_TOWER, UI_ICON_SLIDERS_HORIZONTAL};
    static const char *const labels[] = {"Mở sách", "Truyền file", "Thư viện OPDS", "Cài đặt"};
    EpdRect panel = pop_menu_rect();
    ui_fill_rect_fast(panel, UI_GRAY_WHITE, fb);
    ui_fill_rect_fast((EpdRect){panel.x, panel.y, panel.width, 3}, 0x30, fb);
    for (unsigned i = 0; i < 4; ++i) {
        EpdRect row = pop_menu_row(i);
        int cy = row.y + row.height / 2;
        ui_draw_icon_normalized(fb, row.x + 40, cy, 34, icons[i], UI_GRAY_BLACK);
        ui_text_caption(fb, (EpdRect){row.x + 84, row.y, row.width - 100, row.height},
                        28, labels[i], EPD_DRAW_ALIGN_LEFT, false);
        if (i < 3) ui_hairline(fb, row.y + row.height, row.x + 84, row.width - 100, UI_GRAY_LIGHT);
    }
}

static app_redraw_t simple_pop_menu_action(app_ctx_t *ctx, int x, int y) {
    static const unsigned utilities[] = {
        APP_HOME_UTIL_TRANSFER, APP_HOME_UTIL_OPDS, APP_HOME_UTIL_SETTINGS,
    };
    for (unsigned i = 0; i < 4; ++i) {
        if (!ui_rect_hit(pop_menu_row(i), x, y)) continue;
#if DPREADER_BOARD_METALIO47
        ctx->request_back_ripple = app_settings_main_refresh_mode() == APP_MAIN_REFRESH_WATER;
#endif
        if (i == 0) {
            ctx->request_app = app_at(5);
            return APP_REDRAW_NONE;
        }
        return utility_run(ctx, utilities[i - 1]);
    }
    return set_pop_menu_open(false);
}
