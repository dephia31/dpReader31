idf_component_register(SRCS "metalio_audio.c" "metalio_wav.c" INCLUDE_DIRS "include"
    REQUIRES metalio_hw esp_driver_i2s esp_driver_uart esp_timer freertos)
