# AGENTS.md

面向 AI agent 的仓库入口。人类也可读。产品概述、硬件表、引脚、编译命令见 [README.md](README.md)。

Agent entry for this tree. Humans can read it too. Product overview, hardware, pinout and flash commands live in [README.md](README.md).

新人（人或 agent）上手看 [docs/ONBOARDING.md](docs/ONBOARDING.md)；接手进行中的任务先看 [docs/HANDOFF.md](docs/HANDOFF.md)。

Newcomers (human or agent): read [docs/ONBOARDING.md](docs/ONBOARDING.md). Taking over an in-flight task: read [docs/HANDOFF.md](docs/HANDOFF.md) first.

## 当前工作：双板型本地整合 / Current work: local multi-board integration

用户2026-10-18明确要求同一源码选择pico/metalio47编译，取代两套独立维护应用源码的要求。共享阅读/UI保留同一份文件，平台驱动由boards选择；不得把S31供电/波形/引脚/分区用于S3。用户2026-10-25明确要求将Metalio-S31-kiikoread仓库公开，取代此前源码私有的限制；main维护S31源码，沿用现有分文件许可和第三方权利。官网继续按白名单发布已验证固件与安装资源，源码公开不改变官网部署范围。
The user explicitly requested one source tree for pico/metalio47 on 2026-10-18. Shared reader/UI code lives once; boards select platform drivers. Never apply S31 power, waveforms, pins or partitions to S3. On 2026-10-25 the user explicitly requested making Metalio-S31-kiikoread public, superseding the earlier private-source restriction. Main maintains S31 sources with existing per-file licenses and third-party rights. Website deployment remains an allowlist of verified firmware and installer assets; repository visibility does not broaden the website artifact scope.

构建入口tools/build_firmware.py，--board pico|metalio47|all；独立SDK/配置/构建/输出目录。芯片、项目名和分区必须匹配。Metalio原始GC48/GL48/DU20不得手改，供电和VCOM保留既有顺序；不得写永久TPS OTP或ESP eFuse。用户2026-10-19已明确授权直接调试连接的S31实现300ms，可按原MAC/精确分区/启动槽/已验证回退备份保护流程刷写本地调试与交付应用；该授权不包括其他设备、清数据或发布。调试USB命令入口不得留在最终交付固件。
Use tools/build_firmware.py with --board pico|metalio47|all and separate SDK/config/build/output directories. Validate chip/project/partitions. Preserve Metalio's raw GC48/GL48/DU20, power/VCOM sequencing, and never write permanent TPS OTP or ESP eFuses. On 2026-10-19 the user explicitly authorized direct debugging of the connected S31 toward 300ms, including local diagnostic/delivery application writes guarded by the approved MAC, exact partitions/boot slot and verified recovery backup. This does not authorize other devices, erasing data or publication. Remove the diagnostic USB command reader from the final delivery firmware.

## 构建 / Build

双板型统一入口是 `tools/build_firmware.py --board pico|metalio47|all`，先读 [docs/BOARDS.md](docs/BOARDS.md)。必须使用独立SDK/配置/依赖锁和产物目录；共享功能修改主源码，生成的设备视图不可作为维护源码。已有主机测试用 `board_source_view.py --tests` 生成指定设备视图后运行；CI分别选择Pico和S31，使用匹配SDK并隔离构建。
Use the board build driver and isolated SDK/config/lock/artifact directories. Edit maintained sources, not generated device views. Legacy host tests run in the board test view. CI separately builds Pico and S31 with matching SDKs and isolated outputs.

Pico需要 ESP-IDF v6.1。`components/read_pico/read_pico_flash_hpm.c` 依赖 v6 才有的 `esp_flash_chips/spi_flash_override.h`。

Requires ESP-IDF v6.1. `components/read_pico/read_pico_flash_hpm.c` needs `esp_flash_chips/spi_flash_override.h` (v6 only).

```
idf.py set-target esp32s3
idf.py build
```

- `sdkconfig.defaults`：真机 120 MHz flash / PSRAM 时序，依赖板上 Zbit 型号。/ Board timing for 120 MHz flash / PSRAM; depends on the Zbit part.
- `sdkconfig.ci`：默认时序，只验证能否编过。/ Default timing; compile-only CI check.

不要在没有对应 flash 的机器上把 CI 配置当产品默认。/ Do not ship the CI timing as the product default.

### 版本号与本地测试 / Versions and local tests

