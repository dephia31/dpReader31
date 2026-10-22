set(kiiko_selected_sources "continuous_du.c")
idf_component_register(
    SRCS ${kiiko_selected_sources}
    INCLUDE_DIRS "include"
    REQUIRES epdiy e0470_epaper_waveform read_pico
)
