/* SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：从原应用文件迁移，保留原许可。/ English: Moved from the original app under its existing license. */
/* 中文：Full首页界面。/ English: Full Home interface. */
static void render_root(uint8_t *fb) {
    if(app_settings_simple_ui()){render_simple_root(fb);return;}
#ifdef DPREADER_LOCAL_LATENCY
    int64_t probe_home=esp_timer_get_time();
#define HOME_STAGE(name) do { int64_t probe_now=esp_timer_get_time();ESP_LOGI("latency","HOME stage=%s us=%lld",name,(long long)(probe_now-probe_home));probe_home=probe_now; } while(0)
#else
#define HOME_STAGE(name) do {} while(0)
#endif
    ui_nav_status(fb);ui_text(fb,36,91,52,"Trang chủ",EPD_DRAW_ALIGN_LEFT,false);
    char value[80];unsigned seconds=today_seconds();
    snprintf(value,sizeof(value),"%u phút",seconds/60);fit(value,24,180);label(fb,490,118,24,value,EPD_DRAW_ALIGN_RIGHT,0x30);
    ring(fb,540,118,30,7,seconds,app_settings_home_goal_minutes()*60);
    if(s_avatar_ready)ui_wallpaper_draw_avatar(fb,s_avatar,(EpdRect){588,88,60,60},30);
    else {epd_fill_circle(618,118,30,0x30,fb);label(fb,618,118,30,"K",EPD_DRAW_ALIGN_CENTER,0xff);}
    HOME_STAGE("header");
#if DPREADER_BOARD_METALIO47
    const uint8_t card_gray=0xff;
#else
    const uint8_t card_gray=app_settings_main_fast_refresh()?0xff:0xee;
#endif
    home_tag_card(fb,(EpdRect){36,184,612,378},68,14,card_gray);
    home_bold_label(fb,60,222,26,"Đọc tiếp",EPD_DRAW_ALIGN_LEFT,card_gray);
    if(s_excerpt[0]) {
        char quote[BOOK_EXCERPT_CAP+12];snprintf(quote,sizeof(quote),"“%s”",s_excerpt);
        int px=22;while(px>16&&ui_text_fixed_width_px(px,quote)>426)--px;
        if(ui_text_fixed_width_px(px,quote)<=426)label_on(fb,622,222,px,quote,EPD_DRAW_ALIGN_RIGHT,0x60,card_gray);
    }
    HOME_STAGE("card_top");
    home_book_t *b=featured();
    if(b->path[0]) {
        // 按住整张卡片时封面轻抬4px；原边框不动，不追加黑框或整页刷新。
        // Lift the cover4px while holding any part of the card; keep its frame still without extra boxes/full refreshes.
        draw_cover(fb,b,(EpdRect){60,continue_card_pressed()?274:278,176,240});
        int rows=title_lines(fb,b->title,card_gray);
        if(b->author[0]) {char author[128];snprintf(author,sizeof(author),"%s",b->author);fit(author,25,358);label_on(fb,264,rows==2?406:367,25,author,EPD_DRAW_ALIGN_LEFT,0x50,card_gray);}
        snprintf(value,sizeof(value),"Đã đọc %u%%",b->has_progress?b->progress.pct:0);
        label_on(fb,264,482,30,value,EPD_DRAW_ALIGN_LEFT,0x20,card_gray);
        home_bold_label(fb,622,482,28,"Đọc tiếp ›",EPD_DRAW_ALIGN_RIGHT,card_gray);
        EpdRect track={264,514,358,4};
        if(app_settings_main_fast_refresh()) {
            for(int x=track.x;x<track.x+track.width;x+=3)
                ui_fill_rect_fast((EpdRect){x,track.y,1,track.height},UI_GRAY_BLACK,fb);
        } else ui_fill_round_rect(fb,track,2,UI_GRAY_LIGHT);
        unsigned pct=b->has_progress?b->progress.pct:0;if(pct>100)pct=100;
        int filled=track.width*(int)pct/100;
        if(filled)ui_fill_round_rect(fb,(EpdRect){track.x,511,filled,10},5,UI_GRAY_BLACK);
    } else label_on(fb,342,410,23,"Sau khi mở sách, bạn có thể đọc tiếp ngay tại đây",EPD_DRAW_ALIGN_CENTER,0x60,card_gray);
    HOME_STAGE("card_body");
    EpdRect utility_card={36,590,612,206};
    home_tag_card(fb,utility_card,60,12,card_gray);
    label_on(fb,60,620,24,"Tiện Ích",EPD_DRAW_ALIGN_LEFT,0x30,card_gray);
    if(utilities_configured()) {
        ui_draw_icon(fb,608,620,28,UI_ICON_PENCIL,0);
        for(unsigned i=0;i<4;++i) {
            unsigned action=app_settings_home_utility(i);EpdRect box=utility_button(i);
            ui_fill_round_rect(fb,box,16,card_gray);
            if(app_settings_home_utility_border())home_frame(fb,box,16);
            bool labels=app_settings_home_utility_labels();EpdRect icon=utility_icon_rect(i,labels);
            ui_draw_icon_normalized(fb,icon.x+icon.width/2,icon.y+icon.height/2,icon.width,utility_icons[action],0);
            if(labels)ui_text_caption(fb,(EpdRect){box.x+6,box.y+50,box.width-12,52},18,action?utility_names[action]:"Thêm",EPD_DRAW_ALIGN_CENTER,false);
        }
    }else ui_draw_button(fb,(EpdRect){150,672,384,88},"Thiết lập",false);
    home_book_t *recent[4];unsigned count=recent_books(recent);
    for(unsigned i=0;i<4;++i) {
        EpdRect box=recent_cover(i);
        if(i<count) {
            draw_cover(fb,recent[i],box);
            ui_text_caption(fb,(EpdRect){box.x,1018,box.width,62},19,recent[i]->title,EPD_DRAW_ALIGN_CENTER,false);
        } else {ui_fill_round_rect(fb,box,12,0xf0);home_frame(fb,box,12);ui_draw_icon(fb,box.x+66,box.y+90,38,UI_ICON_BOOK_OPEN,0x90);}
    }
    if(s_utility_notice[0]) {
        EpdRect notice=utility_notice_rect();ui_fill_round_rect(fb,notice,20,0xff);home_frame(fb,notice,20);
        ui_text_caption(fb,(EpdRect){112,824,460,76},22,s_utility_notice,EPD_DRAW_ALIGN_CENTER,false);
    }
    HOME_STAGE("utilities_and_books");
    ui_nav_draw(fb,0);
    HOME_STAGE("nav");
#undef HOME_STAGE
}