用户2026-10-29修订，取代2026-10-06的同版本测试规则：每次交付新的测试构建必须自动递增测试版本，不得沿用当前安装或上一测试构建的版本号。以当前正式版为基线，下一正式版的基础号递增一次并加大写 `RC1`，后续同一正式版候选依次为 `RC2`、`RC3`，直到发布去掉 RC 的正式版。例如 `1.16` → `1.17RC1` → `1.17RC2` → `1.17`；本仓库 `1.1.6-vn` → `1.1.7RC1-vn` → `1.1.7RC2-vn` → `1.1.7-vn`。下一次测试需核对当前版本、已有 `Test Build/` 产物和板型元数据，选择尚未使用且递增的 RC；同一测试构建内的编译重试不另算新交付。同步板型元数据、应用内版本描述和设备版本显示，并从最终 Bin 读取确认版本；不得仅改文件名。正式发布时同步设备、刷机页、升级包与日志。
Per the user's 2026-10-29 revision, superseding the 2026-10-06 same-version test rule, automatically increment the test version for every newly delivered test build; never reuse the currently installed or previous test version. Advance the current official release's base number once for the next official release and append uppercase `RC1`; subsequent candidates use `RC2`, `RC3`, and so on until the official release drops RC. Examples: `1.16` → `1.17RC1` → `1.17RC2` → `1.17`; in this repository, `1.1.6-vn` → `1.1.7RC1-vn` → `1.1.7RC2-vn` → `1.1.7-vn`. Check the current version, existing `Test Build/` artifacts and board metadata before choosing the next unused RC. Compilation retries within one test delivery do not consume another RC. Synchronize board metadata, the embedded app descriptor and device version display, then verify the version read from the final Bin; renaming alone is insufficient. On official publication, synchronize device, flasher, upgrade image and changelog.

用户2026-10-29指定：本地测试固件统一导出到仓库根目录 `Test Build/`，每次仅以固件版本号命名独立子目录（例如 `Test Build/1.17RC1/`，本仓库当前为 `Test Build/1.1.7RC1-vn/`），不加日期、板型或测试说明，并附 SHA-256 校验值。中间编译文件仍放在被忽略的构建目录。测试应用文件名须体现实际板型（例如 `dpReader_metalio47_firmware.bin`），不再要求使用 `Pico-update.bin`。`Test Build/` 中的测试产物不提交、不发布；禁止用同版本测试二进制覆盖 `flash/` 中已发布的固定版本下载。RC编号的本地测试已获授权；联网 OTA 部署和正式发布仍需用户指示。
Per the user's 2026-10-29 decision, export local test firmware to the repository-root `Test Build/` folder, using a separate subdirectory named only with the firmware version (for example, `Test Build/1.17RC1/`; currently `Test Build/1.1.7RC1-vn/` in this repository), without a date, board or test-purpose suffix, and include SHA-256 checksums. Keep intermediate compilation files in ignored build directories. Test application filenames must identify the actual board (for example, `dpReader_metalio47_firmware.bin`); `Pico-update.bin` is no longer required. Do not commit or publish test artifacts from `Test Build/`, or replace immutable published downloads in `flash/` with same-version tests. Local RC-versioned tests are authorized; online OTA deployment and official publication still require user direction.

每次本地测试构建必须随包生成 macOS `FLASH_USB.command`（可执行）及 Windows `FLASH_USB.bat`，双击即可安装同目录中已校验的本板应用，允许指定串口。macOS优先使用已有兼容ESP-IDF Python环境，支持 `/dev/cu.usbmodem*`，不得只生成Windows启动器。包含必要的辅助脚本与校验元数据；不得默默下载正式版代替测试包。用户2026-10-29最新修订：启动器不再执行备份；验证本地Bin、芯片/MAC及精确分区后，直接写应用与匹配的 OTA 启动选择，保留 bootloader、NVS、书籍和用户存储；失败时停止并明确报错。仅允许小范围只读分区校验，不读取整片flash、不生成 `USB Backups`；保留已有备份不删除。写入仍须使用 `--no-stub`。脚本完成写入不等于真机启动验收，仍须核对设备版本。
Every local test build must include an executable macOS `FLASH_USB.command` and a Windows `FLASH_USB.bat` launcher for the verified same-board application in that folder, with optional serial-port selection. On macOS prefer an existing compatible ESP-IDF Python environment and support `/dev/cu.usbmodem*`; do not generate only a Windows launcher. Include required helper scripts and verification metadata; never silently download a release instead. Per the user’s latest 2026-10-29 revision, launchers no longer back up flash: verify the local Bin, chip/MAC and exact partitions, then directly write only the app and matching OTA boot selection; preserve bootloader, NVS, books and user storage. Stop on errors. Only small read-only partition checks are allowed; do not read the full flash or create `USB Backups`. Retain existing backups. Writes retain `--no-stub`. A successful write does not establish device acceptance; verify the running version afterward.

