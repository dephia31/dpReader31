# 双板型构建

维护同一份 `main/` 阅读器和 UI 源码，通过 `KIIKO_BOARD` 选择设备。`boards/<设备>/` 保存该设备的配置、分区和硬件后端，`cmake/board.cmake` 选择参与编译的驱动。已有 `read_pico_*` 接口名继续作为内部兼容接口使用。

## 支持的设备

| 编译选项 | `pico` | `metalio47` |
| --- | --- | --- |
| 设备 | 原 kiikoread 墨水屏 | Metalio E-INK4 Plus 4.7 V1.5 |
| 芯片 / 编译器 | ESP32-S3 / Xtensa | ESP32-S31 / RISC-V |
| 固件项目名 | `Read_Pico` | `Kiikoread_Metalio_S31` |
| 触摸 | CST836U | FT6336 |
| 屏幕供电 | 原 CW32 / SY7636A 路径 | TPS65185 / TCA9555 |
| 波形 | 原 Pico 校准序列 | 原 GC48 / GL48 / DU20 |
| 应用分区 | 双 4 MiB | 双 5 MiB |
| bootloader 地址 | `0x0` | `0x2000` |
| OTA 数据地址 | `0xd10000` | `0xd000` |
| 设置命名空间 | `read_pico` | `metalio_s31` |
| 锁屏 / 启动记录空间 | `pico_lock` / `pico_startup` | `metalio_lock` / `metalio_start` |
| 联网 OTA | 沿用正式版 | 沿用 Metalio 本地版的禁用策略 |

Metalio 仅适用于已适配的 **V1.5 / E0470A14-AF 34Pin** 硬件，不自动推断其他硬件修订。两种设备的原始波形、供电时序、引脚、存储地址及默认配置分别保留；共享功能更新不代表把一块板的底层参数搬到另一块板。

两块板共用rc97的凭据读取和紧凑启动记录逻辑：正常可读的已设密码仍验证；读取失败或损坏时不拦启动、可重新读取且禁止覆盖旧凭据。启动记录兼容旧定长格式，空间不足只回收自身旧记录。S31异常启动字体回退、USB交接、前光与睡眠恢复由自身后端保留。

Metalio设置主列表及文件管理内容按黑白目标使用DU20，固定底栏保留已显示像素，手动全刷与故障恢复仍走完整路径；设置子页图片预览保留灰阶。首页继续阅读卡片纯白，七款时钟在两种板型共用可释放底图缓存，覆盖首页、样式预览及放大时间页。两槽最多231840字节PSRAM，始终预留2MiB；IKEA/小屋数字使用当前系统字体，Cat眼睛和Kehai圆环/圆点仅重绘局部，日期/分钟/方向/黑白模式和尺寸变化按需失效；内存不足直接绘制，进入阅读前释放。普通灰阶切页的备行等待仅在非传输且无欠载时缩减，欠载后本次开机回退保守余量。

Metalio普通主页使用定向黑白端点和完整中灰相位，周期整理与底栏横条共享扫描；阅读、水波纹、手动全刷与失败恢复保留各自路径。当前普通灰阶48相、每相至少12ms，仅相位节拍已需576ms，另有供电、排版与传输开销；不能用CPU或内存规格推断整页能在300ms完成。串口的`SWITCH`日志区分退出、字体、进页、推屏和总耗时，`main refresh`另记录供电及显示总耗时；这些是固件计时，最终光学稳定时间仍需实机测量。

S31的PIE只在Core1可用。较大的差分作业在扫描空闲时整帧提交给现有Core1供屏任务，前台同步等待像素、脏图与前导统计全部归还；小区域、无工作核或资源不足保留原路径，不迁移主任务。超时先请求取消并收回缓冲后回退，无法归还时停止而不留下后台写入。默认构建使用C算子，显式加`--s31-pie`才在Core1的16字节对齐完整块使用128位PIE，Core0、未对齐和尾部保持C。`METALIO_LOCAL_LATENCY=ON`仅供本地整帧oracle及开关对照，交付构建必须关闭；微基准不能作为整机倍数。

