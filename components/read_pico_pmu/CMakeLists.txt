idf_component_register(
    SRCS "metalio_power_adapter.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_i2c metalio_hw esp_timer
    PRIV_REQUIRES freertos esp_rom
)