用户2026-10-29要求：新的正式版本在 GitHub Release 成功发布并核对正式附件与校验值后，自动清理本地 `Test Build/` 中已被该正式版本取代的 RC 目录；如曾发布对应 RC Release/tag，也一并删除对应 RC Release/tag。仅清理版本不高于新正式版的候选版本，保留未来版本 RC、正式版和用户数据；发布失败不得清理。本规则不授权删除 Git 历史或正式版归档。
After a new official GitHub Release is successfully published and its assets/checksums verified, automatically delete superseded local RC folders under `Test Build/` and corresponding RC GitHub Releases/tags if any were published. Remove only candidates whose base version is no newer than that official release; preserve future RCs, official archives and user data. Do not clean on failed publication or delete Git history.

### 正式版发布 / Official releases

用户 2026-10-09 确认：之后每次发布正式版，都在 `wegooo-cell/read-pico-reader` 创建对应版本的 Git tag 和 GitHub Release，附上经验证的 TF/OTA 应用 Bin、完整 USB 刷机包、SHA-256 校验值和简要更新日志；所有附件来自同一次正式构建。
Per the user's 2026-10-09 decision, publish every future official firmware version with a matching Git tag and GitHub Release in `wegooo-cell/read-pico-reader`. Attach the verified TF/OTA application Bin, complete USB flash bundle, SHA-256 checksums and brief release notes, all from the same official build.

GitHub Release 与官网刷机、TF 下载和联网 OTA 同步发布。保留官网现有刷机清单、固件及版本下载地址；OTA 清单继续使用固件允许的官方 HTTPS 域名。核对三种交付的版本、大小、校验值和基础包兼容性，避免影响正在刷机或升级的用户。本地测试仍只生成本地包，不创建正式 Release。
Publish GitHub Releases alongside the web flasher, TF downloads and online OTA. Preserve existing website manifests, firmware and versioned download URLs; keep OTA URLs on the official HTTPS origin accepted by the firmware. Verify matching versions, sizes, checksums and base-layout compatibility across the deliveries so ongoing flashes and updates remain valid. Local tests remain local bundles without official Releases.

用户2026-10-21要求每次保留上一正式版的官网刷机与TF下载入口。替换正式清单前，使用tools/archive_current_release.py归档原Release的四件套和校验值；releases.json保留历次归档，manifest.previous_version指向上一版，发布工具校验该入口存在。不得以本地同号测试包代替原正式字节。TF包是完整应用镜像；引导程序、分区表及用户存储不由TF升级写入，整机四件套继续通过USB安装。
The user requires a previous-release web/TF entry for every publication. Archive the original Release parts with tools/archive_current_release.py before replacing metadata; retain earlier releases.json entries and verify manifest.previous_version. Never substitute a same-version local test. TF updates use a complete application image; bootloader, partition table and user storage remain untouched, with full base installation through USB.

### dpReader Metalio 4.7 (ESP32-S31) 发布流程 / dpReader Release & Web Flash Workflow

dpReader Metalio 4.7 固件与 Web Flasher 完整发布步骤详见 [docs/QUY_TRINH_COMMIT_VA_RELEASE.md](docs/QUY_TRINH_COMMIT_VA_RELEASE.md)。
Detailed commit, release and Web Flasher steps for dpReader Metalio 4.7 live in [docs/QUY_TRINH_COMMIT_VA_RELEASE.md](docs/QUY_TRINH_COMMIT_VA_RELEASE.md).
- **版本同步 / Version sync**: `boards/metalio47/board.json`, `main/ota_update.c`, `main/apps/app_device_settings.c`, `web/index.html`, `docs/CHANGELOG.md`, `metalio-s31-update.json`.
- **构建与打包 / Build & stage**: `python3 tools/build_firmware.py --board dpReader_metalio47`, copy outputs to `releases/`, update `releases/SHA256SUMS.txt` & `metalio-s31-update.json` (`notes` must stay < 384 bytes).
- **提交与标签 / Commit & tag**: commit changes, tag `vX.Y.Z-vn`, push `main` and tag `vX.Y.Z-vn` to `origin`.
- **发布 / Publish**: Run `python3 tools/publish_release.py` or trigger `.github/workflows/release.yml`. Web Flasher auto-deploys via `.github/workflows/pages.yml`.

## 目录职责 / Layout