S31实际显示输出与SDK Flash操作共用递归锁，完整扫描和断轨结束后才允许Flash停顿；准备封面/绘制期间不持有此锁。主页面绘制可与TPS的50ms唤醒等待重叠；只提前WAKEUP，PWRUP/VCOM保持低，实际扫描仍执行原厂后续顺序，取消或失败立即下电，不保压等待用户。供屏在原1KiB查表内预置各像素位，空闲普通/黑白切页保留已验证的128行预填，传输、临时对时/扫描、BLE活动和欠载保持192行；扫描仍为20MHz、原GC48/GL48/DU20与至少12ms。S31内部DMA双缓冲各64行，以64字节边界分成不超过4032字节的描述符，每个缓冲只在末段通知EOF，保留约1ms补数余量且不改变面板行时序。DMA EOF提高中断优先级并过滤准备/停止期的旧VSYNC；按SDK启用RGB ISR缓存关闭安全选项，ISR读取的行队列指针表固定在内部RAM且只按指针大小分配。旧S31构建缓存由构建工具迁移这两项安全配置，其余配置保留。未完成扫描仍报错并走原恢复。

书架最终灰阶封面复用既有动态4MiB LRU，不增设整页常驻副本。键包含源像素校验、尺寸、旋转、对比度与4×4点阵相位；仅复制精确对齐的物理行，收藏、边框和文字实时画，未对齐或内存不足直接绘制。阅读前缩减与媒体失效清理沿用既有生命周期。首页分钟到点把状态栏和静态表盘合并为一次真实差分，输出失败不确认已显示。

## S31普通与速读翻页

阅读设置的“翻页效果”在普通、水波纹、速读黑白间点按切换，原有0/1设置值保持兼容。所有文字路径保留安全192行备行；128行在实机普通阅读出现欠载，已撤回。TPS预唤醒与差分生成重叠，保留厂家等待和驱动顺序。

本地显式 `--s31-text-ripple` 包按用户最新要求让普通与水波纹共用四级目标（0/5/10/15）及22相。普通用一次整轮提交同步输出，不作条带动画；水波纹用8带、末两带共用启动时刻，共28拍。每个像素的有效/尾相保持，正文与进度合并，不追加第二轮页脚刷新。两种短灰阶路径均最迟十页用原厂GC48清残影，服从更短的用户间隔；面板显示有效上限。用户反馈白底闪动后，短表的白→白与其他等值迁移全保持，取消每页三次白底尾部补推；变化的旧字仍保留擦白驱动。此项针对未变化白底闪动，尚不能据此宣称简化中灰迁移的残影已解决。

常规构建强制关闭实验；禁用实验、波形不可用或本次开机扫描失败后，普通回到完整GL48及16级字缘，水波纹使用对应完整序列。这里不把22相称为完整16级等效；灰阶及残影须按实屏验收。原厂波形原数据、电压、扫描时钟及相位下限保留。

速读黑白仅在前后均为无图无背景文字页时使用本板原厂DU20原数据，目标阈值化为黑白，实际旧灰阶保留作为迁移起点；字缘更硬，正文/进度合并，最多十页GC48。所有模式的图片/背景、首次进入和手动全刷保留完整灰阶。三种效果随重启和配置备份保存；Pico仍为两种效果。

实验Bin只适用于已有kiikoread S31分区。CrossLink应用位于0x20000，与本项目0x10000不同，须走单独恢复流程；本地包不改NVS格式、不公开OTA。

English: S31 retains safe192-row prefill after a real128-row reader underrun, and overlaps TPS pre-wake with difference generation without changing vendor sequencing. The explicit local `--s31-text-ripple` build now shares one four-level22-phase table between Ordinary and Ripple: Ordinary batches one synchronous output; Ripple uses eight bands with a paired tail over28 scans. Preserve every active/tail phase and fuse progress without a follow-up scan. Both short-gray paths use vendorGC48 within ten turns or a shorter user interval. After reported background flicker, every equal pair including white holds, removing three unchanged-white tail pulses per page; changed old glyphs still erase. This targets background flicker without claiming to fix the uncalibrated midgray transitions. Regular builds disable the experiment; unavailable/boot-disabled paths retain fullGL48/16-level targets. This is not an optically equivalent16-level replacement. FastBW uses the exact nativeDU20 for consecutive text-only turns, thresholding only targets while retaining real old-gray history, with harder edges and boundedGC48 cleanup. Images/backgrounds,initial entry and manual full remain grayscale. Settings/backup retain S31's three effects and Pico's two. Raw waveforms,power,clock and phase minima remain unchanged. Experimental images require the existing S31 layout; CrossLink needs its separate recovery flow.

