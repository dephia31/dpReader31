# Metalio S31 · kiikoread

本仓库为 **Metalio E-INK4 Plus 4.7 V1.5 / E0470A14-AF 34Pin / ESP32-S31** 私有固件工程。采用同一份双板维护源码，Metalio 的驱动、波形、前光、振动、USB、睡眠和分区由 `boards/metalio47/` 独立选择。

Private firmware for Metalio E-INK4 Plus V1.5. One maintained application tree selects the board-specific Metalio backend with `--board metalio47`.

## 当前本地测试版 / Current local build

- 固件版本 / Firmware: **0.3.3-rc97**，2026-10-21 本地测试。
- 原始维护源码 / Maintained source: `b3bfa555a01a0b92ff14843c1b728fcd3e16c4b5`。
- [下载应用 Bin、USB 包、精确源码和校验值 / Downloads](https://github.com/wegooo-cell/Metalio-S31-kiikoread/releases/tag/v0.3.3-rc97-metalio-local-b3bfa55-20261021)。
- 应用 / Application: `Kiikoread_Metalio_S31`，芯片 / chip `ESP32-S31`，双 5 MiB 应用槽。
- Bin SHA-256: `5a171e8d9467243990ac655c29c8d144f0cfa44d4c592f25539699a02ff6cb7a`。

包含普通/水波纹四级22相文字刷新、安全192行备行、未变化白底保持；常驻灰底栏和与阅读共用判定的右滑返回；透明书封读取优化、锁屏资源失败白底圆锁，以及阅读下拉背景文字轻微模糊。保留前光/滑块和振动、异常启动字体保护等S31功能。

Includes short-gray text turns, safe scan prefill, unchanged-white holding, persistent gray navigation, reading-style right-swipe back, fused transparent-cover reads, white-lock fallback and slightly softer reader-menu glass. S31 hardware and recovery behavior remain board-specific.

S31完整编译和相关宿主回归通过。该快照的实机触控、光学、休眠恢复和实际生成耗时仍待验证。旧锁屏缓存会重建一次。静置防褪色重刷默认5分钟，本次上传保留此逻辑。

Native S31 compilation and relevant host regressions passed. Hardware optics, touch feel, sleep/resume and latency for this snapshot still require validation. Old lock-art caches rebuild once; the default five-minute anti-fade refresh is retained.

## 构建 / Build

准备支持 `esp32s31` 的匹配ESP-IDF 6.1及工具链，详见 [双板构建 / Board guide](docs/BOARDS.md)。以下参数复现当前本地测试的PIE/短灰阶配置：

```sh
python tools/build_firmware.py --board metalio47 --s31-pie --s31-text-ripple \
  --idf-metalio47 /path/to/esp-idf-s31 \
  --tools /path/to/idf-tools \
  --python /path/to/idf-python/bin/python \
  --output build/metalio-local
```

产物在 `build/metalio-local/packages/metalio47/`。应用Bin用于已有匹配S31分区的更新；完整USB包内有该板的地址、镜像与校验值。保留现有数据时使用匹配分区/启动槽的应用更新流程。

Application images update an existing matching S31 layout; the full USB bundle contains the board-specific image addresses and checksums. Use the matching-layout application update flow when retaining device data.

共享项目的完整功能与维护说明见 [原维护入口 / Shared upstream guide](README.upstream.md)、[更新日志 / Changelog](docs/CHANGELOG.md) 和 [许可证 / License](LICENSE)。