用户2026-10-21明确指定GitHub Pages为默认OTA清单及应用来源，rc97真机已下载并校验原rc98成功。新固件优先GitHub清单、官网清单备用；原已发布rc97/98仍依照其原内置顺序请求。官网网页刷机独立保持，正式Bin及历史包继续归档GitHub Release；旧OTA不能直接使用会重定向的Release附件URL。保留双槽、HTTPS与完整SHA校验；不得因官网证书问题重现而自动改回官网应用地址，也不得外推为任意镜像或关闭TLS校验。后续更改默认来源先获得用户指示并验证旧信任库与实际下载。
The user selected GitHub Pages as the default OTA metadata/image source on 2026-10-21; a real rc97 device downloaded and verified original rc98. New builds prefer Pages metadata and retain the website fallback; released clients keep their baked-in order. Keep web flashing separate and archive original binaries/packages in Releases; legacy OTA cannot follow Release redirects. Retain dual slots, HTTPS and full SHA checks. Do not silently restore the incompatible site image origin or allow arbitrary mirrors; future default-source changes require user direction and legacy/device validation.

| 路径 / Path | 职责 / Role |
| --- | --- |
| `main/app_main.c` | 开机装配：板级 init、读设置、定 VCOM、开机图、字体，然后交给 `app_loop`。/ Boot wiring, then `app_loop`. |
| `main/app/app.h` | `app_desc_t` / `app_ctx_t` / `app_redraw_t` 契约。/ Page contract. |
| `main/app/app_loop.c` | 与页面无关的事件循环：触摸去抖、三键与菜单把手、切页、刷屏、锁屏、SD 字体延迟加载。/ Shared event loop. |
| `main/app/app_registry.c` | 菜单顺序。加页只改这张表。/ Menu table. |
| `main/app/app_config.h` | 均衡 / ALL_DU 刷新档位。/ Refresh profile. |
| `main/ui/` | `ui_kit` 排版常量与绘制原语；`ui_menu` 两层菜单。页面互不引用。/ Shared drawing; pages do not include each other. |
| `main/font/` | stb_truetype 字形缓存。`fallback.h` 是生成位图，勿手改。/ Glyph cache. Do not edit `fallback.h` by hand. |
| `main/factory/` | 设备功能自检与出厂 VCOM 标定。/ Device self-test and factory VCOM. |
| `main/display.*` `sleep.*` `settings.*` | 刷屏封装、睡眠/锁屏、NVS 设置。/ Present helpers, sleep/lock, NVS. |
| `components/read_pico/` | 板级 BSP：I2C、EPD 板定义与扫描时序、TF 卡、蜂鸣器、flash HPM。/ Board BSP. |
| `components/read_pico_search/` | 离线拼音/首字母/英文书名匹配，设备和网页共用；字表由工具生成。/ Shared offline pinyin, initials and English matching with a generated table. |
| `components/read_pico_pmu/` | CW32L010 协议主机端。线格式在 `read_pico_pmu_protocol.h`。/ PMU host. |
| `components/epdiy/` | 上游裁剪 fork（LCD 路径）。改动清单在其 LICENSE。不要为注释去改它。/ Trimmed upstream fork. Leave it alone for comment work. |
| `components/continuous_du/` | 连续 DU，触摸跟手。/ Continuous DU for finger tracking. |
| `components/cst836u/` `sc7a20h/` `fca9555/` `sy7636a/` | 芯片驱动。/ Chip drivers. |
| `components/e0470_epaper_waveform/` | 面板波形与裁剪。`waveforms/*.h` 是数据表。/ Panel waveforms. |
| `components/pwm_audio/` | PWM 音频（蜂鸣器底层之一）。/ PWM audio helper. |
| `docs/CHANGELOG.md` | 按版本简述对用户可见的功能变化与修复。/ Brief user-visible changes and fixes by version. |

功能操作见三份 README；页面契约以本文及 `main/app/app.h` 为准。/ See the READMEs for feature usage; this file and `main/app/app.h` define the page contract.

### 文档交付边界 / Documentation delivery boundary

对外交付功能实现、可复跑测试、功能说明及必要的维护契约。功能变化只在 `docs/CHANGELOG.md` 按版本轻量记录；代码演进由 Git 管理，不另建逐任务变更账本。
Deliver implementation, reproducible tests, feature documentation and necessary maintenance contracts. Summarize feature changes only in `docs/CHANGELOG.md`; Git owns code history, not per-task ledgers.

计划、检查点、过程审查、验收结果和调试日志在 `docs/local/` 或被忽略的构建目录内部闭环，不加入提交。长期有效的行为与限制整理进对应功能文档，不要求读者查内部计划。只有需要交接未完功能或责任时才按 `docs/HANDOFF.md` 提供最小必要摘要；本机账本仍不入库。
Keep plans, checkpoints, review records, acceptance results and debug logs in ignored `docs/local/` or build directories. Put lasting behavior and limits in feature documentation without dependencies on internal plans. Use `docs/HANDOFF.md` for the minimal necessary handoff of unfinished work or responsibility; the local ledger remains untracked.

