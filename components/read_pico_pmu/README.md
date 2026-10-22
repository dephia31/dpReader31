# Metalio S31 电源适配

本组件仅编译`metalio_power_adapter.c`。`read_pico_pmu_*`是RC90兼容API，硬件不使用CW32。

| 功能 | S31来源 |
| --- | --- |
| 电量与电压 | BQ27220，0x55，电压0x08/SOC0x2c；有效范围检查，每2秒读取 |
| 充电图标 | CX25601N，0x6b；参考CHG_STAT、EN_CHG与EN_OTG组合，读取失败显示未知 |
| UTC时间 | PCF8563，0x51；拒绝STOP/VL、非法BCD和日期；写时暂时STOP并恢复，读回确认 |
| 电源键 | TCA P17输入，后台采样、去抖，RC90短按/长按语义 |
| 音量键 | TCA P10/P11，去抖事件进入阅读翻页；睡眠前后清队列 |
| 关机 | 原厂S31电源脉冲逻辑 |
| VCOM | 参考运行1200mV；不做永久写入/出厂标定 |

正常界面由主循环调用轮询，不依赖旧Pico后台PMU快照。旧协议枚举只作为API兼容标识；闹钟、LED、CW32配置等未实现命令返回NOT_SUPPORTED。归档协议文档不能作为S31接线或充电参数依据。充电电流/电压未在此次审查中改写。
| 芯片 / Part | 跑小纸 Pico PMU 固件的 CW32L010 / CW32L010 running the Read Pico PMU firmware |
| 总线 / Bus | I2C，400 kHz，地址 `PMU_I2C_ADDR` / I2C, 400 kHz, address `PMU_I2C_ADDR` |
| 中断 / Interrupt | 经 FCA9555 扩展口（P0.2），也是浅睡唤醒源 / Routed through the FCA9555 expander (P0.2), also the light-sleep wake source |

## 许可 / License

Apache-2.0，见 [LICENSE](LICENSE)。

Apache-2.0, see [LICENSE](LICENSE).


## Pico 高电量显示 / Pico upper-SOC display

CW32按其文档中的电压表估算SOC，并在充电结束时重新定基；拔电电压松弛可能造成百分比跳变。主机不改这张表、不用电压猜充满：只有校验且有效的快照参与显示。高段使用每秒一次的五样本中位数，电源状态变化观察30秒，再按有效较低读数逐步收敛；轮询频率不改变下降速度。低SOC、低压和真实充满状态立即采用原值；长时间缺读或重启从有效原值重新定基，不累计离线下降。原始快照和安全保护不经显示滤波，Metalio电量计不使用此算法。它稳定估算显示，不能代替实际电流计或电池放电校准。

CW32 estimates SOC from its documented voltage table and rebases after charging; unplug relaxation may therefore jump percentages. The host keeps that table and accepts only valid checked samples, without inferring full from voltage. Upper-range display uses a five-sample median at one-second intervals, observes power changes for30seconds and then converges toward lower readings by time. Poll frequency cannot accelerate descent. Low SOC/voltage and confirmed-full samples bypass smoothing; gaps/restarts rebase without offline descent debt. Raw diagnostics/safety remain untouched, and Metalio's gauge does not use this filter. Display stability is not a replacement for current measurement or discharge calibration.