## S31二级页与返回

2026-10-20用户最新修改：系统二级菜单恢复常驻灰底栏，首页组件/阅读目标、设置、文件目录及配网传输页面均保留。设置/文件内容止于1088，分页和系统键盘上移；设置PIN键盘采用紧凑布局，实际锁屏PIN保持全高。点击当前底栏项回到所属根页，其他项按下切页；升级写入、USB独占与上传等不可中断阶段保留保护。物理三键仍按根页/子页各自语义执行，不因为底栏可见就覆盖返回行为。

S31仅二值化内容，底栏保留固定0xe0灰底、固定图标和局部选中横条。2026-10-21用户修订：两板二级页右滑返回共用阅读识别器，内容区任意横向位置起手，水平位移至少120px且大于竖向位移，不再追加32px边缘和两倍角度限制；键盘起手仍留给输入控件。S31复用本板原八条带水波纹LTR返回，范围截止1120，避开底栏图标。主页间的水波纹仍按所选模式执行；二级页点按进入、左上角或物理返回和分页仍普通差分。多指、过短和竖向主导手势不触发返回；下拉玻璃、正文、锁屏与手动/故障恢复保留各自路径。常驻底栏不改变22相文字波形、192行备行或白底保持修复。

English: Per the latest2026-10-20 request, system children retain fixed0xe0 gray navigation, including home widgets/goals,Settings,Files and network/transfer pages. Content ends at1088 with paging/keyboards moved upward; settingsPIN is compact while the lockPIN retains its full-height layout. The current tab returns to its root,other tabs switch on press; update writes,USB ownership and uploads remain guarded. Physical-key root/child semantics stay independent. S31 binarizes content only and preserves fixed navigation icons plus local selection feedback. Accepted content right-swipes reuse reading recognition and this board's existing eight-band LTR ripple through1120,excluding navigation icons; taps,top-left/physical returns and paging keep ordinary differences. Main tabs honor the selected ripple policy. Reading22 phases,safe192-row prefill,white holding,quick-menu glass,lock,manual full and failure recovery retain their separate paths.

S31充电图标固定黑底白色闪电描边。检测复用CX25601N有效充电阶段，同时按官方BQ27220 `0x0C`有符号电流（大于5mA）补足阶段漏报；明确停止充电/OTG立即清除，通信未知最多保留已确认状态六秒。VBUS类型不作为插电依据，不修改电流、电压或终止参数。

S31 charging uses a fixed black well with a white bolt outline. Detection retains valid CX25601N charge stages and supplements missing stages with the vendor BQ27220 signed-current register0x0C above5mA. Explicit disable/OTG clears immediately; unknown communication can retain confirmed state for at most six seconds. Do not infer input from VBUS type or change electrical charge parameters.

## 准备 SDK

需要 Python 环境、ESP-IDF 6.1 及对应工具链。Pico 使用已有的 ESP32-S3 SDK；Metalio 使用已适配 ESP32-S31 的 SDK，必须包含 `components/soc/esp32s31`。普通只支持 S3 的 SDK 不能编译 S31。

将路径填入环境变量，Python 路径应指向已安装 ESP-IDF Python 依赖的虚拟环境入口：

```sh
export KIIKO_IDF_PICO=/path/to/esp-idf-s3
export KIIKO_IDF_METALIO47=/path/to/esp-idf-s31
export IDF_TOOLS_PATH=/path/to/idf-tools
```

## 一条命令生成两个版本

在源码根目录执行：

```sh
python3 tools/build_firmware.py --board all \
  --python /path/to/idf-python/bin/python \
  --tools "$IDF_TOOLS_PATH"
```

也可以单独选择：

