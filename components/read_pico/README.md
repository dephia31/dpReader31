# Metalio S31 板级适配

`read_pico` 仅保留RC90兼容接口名。本工程的设备为独立Metalio E-INK4 Plus V1.5，ESP32-S31，E0470A14-AF 34Pin。

- 共享I2C1 SDA0/SCL1，100kHz；通过`metalio_hw`管理TCA9555和TPS65185。
- GPIO2为TCA/SC7A20H共享低有效IRQ；FT6336U IRQ为GPIO5、RST为TCA P12。
- S31 LCD8bit/GDMA，参考20MHz时序与GC48/GL48/DU20；运行VCOM1200，未写永久值。
- TF为SDMMC slot0四线：CLK24/CMD25/D0–D3=20–23；无卡座CD，不自动格式化。
- SC7A20H按V1.5参考使用开漏低有效、无路由轮询；待机CTRL1=0x10，释放共享IRQ。
- GPIO6/7驱动双色前光；TCA P03驱动振动；不存在Pico的GPIO2蜂鸣器。
- 不编译Pico的Flash HPM补丁，不操作SY7636A或CW32。

旧的SY状态类型与若干未编译的调试源只用于兼容归档，不是本板硬件依据。准确构建目标及验证记录见根README、source/AGENTS.md、docs/HANDOFF.local.md。
