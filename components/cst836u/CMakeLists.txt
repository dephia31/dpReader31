idf_component_register(
    SRCS "metalio_ft6336.c" "metalio_touch_decode.c"
    INCLUDE_DIRS "include"
    REQUIRES metalio_hw esp_driver_gpio esp_driver_i2c
    PRIV_REQUIRES metalio_hw freertos
)