```sh
python3 tools/build_firmware.py --board pico --python /path/to/idf-python/bin/python
python3 tools/build_firmware.py --board metalio47 --python /path/to/idf-python/bin/python
```

`--idf-pico`、`--idf-metalio47` 可代替 SDK 环境变量；`--output` 修改输出目录。离线构建可用 `--managed-pico`、`--managed-metalio47` 指定与各自锁文件匹配的 `managed_components` 缓存。`--dry-run` 显示计划，`--board all --list` 显示支持的设备。工具仅编译和打包，不打开串口。

构建器从同一维护源码生成独立的临时设备视图，分开保存配置、依赖、CMake 缓存、日志及产物：

```text
build/boards/
  pico/{source,build,sdkconfig,build.log}
  metalio47/{source,build,sdkconfig,build.log}
  packages/pico/
  packages/metalio47/
  packages/kiikoread-pico-<版本>-USB.zip
  packages/kiikoread-metalio47-<版本>-USB.zip
```

日常修改应改维护源码 `main/`、`components/` 或相应 `boards/<设备>/src/`，不要修改生成的 `build/boards/<设备>/source/`。共用功能不再分别维护两套应用目录；仅硬件、驱动和有明确平台差异的行为选择分支。

图标由同一个生成器维护：`python3 tools/gen_ui_icons.py --board all` 更新两块板的生成头文件；也可只指定一种设备。保留原图标枚举顺序和 Metalio 的前光图标，生成器不会覆盖共享的图标选择头。

## CMake 选择层

构建器将 `-DKIIKO_BOARD=pico|metalio47`、对应 `IDF_TARGET`、sdkconfig 和依赖锁文件交给各自的 ESP-IDF。生产源码中的条件分支由真实 C 编译器处理。请用上面的构建器隔离依赖锁和 SDK 缓存；不要在维护源码根目录轮换两个 SDK 并共用根 sdkconfig。板型和目标冲突、未知板型会在配置阶段报错。已有 CI 的默认板型仍为 Pico。

## 交付与验证

打包前校验应用内部芯片 ID、项目名、版本、分区表及两个 OTA 槽的容量，同时校验 bootloader 的芯片 ID 和 SDK 给出的烧录地址。完整 USB 包包含四个镜像、SHA-256、manifest、`flash_args`、`flash-command.txt` 及 `verification.json`；S31 命令保留 `--no-stub`。应用 Bin 只适用于已有正确底包的同板型升级，不能写到地址 0。固件内的升级校验也拒绝另一芯片/项目的应用。

本次整合为本地测试，沿用当前正式版本号区分板型，未覆盖官网、Release 或历史固定版本下载。Metalio 代码当前保留本地/私有发布边界。两套完整构建和主机回归不等同于已在两块实机完成触摸、显示、TF 卡及低功耗验证。

本地同版本测试请使用对应 USB 包；设备的正常 TF/OTA 检查仍会拒绝与当前版本号相同的镜像。

## 回归命令

```sh
python3 tools/board_selection_host_test.py
python3 tools/board_selection_host_test.py --build-root build/boards

python3 tools/board_source_view.py --board pico --tests --output build/host/pico
python3 tools/board_source_view.py --board metalio47 --tests --output build/host/metalio47
```

在相应生成的主机测试视图中运行现有测试。Pico 包括 `run_water_turn_host_test.sh`、`run_display_host_test.sh`、`reader_spacing_host_test.py`、`test_book_toc.py`、`lock_pin_host_test.py`；Metalio 还必须运行 `run_metalio_host_test.sh` 和 `run_metalio_ota_host_test.sh`，其专用夹具随测试视图自动选择。

子页布局按1216px可用高度定位：文件目录列表扩展，目录设置/移动按钮与分页贴底，系统输入键盘底部留26px；绘制、点击和局部刷新共用几何。字体列表与书架样式预览使用完整子页视口，传输底部按钮随视口下移。Pico保留原布局。

Child layouts use the1216px viewport: extend file lists, bottom-anchor directory/move actions and paging, and leave26px below system keyboards. Painting,hits and local refresh share geometry. Font lists and shelf-style previews use the child viewport; transfer footer controls move with it. Pico retains its existing layout.
