set(kiiko_selected_sources "e0470_page_turn.c")
idf_component_register(
    SRCS ${kiiko_selected_sources}
    INCLUDE_DIRS "include"
    REQUIRES epdiy e0470_epaper_waveform
    PRIV_REQUIRES log esp_timer
)
