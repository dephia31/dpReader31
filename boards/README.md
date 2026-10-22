# 板级选择层 / Board selection layer

这个目录把「同一份代码、两块板、两个 bin」所需的一切板型差异集中在一起。
`idf.py -DKIIKO_BOARD=<板名>` 在 `project()` 之前选中 `boards/<板名>/board.cmake`，
由它决定芯片目标、独立 sdkconfig、分区表、工程名与板级编译宏。

This directory collects every board-specific difference needed to build two images from one
source tree. `idf.py -DKIIKO_BOARD=<name>` selects `boards/<name>/board.cmake` before `project()`;
that file decides the chip target, the per-board sdkconfig, the partition table, the project name
and the board compile macros.

## 现有板型 / Available boards

| 板名 / Board | 芯片 / Chip | 工程名 / Project | 固件 / Artifact | sdkconfig |
| --- | --- | --- | --- | --- |
| `pico` | ESP32-S3（Xtensa） | `Read_Pico` | `Read_Pico.bin` | `sdkconfig.pico` |
| `metalio_s31` | ESP32-S31（RISC-V） | `Kiikoread_Metalio_S31` | `Kiikoread_Metalio_S31.bin` | `sdkconfig.metalio_s31` |

两块板共用同一个 ESP-IDF v6.1 工具链（S31 需要带 `esp32s31` 支持的提交，见根 README）。

Both boards share one ESP-IDF v6.1 toolchain; the S31 needs the commit that carries
`esp32s31` support, see the root README.

## 目录约定 / Layout

```
boards/<board>/
  board.cmake            # project() 之前：KIIKO_PROJECT_NAME/VER/ENV、IDF_TARGET、
                         # KIIKO_BOARD_DEFINES、SDKCONFIG 与 SDKCONFIG_DEFAULTS
  kiiko_board_config.h   # 板级身份常量 + 硬件能力宏（共享代码 #if 的开关）
  board_hw.c             # kiiko_board.h 门面里本板独有的实现（当前只有身份与空 extras）
  sdkconfig.defaults     # 板级 SDK 覆盖项，追加在根 sdkconfig.defaults 之后
  partitions*.csv        # 分区表（OTA 槽大小按板）
  sdkconfig.ci           # （pico）CI 只验编译用的默认时序
```

- 根 `sdkconfig.defaults` 放两板共性；板级文件在后、因此板级值优先。
- `sdkconfig.<板名>` 落在仓库根、入库，保证两块板的产物配置可复现；`sdkconfig` 本身不再使用。
- C 侧统一 `#include "kiiko_board.h"`：它包含选中板的 `kiiko_board_config.h`，并给能力宏兜底。
- 板级宏必须是 `0`/`1`；写错在编译期就断，而不是在真机上表现异常。
- `KIIKO_BOARD_METALIO47` 沿用上游 rc99 已在 `app_loop.c` 使用的名字，S31 分支一律用它。

The shared root `sdkconfig.defaults` holds the common half; the board file is appended after it and
wins. `sdkconfig.<board>` lives at the repository root and is committed so each artifact's
configuration is reproducible; the plain `sdkconfig` is no longer used. C code includes
`kiiko_board.h`, which pulls in the selected board's `kiiko_board_config.h` and supplies fallbacks
for every capability macro. Capability macros must be `0` or `1`. `KIIKO_BOARD_METALIO47` keeps the
name upstream rc99 already branches on in `app_loop.c`.

## 加一块板 / Adding a board

1. 复制一份最接近的 `boards/<新板>/`，改 `board.cmake` 的工程名、版本、`IDF_TARGET`、
   `KIIKO_BOARD_DEFINES` 与分区表。
2. 在 `kiiko_board_config.h` 里填身份常量（NVS 命名空间、镜像 chip_id、缓存目录）与能力宏。
3. 自己的 sdkconfig defaults 与分区表；`sdkconfig.<新板>` 首次构建会自动生成。
4. 板级驱动：要么在 `board_hw.c` 里实现 `kiiko_board.h` 的门面，要么像现有两板一样
   提供同名 BSP 组件，并在组件 `CMakeLists.txt` 里按 `KIIKO_BOARD` 选源文件。
5. `main/CMakeLists.txt` 与相关组件的 `CMakeLists.txt` 里补上新板的源文件分支。

Copy the closest `boards/<new>/`, change the project name, version, `IDF_TARGET`,
`KIIKO_BOARD_DEFINES` and partition table; fill identity constants and capability macros into
`kiiko_board_config.h`; add sdkconfig defaults and a partition table. Board drivers either
implement the `kiiko_board.h` facade in `board_hw.c` or, like the two current boards, ship a
same-named BSP component and pick sources by `KIIKO_BOARD` in its `CMakeLists.txt`. Finally add the
new board's source branches to `main/CMakeLists.txt` and the affected components.
