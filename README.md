# Metalio S31 kiikoread（私有协作测试工程）

独立适配 Metalio E-INK4 Plus V1.5 / E0470A14-AF 34Pin，ESP32-S31，16MB Flash / 16MB PSRAM。不能与 Pico S3 固件混刷。

当前版本：**`0.3.3-rc90-metalio-test36`**。包含 test31～test36 的字体事务、下拉菜单、书架/设置刷新、Moku 电源与防淡墨修改；设置页参考已发布的 RC96。完整说明见 [更新记录](docs/S31_CHANGELOG.md)。

## 构建

使用支持 S31 的 ESP-IDF6.1 提交 `fff9895c82d744c7237be8847347bdd1b07c6643`，初始化相应子模块、安装匹配工具链并载入 `export.sh` 后执行：

```sh
idf.py -B build-s31-native -D IDF_TARGET=esp32s31 -D SDKCONFIG_DEFAULTS=sdkconfig.defaults -D METALIO_LOCAL_LATENCY=OFF build
```

依赖版本由 `dependencies.lock` 固定。不要用 Pico 的 SDK、引脚或分区覆盖本项目。构建输出项目身份为 `Kiikoread_Metalio_S31`，测速命令默认关闭。

## 固件下载与更新

下载 [test36 私有测试固件](https://github.com/wegooo-cell/Metalio-S31-kiikoread/releases/tag/v0.3.3-rc90-metalio-test36)。应用大小 4,788,464 字节，SHA256：

`f188b2c53ac9986167cb8856904d1643bbf786edf53b50f4b09c4788adde75c0`

已有本项目的设备优先将 `Metalio-S31-update.bin` 放到 TF 卡，使用设备本地升级。USB 应用更新必须核对芯片、项目、分区及实际启动槽；应用不能写入 0x0。完整 16MB 镜像仅用于同板型首次安装，从 0x0 写入，会覆盖原系统与内部设置。TF 文件不在镜像中。

共享包保留本地已验证 Bin 原字节，仅整理说明和元数据。此仓库不包含设备身份、原机恢复备份、NVS/凭据、个人字体、书籍、串口日志或绑定单台设备的电脑刷机记录。

## S31 硬件约束

完整原厂 GC48 / GL48 / DU20 数组保留，默认完整48相、裁剪关闭。保留 8bit / 20MHz LCD、192行预填、12ms最短相位、XOE 关闭至少12us、TPS65185、FT6336U、GPIO36 / USB、SDMMC、电池 / RTC、前光与振动。VCOM 默认3000mV来自 Moku 的对比测试，并非本台设备实测标定；已有保存值优先，只写运行寄存器，不烧永久 OTP。详见 [硬件说明](docs/S31_HARDWARE.md)。

## 验证与限制

test36 的 S31 完整编译、11组宿主回归及33项本地刷机保护模拟测试通过。构建输入哈希见 `docs/SOURCE_SHA256.json`，验证摘要见 `docs/TEST36_VALIDATION.json`。本轮未刷机，未完成强光保持、光学残影、实际响应时间、BLE、USB-MSC、长时间锁屏或功耗验收；不承诺所有切换达到300ms。

```sh
export METALIO_IDF_PATH="$IDF_PATH"
bash tools/run_metalio_host_test.sh
bash tools/run_metalio_ota_host_test.sh
bash tools/run_water_turn_host_test.sh
bash tools/run_display_host_test.sh
```

其余测试及当前执行清单在 `docs/TEST36_VALIDATION.json`；部分历史夹具需要原厂对照源码或已解析的 managed_components，不等于全部旧 Pico 板级测试均适用 S31。

Wi-Fi 传书速度仍待解决：之前 STA 实测约30.2秒/1MiB，接收等待约29.8秒、写卡约0.13秒，关闭蓝牙没有明显改善。S31 只支持2.4GHz Wi-Fi6，不能连接5GHz。

仅作私有协作测试，非 Metalio 或 Pico 官方发布。第三方许可见 `THIRD_PARTY_NOTICES.md` 与 `licenses/`，保留各文件原有 SPDX。