### demo 页 / Demo pages

每个文件只导出 `app_desc_t`。顺序与 `app_registry.c` 一致。

One file, one `app_desc_t`. Order matches `app_registry.c`.

| 文件 / File | 演示 / Demo | 依赖 / Depends on |
| --- | --- | --- |
| `app_home.c` | 概览：I2C 普查、识别码、电池、构建时间。/ Overview. | `read_pico`, `read_pico_pmu` |
| `app_refresh.c` | GC16 / DU / 灰阶梯子与耗时。/ Refresh matrix. | `epdiy`, `e0470_epaper_waveform` |
| `app_reading.c` | 内置正文翻页。/ Reading demo. | `ttf_font` |
| `app_touch.c` | 两点跟手连续 DU、深睡唤醒。/ Touch + continuous DU. | `cst836u`, `continuous_du` |
| `app_axis.c` | 读数页 Live/Tap/Orient 与诊断页 Setup/Event/FIFO/Test。/ IMU pages. | `sc7a20h` |
| `app_power.c` | 电池与 SY7636A 只读。/ Battery + rails. | `read_pico_pmu`, `sy7636a` |
| `app_pmu.c` | CW32 协议五视图。/ Protocol views. | `read_pico_pmu` |
| `app_key.c` | 电源键原始事件。/ Power-key events. | `read_pico_pmu` |
| `app_sleep.c` | 浅睡 / 深睡 / 关机。/ Sleep modes. | `sleep`, `read_pico_pmu`, `sc7a20h` |
| `app_sd.c` | TF 卡与蜂鸣器。/ Card + buzzer. | `read_pico_sd`, `read_pico_buzzer` |
| `app_font_pick.c` | 卡上 TTF 列表与字重。/ Font picker. | `ttf_font` |
| `app_ioe.c` | FCA9555 Port-0。/ Expander pins. | `fca9555` |
| `app_selftest.c` | 设备功能自检入口。/ Device self-test UI. | `pmu_selftest` |
| `app_book.c` | TXT / EPUB 书架、目录、字号与逐书进度与 TF 阅读背景。/ TXT / EPUB shelf, TOC, size, progress and SD reading backgrounds. | `book_*`, `ttf_font` |
| `app_image.c` | 内置灰阶图与 TF 卡 JPG/PNG 图片对比。/ Built-in gray chart and SD JPG/PNG image comparison. | `book_cover`, `read_pico_sd` |
| `app_transfer.c` | 设备热点/已有 WiFi 传 TXT/EPUB，触屏/网页配网与热点二维码，离页停止；停止按钮返回进入前的位置。/ AP/STA upload with touchscreen/web provisioning and hotspot QR; stops and returns to the entry origin. | `read_pico_transfer`, `book_store` |

加页：在 `main/apps/` 新建文件，实现需要的回调，把它加入 `app_registry.c` 的 `s_apps[]`。锁屏前状态保存通过 `on_before_lock` 回调接入主循环。

To add a page: create it in `main/apps/`, implement its callbacks, and append it to `s_apps[]`. Use `on_before_lock` to flush state before lock rendering.

用户2026-10-29修订：阅读正文更靠近状态栏，底部页脚紧凑且贴近屏幕下沿；音量键使用独立保存的快捷操作，按住及去抖期间采样任务不得等待下一次IRQ。系统「Phím cứng & Nút bấm」各项通过带当前选择的菜单直接设置，不再点击轮换；提供首页、灯光、翻页/翻章、工具栏、全刷及同步等适用快捷操作。首页快捷操作直接回首页。
Per the user's 2026-10-29 revision, text sits closer to the status bar with a compact footer near the bottom edge. Volume keys use independently saved shortcuts; the sampler must keep running while held or debouncing instead of waiting for another IRQ. The system “Phím cứng & Nút bấm” entries open menus showing the current choice rather than cycling on taps, exposing applicable Home, light, page/chapter, tools, refresh and sync actions. The Home shortcut goes directly home.

## `app_desc_t` 契约 / Contract

定义在 [`main/app/app.h`](main/app/app.h)。主循环按返回值刷屏，见 [`main/app/app_loop.c`](main/app/app_loop.c) 的 `app_present()`。

Defined in [`main/app/app.h`](main/app/app.h). The loop presents via `app_present()` in [`main/app/app_loop.c`](main/app/app_loop.c).

