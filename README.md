# Metalio S31 kiikoread（私有测试工程）

独立适配 Metalio E-INK4 Plus V1.5 / E0470A14-AF 34Pin，ESP32-S31、16MB Flash/PSRAM。不是 Pico S3 固件，不可混刷。

版本：`0.3.3-rc90-metalio-test30`，同步已验证的 RC95 与后续阅读排版、导航和透明PNG锁屏修改；保留 S31 前光、振动、FT6336U、GPIO36/USB、TPS65185、电池/RTC、SDMMC 和独立分区。

## 构建

使用 ESP-IDF6.1 的 S31 支持提交 `fff9895c82d744c7237be8847347bdd1b07c6643`，安装对应工具链后载入其 `export.sh`，在本目录执行：

```sh
idf.py -B build-s31-native -D IDF_TARGET=esp32s31 -D SDKCONFIG_DEFAULTS=sdkconfig.defaults -D METALIO_LOCAL_LATENCY=OFF build
```

原始 GC48/GL48/DU20 表默认完整使用（`E0470_WAVEFORM_TRIM=1` 才回退官方 GC36/GL37 裁剪，对齐厂家 2.0.76）；8bit/20MHz、192行预填、12ms最短相位。VCOM 实测最优 **3000mV（-3.00V）**，开机按住音量键进标定页可调（900–4000mV），值存 NVS，不烧写永久标定。

## 本地更新

当前独立分区有5MiB双槽。仅应用Bin不能写到0x0；应用更新必须核对S31项目、分区和实际启动槽。完整0x0镜像仅用于首次安装，会覆盖内部数据，TF不在镜像中。已有设备优先使用 TF 本地升级或受保护的本机刷机工具，保留设置。

## 当前测试版的已知问题

Wi-Fi 传书速度尚未解决：同一网络实测1MiB约30.2秒，接收等待约29.8秒、写卡约0.13秒；关闭蓝牙后仍没有明显改善，直连热点对比待完成。接收合并写卡与TCP窗口调整已经加入，不能将其描述成已取得明显实机提速。当前版本用于私有协作测试。

固件下载请到本仓库的 **Releases → test30**。应用Bin SHA256：`6eef5ee68c320b1db2ded520f70f46a1dbae652e6798f6b6beac996957673247`。

## 验证范围

阅读/排版、PNG合成、锁屏PIN失败保护、S31浅睡/深睡事务、USB路由与显示失败恢复的主机回归和S31编译已通过。真实光学画质、长时间休眠、功耗、BLE翻页器及传输速度应按设备验证结果验收；不承诺全部切换在300ms内。S31只支持2.4GHz Wi-Fi6，不支持5GHz。

四项S31必要主机回归：

```sh
export METALIO_IDF_PATH="$IDF_PATH"
bash tools/run_metalio_host_test.sh
bash tools/run_metalio_ota_host_test.sh
bash tools/run_water_turn_host_test.sh
bash tools/run_display_host_test.sh
```

其余工具保留上游与移植测试，其中部分旧Pico板级测试不适用于S31。验证范围摘要在 `docs/TEST30_VALIDATION.json`。

此私有仓库不包含设备整机备份、NVS/凭据、书籍、个人字体、串口日志或本机操作记录。第三方许可见 `THIRD_PARTY_NOTICES.md` 与 `licenses/`，源码按各文件SPDX保留。
