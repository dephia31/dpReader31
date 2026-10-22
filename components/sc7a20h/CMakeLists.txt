# 加速度计驱动。S31 的共享 INT1 轮询助手需要板级宏，因此依赖 kiiko_board。
# Accelerometer driver. The S31's shared-INT1 polling helper needs the board macros, so this
# component depends on kiiko_board.
set(kiiko_selected_sources "sc7a20h.c")
idf_component_register(
    SRCS ${kiiko_selected_sources}
    INCLUDE_DIRS "include"
    REQUIRES kiiko_board esp_driver_gpio esp_driver_i2c
    PRIV_REQUIRES esp_timer freertos esp_hw_support
)