| 返回值 / Return | 主循环做什么 / Loop does |
| --- | --- |
| `APP_REDRAW_NONE` | 不刷。/ No update. |
| `APP_REDRAW_AREA` | 只推 `area_hint()`。回调必须先画好 fb。模式是 `APP_DYNAMIC_REFRESH_MODE`（DU）。/ Push `area_hint()` only. Caller paints fb first. |
| `APP_REDRAW_PAGE` | 调 `render()`，整页 `APP_PAGE_REFRESH_MODE`（均衡档是 GL16）。/ `render()` then page mode. |
| `APP_REDRAW_FULL` | 调 `render()`；`APP_PAGE_FORCE_FULL` 时整屏 GC16，否则仍走页模式。/ `render()`; GC16 when force-full is on. |
| `APP_REDRAW_DONE` | 页面自己已经刷完。/ Page already presented. |

回调纯度：

- `render()`：纯绘制。禁止 I2C 写、蜂鸣、睡眠、改设置。KEY2 整屏强刷会复用它。/ Paint only. No I2C writes, buzzer, sleep, or settings. KEY2 reuses it.
- `on_enter()`：上电、唤醒传感器、拉一次数据。/ Power-up and first sample.
- `on_before_lock()` 可选：锁屏绘制前保存阅读进度与统计。/ Optional pre-lock flush of reading progress and statistics.
- `on_media_lost()` 可选：主循环检测已挂载卡失效时调用，先停止文件/后台消费者，不画屏、不重挂载；返回后主循环回退字体并重绘。/ Optional mounted-media-loss callback: stop file/background consumers without drawing or remounting; the loop then falls back fonts and redraws.
- `on_exit()`：掉电、停传感器。/ Power-down.
- `present()`：自定义推屏。返回 true 表示已经刷过，主循环不再推。/ Custom present; true means done.
- `on_touch()` / `on_gesture()` / `on_key()` / `on_tick()`：可有副作用，用返回值要刷屏。/ Side effects OK; return the redraw.
- `on_key_long()` 可选：仅接管三键的页面在同键单指保持500ms后触发一次；`defer_middle_short` 页面中键短按在抬起时执行，长按不触发短按；滑出、多点、读错、睡眠、字体重载或切页取消。/ Optional owned-key hold callback fires once at 500ms; with `defer_middle_short`, a middle short press fires on release and a hold skips it. Leaving, multitouch, read errors, sleep, font reload and page switches cancel.
- `on_power_short()` 可选：页面可消费电源键短按；返回 `APP_REDRAW_NONE` 时主循环继续锁屏。阅读正文的“电源键翻页”开关开启后短按翻下一页，长按由主循环锁屏；其他页面保留原电源键逻辑。/ Optional short-power callback may consume the press; `APP_REDRAW_NONE` keeps the lock path. With reader power-turn enabled, short presses advance in the reader body and long presses lock; other pages keep their usual power behavior.
- `on_media_ready()` 可选：首次插卡、显式重挂或Pico后台恢复完成时通知当前页面更新目录；不得绕过失效后的句柄清理。/ Optional callback refreshes page listings after insertion, explicit remount or Pico background recovery; it must not bypass handle cleanup after invalidation.
- `on_gesture()` 可选；提供后不再接收 `on_touch()`。主循环负责识别与全局中断取消，页面解释动作。/ Optional gesture callback replaces `on_touch`; the loop recognizes and cancels, the page interprets.
- `on_key()` 默认只收 `UI_KEY_1`；`owns_keys` 页面在菜单关闭时接收三键。主页面按用户2026-10-16修订：左右键循环切换四个主页，中键长按全刷；书架中键短按切换分页控制并沿左右箭头轮廓描边，可通过导航随时打断。系统子页仍左返回、中首页、右返回；阅读正文三键短按可自定义，两套推荐默认上一页/工具栏/下一页；底部上滑固定呼出工具栏，三键短按和中键长按不强制工具栏映射；中键长按可自定义，默认返回首页。用户2026-10-29取消阅读全屏模式：开书、恢复和翻页保持顶部状态栏及紧凑底部页脚；用户随后要求移除重复书名/返回/收藏栏，翻页只关闭工具栏；删除全屏开关和按键选项，旧全屏按键映射为工具栏，正文双击不切模式，插图双击仍缩放。/ Normally KEY1 only; `owns_keys` pages receive all three outside the menu. Product system pages use Back/Home/Back; reader short actions are configurable, defaulting to previous/tools/next; bottom-edge swipes always open the toolbar without a required short/hold mapping; middle holds are configurable and default to home. Per the user’s 2026-10-29 decision, reader full screen is removed: opening, resuming and turning retain the top status bar and compact bottom footer; the user subsequently removed the duplicate title/back/favorite row, and turns only dismiss tools. Remove the full-screen toggle and key choice; legacy bindings open tools. Text double taps do not switch modes; illustration double taps retain zoom.

标志：

