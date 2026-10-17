# Metalio S31 FT6336U 兼容组件

本工程实际构建 `metalio_ft6336.c` 和 `metalio_touch_decode.c`，控制器是 FT6336U（地址0x38、ID0x64），`cst836u_*`仅为原阅读器API兼容名，不是这台设备的芯片型号。目录下旧示例没有参与S31构建。

- 共享I²C由`metalio_hw`管理，GPIO5独立低有效IRQ，TCA9555复位。
- 原厂IRQ+feed：空闲等待下降沿；触点、低IRQ或读取失败每10ms采样。IRQ初始化失败退回轮询，不让界面失去输入。
- 固定684×1216竖屏坐标。按原厂b400605取消Y偏移；保留双指、边缘夹值与盖板原始坐标。
- 保留跨刷屏短按缓存；采样/模式/复位串行，睡眠后不访问触控总线。
- A3/9F/A6仅只读识别。没有开启触控Flash擦写；供应商V03仅声明用于LT0470GF0430M_V1.1，不能只凭电子纸型号自动升级。

参考：https://github.com/CloudZao/Metalio-E-INK4-Plus/commit/b400605e9a6a276a2c39864c0cff63798c78e8d8