- `holds_pmu`：长时间独占 PMU（自检）。主循环的锁屏按键轮询让路。/ Page owns the PMU; lock-key poll yields.
- `owns_keys`：接管三键；不影响菜单打开时的全局键和菜单把手。/ Own all three keys outside the menu; global menu keys and the handle remain.
- `defer_middle_short`：中键短按在抬起时触发，以区分长按。/ Defer middle short action until release to distinguish a hold.
- `menu_handle_enabled`：按页面状态决定右下角菜单把手是否响应。/ Control bottom-right menu-handle hit testing by page state.
- `enter_full`：进页走 `APP_REDRAW_FULL`，避免差分刷留边。/ Enter with a full refresh.

`app_ctx_t.request_app` 非空时，主循环在本轮末尾切页；`request_menu` 请求打开根菜单。同时设置时切页优先。/ At tick end, `request_app` switches pages and `request_menu` opens the root menu; a page request takes priority.

## 术语表 / Glossary

| 术语 / Term | 中文 | English |
| --- | --- | --- |
| DU | 快速差分刷新，只驱动变化像素，灰阶少、残影多。 | Fast differential update. Drives changed pixels only. |
| GC16 | 16 级灰度全像素刷新。区域内白像素先压黑再擦白，刷新处一条黑带。 | 16-gray full-pixel update. Whites go black then white; a black flash. |
| GL16 | 16 级灰度，白底保持，不先压黑。 | 16-gray update that keeps an already-white background. |
| 跟随 DU / FOLLOW DU | 开机生成的 8 帧短波形，一次刷新连 diff 带扫描，用来跟手数字或读数。 | Boot-built 8-frame short waveform: one refresh does diff + scan. Used for live digits. |
| 连续 DU / continuous DU | 每轮只扫一个相位，软件记每像素剩余相位。帧率与黑度解耦。 | One DU phase per scan; software tracks leftover phases per pixel. |
| 跟手 | 手指或传感器在动时用 DU/连续 DU 追画面。 | Live tracking with DU / continuous DU while the finger or sensor moves. |
| 定稿 | 动作停下后用 GC16/GL16 整页（或指定区）清残影。 | Settle after motion with GC16/GL16 to clear ghosting. |
| 把手 | 底栏菜单按钮；演示页的 KEY3 也可触发。打开/关闭全屏一级菜单。 | Bottom-right menu handle; KEY3 can also toggle it on demo pages. |
| 冻结 / Frozen | 文件头里的产品决策。agent 不得改行为去“优化”它。 | Product decisions in the file banner. Do not “improve” them. |
| 均衡配置 | `APP_REFRESH_BALANCED`：整页 GL16，周期 GC16 压灰底，动态区 DU。 | Default refresh profile: GL16 pages, periodic GC16, DU for motion. |
| ALL_DU | 实验档：整机都走 DU，只看速度。 | Experimental profile: everything is DU. |
| 锁屏 | 电源键短按后的锁页。2026-10-19用户改为双击电源键才唤醒，有密码时继续验证。 | Lock face after a short press; per the 2026-10-19 user decision, double-press power to wake, then verify an enabled PIN. |
| 软睡 | `SOFT_SLEEP`：PMU 拉低 EN，再短按开机，不是硬复位。 | Soft sleep: PMU drops EN; a short press powers on without a hard reset. |
| VCOM | 面板公共电压（绝对值 mV）。出厂写入 PMU，主机只读。 | Panel common voltage (absolute mV). Factory-written in the PMU; host is read-only. |

注释里不要再展开这些词。/ Do not re-explain these terms in comments.

## 硬约束 / Hard rules

- 用户 2026-10-09 确认项目品牌为小写 `kiikoread`，网页设备称「墨水屏」，页面底色采用明亮纯白。保留 `wegooo-cell/read-pico-reader` 仓库网址与已发布下载/OTA地址、备份文件名和内部协议标识。用户最新要求固件界面也统一命名，主站 OTA 使用 kiikoread.com，并保留 GitHub 回退源。/ Per the user’s 2026-10-09 decision, use lowercase `kiikoread` for the project and “墨水屏” for devices on the bright-white website. Preserve repository/download/OTA URLs, backup filenames and protocol identifiers. The latest instruction also rebrands the firmware UI, with kiikoread.com as the primary OTA host and GitHub retained as fallback.

`冻结 / Frozen:` 段落是产品决策，不是建议。改行为前必须先改这段，并说明为什么决策变了。

A `冻结 / Frozen:` block is a decision, not a hint. Change the behavior only after rewriting that block and saying why the decision changed.

- 面板 VCOM 存在 PMU 里。主机开机读一次，不写 ESP NVS，产品页不提供修改入口。标定页（`vcom_setup.c`）是未标定时的出厂拦住路径。/ VCOM lives in the PMU. Host reads once. No NVS copy. No product UI to edit it. `vcom_setup.c` is the factory gate when unset.
- 不要写 SY7636A 的 VCOM / VLDO / 放电 / 延时 / VCOMCTL。电源页只读。/ Do not write SY7636A VCOM / VLDO / discharge / delay / VCOMCTL. Power page is read-only.
- `sdkconfig.defaults` 的 120 MHz 时序依赖真机 Zbit flash。覆盖 HPM 表见 `read_pico_flash_hpm.c`。/ 120 MHz timing needs the onboard Zbit part. HPM override is `read_pico_flash_hpm.c`.
- 不要改 `components/epdiy/**` 来“整理注释”。/ Do not touch `components/epdiy/**` for comment cleanup.
- 不要手改 `main/font/fallback.h`、`main/font/stb_truetype.h`、`components/e0470_epaper_waveform/waveforms/*.h`。/ Do not hand-edit generated or vendored tables.
- 不改 UI 字面量或日志字符串，除非任务明确要求。/ Do not change UI strings or log text unless asked.

## 注释规范 / Comment style

### 授权声明 / License notices

用户 2026-10-15 要求新增原创内容仅限个人学习、不得未经书面授权商用。适用边界以根目录 [LICENSE](LICENSE) 和 [CONTRIBUTING.md](CONTRIBUTING.md) 为准；这是今后明确标注的新原创内容的许可，不撤销 rc93 及以前的授权，不覆盖第三方或其他贡献者的权利。新原创文件只有在权利人明确同意时使用 `LicenseRef-Kiikoread-Personal-Study-1.0`，保留真实版权人。不要将下面的 Apache 文件头示例机械用于新原创文件，也不要批量替换既有文件头。
Per the user's 2026-10-15 decision, explicitly marked new original material is for personal study, with commercial use requiring written permission. [LICENSE](LICENSE) and [CONTRIBUTING.md](CONTRIBUTING.md) define the scope; rc93 and earlier grants and third-party or other contributors' rights remain intact. Use `LicenseRef-Kiikoread-Personal-Study-1.0` on new original files only with the rights holder's explicit consent and accurate attribution. Do not mechanically apply the Apache example below to new original files or replace existing headers in bulk.

全仓中英双语。只改注释，不改逻辑。

Every comment is bilingual. Comments only; no logic changes.

文件头：

```
/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：这个文件负责什么、边界在哪。
 *
 * English: what this file owns and where it stops.
 *
 * 冻结：不得做的事。
 * Frozen: things this file must not do.
 */
```

- 头文件公开 API 与 struct 成员用 `///`。/ Public API and struct fields: `///`.
- `.c` 内部用 `//`。不要 `/** */`。/ `.c` internals: `//`. No `/** */`.
- 单行：`// 中文。/ English.`
- 超过一行：中文一行，英文紧跟一行。/ Multi-line: Chinese line, then English line.
- `enum` 成员短尾注：`///< 中文 / English`
- 大 `.c` 用 `/* ---- 区块名 / Section ---- */` 分区。/ Large `.c` files use section banners.
- 术语见表，不在注释里重复解释。/ Use the glossary; do not redefine terms.

每个正式版本同时保留USB全量包与TF升级包，官网按所选版本分别提供下载入口。TF应用Bin按实际板型命名，ZIP可附说明与校验值；不得将USB全量包当作TF升级镜像。
Retain both USB full base packages and TF application upgrades for each official release, with separate version-matched website downloads. Name TF application binaries for their actual board; ZIPs may include instructions/checksums. Never present a USB base bundle as a TF upgrade image.

用户2026-10-21明确要求修复官网OTA，不切换GitHub固件镜像。OTA固件URL保持https://kiikoread.com/；备用清单不能自行更换固件下载源，证书链问题须在官网或经确认的兼容固件中修复，保持TLS/SHA校验。
The user explicitly requires the official OTA origin, not a GitHub firmware mirror. Keep firmware URLs on https://kiikoread.com/; do not change the download origin through fallback metadata. Repair certificate compatibility on the site or in approved firmware while retaining TLS and SHA checks.

发布前及证书续期后按 `docs/OTA_MAINTENANCE.md` 运行原正式固件信任链、完整下载和Range检查，并用已发布设备验收；浏览器可用不能代替设备校验。临时Cloudflare令牌不入仓库。
Before publication and after certificate renewal, run the released-trust-store, download and Range checks in `docs/OTA_MAINTENANCE.md`, plus a released-device acceptance test. Browser success is insufficient; never commit temporary Cloudflare credentials.
