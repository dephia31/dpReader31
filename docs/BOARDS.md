# 双板型构建

Metalio S31 源码维护于公开仓库 [Metalio-S31-kiikoread](https://github.com/wegooo-cell/Metalio-S31-kiikoread) 的 `main`。正式二进制继续在统一 Release 归档，官网按机型选择。现有文件许可与第三方权利保持。

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
| 联网 OTA | 沿用正式版 | 支持，GitHub Pages 优先、官网备用 |

Metalio 仅适用于已适配的 **V1.5 / E0470A14-AF 34Pin** 硬件，不自动推断其他硬件修订。两种设备的原始波形、供电时序、引脚、存储地址及默认配置分别保留；共享功能更新不代表把一块板的底层参数搬到另一块板。

两块板共用rc97的凭据读取和紧凑启动记录逻辑：正常可读的已设密码仍验证；读取失败或损坏时不拦启动、可重新读取且禁止覆盖旧凭据。启动记录兼容旧定长格式，空间不足只回收自身旧记录。S31异常启动字体回退、USB交接、前光与睡眠恢复由自身后端保留。

Metalio设置主列表及文件管理内容按黑白目标使用DU20，固定底栏保留已显示像素，手动全刷与故障恢复仍走完整路径；设置子页图片预览保留灰阶。首页继续阅读卡片纯白，七款时钟在两种板型共用可释放底图缓存，覆盖首页、样式预览及放大时间页。实时底图两槽最多231840字节；S31额外为七款选择页预览各保留一个有界槽，统一池最多1043280字节PSRAM，始终预留2MiB；IKEA/小屋数字使用当前系统字体，Cat眼睛和Kehai圆环/圆点仅重绘局部，日期/分钟/方向/黑白模式和尺寸变化按需失效；内存不足直接绘制，进入阅读前释放。S31样式选择页固定10:10:30并按需缓存完整表盘，停止秒针/猫眼更新；箭头切换不单独扫描按下态，未变的底部按钮排除在更新范围之外，仍使用完整灰阶。选择页显示完成且空闲600ms后每轮最多在临时PSRAM画布准备一个相邻表盘，不写front/back，保留2MiB余量，用后即释放；触摸、待推屏或缺内存时暂缓。离开字体设置后重新进入首页时失效完整预览，首页与时间详情的实时底图不包含活动字形。普通灰阶切页的备行等待仅在非传输且无欠载时缩减，欠载后本次开机回退保守余量。

Metalio普通主页使用定向黑白端点和完整中灰相位，周期整理与底栏横条共享扫描；阅读、水波纹、手动全刷与失败恢复保留各自路径。当前普通灰阶48相、每相至少12ms，仅相位节拍已需576ms，另有供电、排版与传输开销；不能用CPU或内存规格推断整页能在300ms完成。串口的`SWITCH`日志区分退出、字体、进页、推屏和总耗时，`main refresh`另记录供电及显示总耗时；这些是固件计时，最终光学稳定时间仍需实机测量。

S31的PIE只在Core1可用。较大的差分作业在扫描空闲时整帧提交给现有Core1供屏任务，前台同步等待像素、脏图与前导统计全部归还；小区域、无工作核或资源不足保留原路径，不迁移主任务。超时先请求取消并收回缓冲后回退，无法归还时停止而不留下后台写入。默认构建使用C算子，显式加`--s31-pie`才在Core1的16字节对齐完整块使用128位PIE，Core0、未对齐和尾部保持C。`METALIO_LOCAL_LATENCY=ON`仅供本地整帧oracle及开关对照，交付构建必须关闭；微基准不能作为整机倍数。

S31实际显示输出与SDK Flash操作共用递归锁，完整扫描和断轨结束后才允许Flash停顿；准备封面/绘制期间不持有此锁。主页面绘制可与TPS的50ms唤醒等待重叠；只提前WAKEUP，PWRUP/VCOM保持低，实际扫描仍执行原厂后续顺序，取消或失败立即下电，不保压等待用户。供屏在原1KiB查表内预置各像素位，空闲普通/黑白切页保留已验证的128行预填，传输、临时对时/扫描、BLE活动和欠载保持192行；扫描仍为20MHz、原GC48/GL48/DU20与至少12ms。S31内部DMA双缓冲各64行，以64字节边界分成不超过4032字节的描述符，每个缓冲只在末段通知EOF，保留约1ms补数余量且不改变面板行时序。DMA EOF提高中断优先级并过滤准备/停止期的旧VSYNC；按SDK启用RGB ISR缓存关闭安全选项，ISR读取的行队列指针表固定在内部RAM且只按指针大小分配。旧S31构建缓存由构建工具迁移这两项安全配置，其余配置保留。未完成扫描仍报错并走原恢复。

书架最终灰阶封面复用既有动态4MiB LRU，不增设整页常驻副本。键包含源像素校验、尺寸、旋转、对比度与4×4点阵相位；仅复制精确对齐的物理行，收藏、边框和文字实时画，未对齐或内存不足直接绘制。阅读前缩减与媒体失效清理沿用既有生命周期。首页分钟到点把状态栏和静态表盘合并为一次真实差分，输出失败不确认已显示。

## S31关机与锁屏省电

“关机睡眠”只保留“浅睡10分钟后进入深睡”（默认）和“彻底断电”，不再单独选择锁屏省电方式。旧`lock_lp`值忽略，已明确保存的关机/睡眠选择仍保留；新选择在NVS提交成功后生效。日常短按及自动锁屏统一先浅睡十分钟后深睡，电源菜单选择彻底断电时走原硬件关机流程。

浅睡保留RAM、字形与阅读索引，使用Metalio原厂SD、TPS、屏总线、P17/GPIO2和断轨顺序。每个维护切片不超过60秒，最后一段按十分钟截止时间缩短，不反复挂载TF。真正按键优先于到时交接，定时不能解锁或点前光。深睡保存阅读检查点，暂停字体文件、触摸、总线和外设电源后调用`esp_deep_sleep_start()`，短按通过启动恢复，有密码仍验证；这与原厂浅睡后硬关机的策略不同。

USB、忙传输、未完成的媒体恢复和不安全唤醒线暂缓休眠；没有成功保存的检查点不贸然进入深睡。浅睡失败按1/3/10/30秒退避，深睡交接失败保持锁定并先浅睡、30秒后重试，不循环重启。续航、十分钟交接、字体/PIN及USB热插恢复仍需拔线实机验证。

English: The S31 power preference contains only default light sleep then deep sleep after ten minutes, or complete power-off; obsolete lock_lp is ignored and saved shutdown choices remain. Persist before changing live selection. Ordinary locks always use staged sleep. Preserve vendor peripheral ordering, RAM/font/index state, at-most60second slices and a shortened final slice. Physical input wins over the timed handoff; timers never unlock/light. Deep sleep requires a saved checkpoint and parks font IO/touch/bus/rails before the SDK call, retaining boot/PIN recovery. USB/busy media/unsafe IRQ defer admission. Light failures back off1/3/10/30seconds; refused deep handoffs remain locked and light-sleep before retrying30seconds later. Hardware current and wake/USB behavior require validation.

## S31 AI快捷键

AI/BOOT按键沿用[官方GPIO61输入](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/main/main/boards/metalio-e-ink4-plus/config.h)，不改下载和唤醒配置。稳定按下500ms打开上方横向液态玻璃菜单，点选“全刷一次、书签开关、前光开关、返回首页、不设功能”后保存短按映射并收起；默认全刷。只改变映射，不当场执行所选动作。菜单已打开时再按AI或点外部关闭，长按后的抬手不触发短按；书签仅当前阅读正文有效，复用既有书签记录与提示。

菜单只保存648×252底图（81648字节临时PSRAM），展开一次模糊与一次有界灰阶差分，收起精确还原并一次差分；没有入场位移动画、额外清白或逐项推屏。选项共用绘制/命中矩形，输入零分配/零绘制，多指、拖动和触摸读错取消，保存失败保留旧映射并提示重试。低内存无法保存底图时保持原页面。玻璃保留本板现有完整灰阶，未宣称已实测达到300ms；串口AI compose/display分开记录耗时。

菜单覆盖时暂停底页轮询/字体重试/导航和蓝牙翻页；电源、锁屏、媒体变化会清理覆盖层；配置菜单不阻止原自动锁屏，进入锁屏前先还原实际底页，透明书封不会缓存菜单。锁屏/PIN期间AI不用于唤醒；跨休眠持有的按键须先释放。USB读卡、升级、传输写入和PMU专属页不接管AI。映射写入S31设置命名空间，仅成功提交才更改当前映射；旧跨板配置备份格式保留。

English: The vendorGPIO61 AI input keeps USB/wakeup configuration. Hold500ms for a glass chooser to persist tap refresh/bookmark/light/home/disabled; selection never executes the action. Default refresh; bookmarks remain reader-body-only. One blur and one bounded grayscale output per normal transition,81648B temporary underlay,exact restore and zero input allocation/painting. Preserve the calibrated grayscale path,modal/transfer/update/USB/PIN guards and sleep-release suppression; no measured300ms claim or portable-backup format change.

## S31覆盖层与电量

AI菜单五个圆形图标横向排列，文字在图标下方，选中项以圈内右上小点标记；标题和标签在玻璃处理之后直接绘制黑字，不加独立文字底色。下拉菜单保留左侧2×2开关和右侧前光双滑块，玻璃层提亮并模糊底字；输入继续恢复缓存并局刷。电源菜单仅在滑动条和重启/取消圆钮内部模糊提亮，控件外的页面像素保持原样，再画清晰文字与图标；输入不重复处理背景，缺内存保留原实心回退。

S31百分比直接使用BQ27220 `StateOfCharge(0x2C)` 的0～100值，不使用电压曲线估算或Pico的显示修正。电压仅验证读数范围；SOC读错或越界显示未知，不能用电压补算。充电检测继续结合BQ电流和充电芯片状态。计量精度取决于电池容量/化学参数、学习与老化状态，显示来源正确不等于已验证计量精度。

English: Paint crisp black labels after glass. Keep the S312×2 switches and dual light sliders; blur only on entry and retain cached local input. Power glass is confined to the slider and round buttons; all background pixels outside their shapes stay intact. AI labels have no separate backing. S31 percentages use BQ27220 SOC directly with no voltage estimate or Pico correction; failed/invalid SOC remains unknown. Gauge accuracy still depends on battery parameters,learning and aging.

## S31锁屏与侧键

“双击解锁”位于显示主列表的自动休眠锁屏下一行；新配置默认关闭，旧配置中的明确单击/双击选择不改变。S31继续使用自身单击/双击事件消费与PIN门禁，关机睡眠页统一为先浅睡十分钟后深睡或彻底断电。票根封面与背景仅经一次对比度灰阶映射，保持自然比例和旧封面缓存兼容。用户主动选择导入阅读字体连续三次真正回退内建后，可选择保存进度再重启或返回字体页；未保存成功不能重启，异常启动保护仍独立生效。

首页元数据、封面索引与头像使用PSRAM BSS，保留静态零初始化；DMA、ISR、板级状态和缓存预算不变。时钟预览待更新范围独立于按下反馈，在失败后合并后续范围，仅输出成功才清除。S31冷启动使用已有内建字形绘制Kiikoread与准备资源，不再链接全屏loading位图；USB接管、VCOM校准、深睡保图与PIN流程保留。

English: The Display root list owns the unlock toggle immediately after auto-lock; missing settings default to single press while explicit saved choices survive. Keep S31 wake-event consumption and PIN with the unified staged-sleep/power-off preference. Ticket artwork receives one grayscale contrast mapping. Three actual fallbacks from explicit imported-face selections offer a progress-safe restart or cancel; abnormal-boot protection remains independent. Cold Home metadata/cover indices/avatar use zero-initialized PSRAM BSS, never DMA/ISR storage. Clock pending bounds survive failed outputs and merge subsequent changes. The cold splash uses existing embedded glyphs without the loading bitmap, preserving USB/VCOM/deep-wake/PIN flows.

锁屏单击电源键后等待650ms双击窗口，再局部显示“双击电源键解锁”，约三秒后还原原图。只暂存448×72区域（16128字节PSRAM），不额外常驻整帧；缺内存跳过提示。提示不打开前光，触摸不解锁；双击及已有密码验证继续走S31原路径。提示待显示或已显示期间不启动新的硬件休眠事务，恢复后沿用原模式。锁屏图缓存先于提示保存，提示不会进入持久缓存。

上下音量键在四个根页面与屏幕左右键一致：上键对应左键、下键对应右键，循环切换主页面；书架分页控制开启时同样用于翻页。阅读和二级页面继续由该页面的按键映射处理。菜单、电源对话框、下拉层、USB独占、升级及待处理切页均保留输入保护。

English: A debounced locked short release schedules Pico's448×72 hint after650ms for3s, using16128 bytes of temporary PSRAM and restoring exact pixels. Keep the lamp off, double-click/PIN authentication and S31 sleep transactions; no touch unlock or persistent hint cache. Root volume edges share left/right navigation and shelf paging; reader/child handlers remain page-owned, with modal/USB/update/pending-navigation guards.

透明PNG锁屏书封缓存继续放在`/sdcard/.metalio_s31/locks/seal-layer.bin`，使用SEA2无损行压缩保留原gray/alpha字节和阴影。首次生成在空闲PSRAM大于两图层加1MiB时直接处理并写压缩层，否则沿用有界行流程；所有工作区用后释放，不缓存当前页面底图。旧格式/尺寸/文件变化或损坏触发重建，完整校验先于叠加；FAT不能覆盖重命名时备份原缓存、发布失败回滚。图层不减灰、不改完整刷新波形和S31睡眠/PIN/USB流程。

## 大图片EPUB打开

排版通过书源尺寸接口测量插图，不为每幅图完整提取资源。PNG先探测32字节；JPEG仅在必要时扩大到4KiB、64KiB，SVG包装与更晚的帧头沿用原有有界完整读取回退。ZIP前缀探测按512B/4KiB输入分块，完整提取仍使用32KiB及完整CRC校验；头部仅是尺寸提示，不能作为图片内容验证或完整下载成功的依据。当前章仍按原规则分页、恢复位置，实际显示只解码当前页图片，没有因此承诺任意长任务可打断或其余章节已全面后台化。

English: Layout uses advisory source dimensions instead of extracting every illustration in full. PNG starts with32 bytes; JPEG grows to4KiB/64KiB when needed, retaining bounded full fallback for SVG wrappers and later frames. Prefix input chunks are512B/4KiB; full extraction retains32KiB bulk reads and complete CRC verification. Dimensions do not establish payload integrity. Keep current-chapter pagination/progress and current-page decoding; this does not implement universal cancellation or background preparation of all remaining chapters.

## S31阅读面板响应

同一已显示纯文字页打开工具栏，或从工具栏进入字体/阅读设置与统计时，核对正文、章节、页码、排版代次、方向、全屏状态和前后缓冲一致后，只叠加面板，不重新绘制正文、准备插图或启动预绘。图形面板恢复原厂GL48、16级目标及一次区域定稿，不再把滑轮、开关借用文字22相量化。符合条件的完成绘制/字体重排及局部控件也通过同一完整灰阶面板出口；其余回退原路径。面板的绘制缓存优化仍保留，相位扫描时间恢复完整灰阶。
面板开合暂停未完成预绘，保留现有四槽里同代次的已完成正文；完整绘制需要时可复用这些正文，再画当前控件。覆盖面板、提示不写入正文缓存，字体/字重/抗锯齿/布局与媒体变化继续走既有完整失效。缓存未命中或内存不足直接绘制，不增加另一套常驻整帧。面板差分不计入整页自动清理次数，手动全刷和正文周期清理继续保留。

English: Overlay tools/primary settings only over verified unchanged text, retaining existing body-cache/prefetch reuse. Graphical panels now use complete vendorGL48 and16-level targets in one regional settle; neither text22-phase quantization nor FastBW may handle sliders/switches. Qualifying final renders/reflows and local controls share this full-gray outlet. No measured latency claim.

### 可选GL37阅读面板实验

仅显式加`tools/build_firmware.py --board metalio47 --s31-reader-panel37`启用，普通构建每次强制关闭，包清单记录`experimental_reader_panel37`。按Metalio官方`erase_max=11/sat_cut=5/white_sat_cut=0/hold=3`从本机GL48派生37相，保留16级输入和灰阶尾段，不量化控件；屏蔽所有等值迁移，包括白到白补推。原GC48/GL48/DU20数据与扫描时序不变。

只作用于已核对正文缓存、章节/页码/排版代次/方向/全屏和缓冲的面板叠加、同面板切换及局部控件反馈；字体或字号松手后重排的整页仍走完整48相。手动/周期清理和故障恢复优先，图文、书架/封面、首页/系统页面及锁屏沿各自原路径。构造仅尝试一次，波形缓冲最多3072B PSRAM；原厂裁剪器另有17408B静态工作区，S31放在外部RAM，不挤占联网内部堆，不能把这部分记为零开销。分配失败直接回到原48相；扫描失败保留原故障恢复并在本次启动停用37相。

37相相对于48相少11次扫描，不等于整次交互必然提速23%：渲染、差分、供电和像素实际变色均有耗时。裁掉擦除/饱和余量可能增加残影或降低墨色稳定性，必须按实屏比较；本地包不覆盖已发布同版本固件，不影响Pico。

English: Explicit `--s31-reader-panel37` alone enables vendor11/5/0/3 GL37 on verified same-body panels/sliders; regular builds force it OFF and manifests record the flag. Preserve sixteen input levels, gray tails,final holds,native tables and scan timing,holding every equal pair. Whole-body reflow,art/other pages and manual/periodic/fault cleaning retain previous owners. One3072B PSRAM LUT plus the vendor trimmer's17408B external static scratch are the added budgets,preserving internal network heap; allocation failure uses native48,while a scan fault retains recovery and disables37 until reboot. Eleven fewer scans do not establish23% total latency improvement or equivalent optical stability; compare on hardware and keep the image local.

### 两个主设置面板四灰快刷

用户在200毫秒提速讨论后最终选择四灰，取代此前讨论的纯黑白。仅显式`--s31-fast-reader-panels`构建启用，清单标记`experimental_fast_reader_panels`；普通构建强制OFF。仅字体设置、阅读设置使用0/5/10/15四级，UI原13/14近白底色映射浅灰10，避免控件消失；实底先覆盖面板矩形，区域外正文/插图和历史帧不量化。不改其他设置子面板、书架、封面、锁屏或正文刷新规则，不把前次图形22相扩大试验重新全局启用。

面板独立16相：黑白目标沿用官方短DU的7黑/8白预算；白起点中灰直接复用已有FOLLOW灰推；其他旧色先复用8白再推中灰，最长8白+5黑，末尾至少3相保持。没有截取GL48或正文22相，不增加白到白维护。此组合是本地实验，并非原厂提供的完整四灰标定表，墨色与残影必须实屏比较。LUT仅1024B外部RAM，无额外整帧；不受正文22相的开关/错误禁用影响。

面板扫描故障本次启动禁用短表，保留既有完整恢复，后续回退37/48相；37相自身的错误禁用不能被绕过。图文底页只在已成功显示且正文/章节/页码/排版代次/图片数/方向/全屏及前后帧一致时直接覆盖，不重复解码；新实底必须覆盖完整旧面板。露出正文/图片、重排或其他不匹配仍按原路径恢复。字体/字号等真正正文重排、手动/周期清理继续完整48相。入口不为按下态预刷DU20，抬起提交一次；取消不补刷。

按现有12毫秒相周期，16相名义约192毫秒，再加绘制/差分/供电；不能将此写为实测总响应已达200毫秒。此试验不加后台延迟全刷，旧37相本地包仍保留对照。

English: The final choice is four grays only on the Font/Reading primary sheets. Explicit `--s31-fast-reader-panels` enables it,regular builds force OFF,and manifests record the flag. UI targets are0/5/10/15,with13/14 fills retained as10; opaque sheets leave outside text/art/history untouched. The isolated16-phase experiment combines vendor shortDU seven-black/eight-white budgets with existing white-origin FOLLOW grays; nonwhite gray transitions whiten first. The longest eight-white/five-black sequence retains three final holds,with equal whites held. This is a local combination,not a vendor-calibrated four-gray table,and does not trim GL48/text22. One1024B external LUT,no extra frame,independent body settings/faults. Guarded overlays may reuse image underlays without decoding; uncovered body/art,reflow/manual/periodic/fault recovery retains previous paths and37/48 fallback. Drop the entry DU20 press pass; cancel without scanning.192ms is nominal scanning,not measured200ms total; compare ink and ghosting on hardware without delayed background cleaning.

## S31图形短灰阶实验撤回

406f7e8的22相图形扩展已撤回：书架亚克力浅灰被13/14→15量化成白色，原16级全刷与四灰页刷目标不一致；简化中灰迁移未经光学校准，用户实机反馈封面发黑、文字残影和设置控件异常。书架/首页封面、下拉、AI、电源、目录和解锁提示恢复原灰阶路径；普通/水波纹16级48相，原快刷仍黑白DU20，固定灰底栏与缓存预算保留。禁止仅把48截成24或改名兼容：原GL表白→10需23个有效驱动及25相跨度，5→10需38个有效驱动。正文22相不改，原厂表/电源/VCOM/扫描/USB/PIN保留，新构建显式关闭旧UI实验缓存。恢复图形正确性会增加扫描时间，后续独立短灰阶须实机标定。

English: Withdraw406f7e8's graphical22-phase extension after optical reports. Nearest-level mapping collapsed13/14 to white and differed from full16-gray redraws; uncalibrated midgray transitions produced dark covers/ghosts/control defects. Restore native UI/art paths and complete gray panel settling, retaining text22-phase,fastDU20,gray navigation,caches and hardware.24 phases are not an automatic compatibility fix: source15→10 has23active pulses over25phases,5→10 has38active pulses. New builds explicitly disable the old UI flag; optical validation remains required.

## S31普通与速读翻页

阅读设置的“翻页效果”在普通、水波纹、速读黑白间点按切换，原有0/1设置值保持兼容。所有文字路径保留安全192行备行；128行在实机普通阅读出现欠载，已撤回。TPS预唤醒与差分生成重叠，保留厂家等待和驱动顺序。

本地显式 `--s31-text-ripple` 包按用户最新要求让普通与水波纹共用四级目标（0/5/10/15）及22相。普通用一次整轮提交同步输出，不作条带动画；水波纹用8带、末两带共用启动时刻，共28拍。每个像素的有效/尾相保持，正文与进度合并，不追加第二轮页脚刷新。用户2026-10-22修订：自动全刷统一选择不全刷、5/10/15/20/30页，短灰阶与速读不再强制十页清理，面板显示所选间隔，保存/重启/备份恢复保持一致。用户反馈白底闪动后，短表的白→白与其他等值迁移全保持，取消每页三次白底尾部补推；变化的旧字仍保留擦白驱动。此项针对未变化白底闪动，尚不能据此宣称简化中灰迁移的残影已解决。

常规构建强制关闭实验；禁用实验、波形不可用或本次开机扫描失败后，普通回到完整GL48及16级字缘，水波纹使用对应完整序列。这里不把22相称为完整16级等效；灰阶及残影须按实屏验收。原厂波形原数据、电压、扫描时钟及相位下限保留。

速读黑白仅在前后均为无图无背景文字页时使用本板原厂DU20原数据，目标阈值化为黑白，实际旧灰阶保留作为迁移起点；字缘更硬，正文/进度合并，周期GC48遵循所选自动全刷间隔。所有模式的图片/背景、首次进入和手动全刷保留完整灰阶。三种效果随重启和配置备份保存；Pico仍为两种效果。

实验Bin只适用于已有kiikoread S31分区。CrossLink应用位于0x20000，与本项目0x10000不同，须走单独恢复流程；本地包不改NVS格式、不公开OTA。

English: S31 retains safe192-row prefill after a real128-row reader underrun, and overlaps TPS pre-wake with difference generation without changing vendor sequencing. The explicit local `--s31-text-ripple` build now shares one four-level22-phase table between Ordinary and Ripple: Ordinary batches one synchronous output; Ripple uses eight bands with a paired tail over28 scans. Preserve every active/tail phase and fuse progress without a follow-up scan. Per the2026-10-22 revision, periodicGC48 follows Off/5/10/15/20/30 turns for every effect, persisting across reload and backups without a ten-turn cap. After reported background flicker, every equal pair including white holds, removing three unchanged-white tail pulses per page; changed old glyphs still erase. This targets background flicker without claiming to fix the uncalibrated midgray transitions. Regular builds disable the experiment; unavailable/boot-disabled paths retain fullGL48/16-level targets. This is not an optically equivalent16-level replacement. FastBW uses the exact nativeDU20 for consecutive text-only turns, thresholding only targets while retaining real old-gray history, with harder edges and user-selected periodicGC48 cleanup. Images/backgrounds,initial entry and manual full remain grayscale. Settings/backup retain S31's three effects and Pico's two. Raw waveforms,power,clock and phase minima remain unchanged. Experimental images require the existing S31 layout; CrossLink needs its separate recovery flow.

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

## S31传输与深睡恢复

大于等于四个接收块的上传可使用两槽顺序写卡：HTTP只填空槽，单个后台任务按原顺序写入；每次最多增加一块接收块大小的PSRAM及4096字节内部任务栈，资源不足沿用单缓冲。任何接收/写入失败先等待已提交任务完成，再关闭、清理或替换文件。只在整个文件落盘关闭成功后确认提交。TCP接收窗口65535字节、接收邮箱64项，静态RX默认6/动态RX64/BA12；PSRAM传书独占会话按S31 SDK建议对照16槽静态RX/BA窗口（最大16槽），普通联网与临时对时保持原6槽。乱序队列最多12包并启用SACK，省电所有权保留；USB MSC块16KiB、TF四线20MHz，始终电脑/固件独占挂载。

深睡保留屏幕物理锁图，TF挂载尚未完成时不清白换图；有效快照恢复前后参考不额外扫描，缺失/损坏时仅对可独立生成的真实锁屏作一次完整输出。未知透明底图保留物理画面，禁用差分提示；实际解锁取消所有延迟恢复。PIN继续验证，异常启动字体保护和S31外设休眠顺序保持。入书/目录返回正文使用一次原厂GC48，后续正文22相及图形GL48各走既有路径。

English: Large S31 uploads use two bounded ordered slots,one optional PSRAM block and4096B internal worker stack; resource failure falls back synchronously. Join before close/cleanup/commit on every path. TCP receive65535/mailbox64,default staticRX6/dynamicRX64/BA12 and bounded12-packet SACK. PSRAM bulk sessions compare the SDK16-slot staticRX/BA minimum (capped at16),while ordinary connectivity/time sync retain6; MSC16KiB with unchanged four-bit20MHz card timing and exclusive ownership. Preserve physical deep-lock art while media is pending; trusted snapshots restore references without scanning,unknown transparent art never receives difference hints. Cancel deferred work before unlock,retain PIN/abnormal-font/board sleep guards,and clean reader entry once with vendorGC48 without changing subsequent body or graphical waveforms.

文件传输服务持有BLE网络租约，停止扫描/连接并阻止主循环重启蓝牙；关闭服务后按原设置及最近设备恢复，不清配对/学习。仅连接WiFi的常驻会话不持有该租约，仍可使用蓝牙翻页器。临时扫描/联网的失败必须回滚租约；成功启动后的任何退出都释放一次。`/info`提供固件ELF身份、实际接收预算、BLE运行状态和内部RAM余量，便于核对实机；这些诊断不提供任意命令或存储访问。

English: File-transfer sessions own a BLE radio lease,stop scanning/links and prevent foreground restart until release,retaining user settings and recent peers. Network-only keeps coexistence;temporary-network failures roll back ownership. `/info` reports firmware ELF identity,actual RX budget,BLE and internal heap without arbitrary command/storage access.

## 板级能力与跨平台入口

`boards/<设备>/kiiko_board_config.h` 集中维护身份、NVS 命名空间、硬件能力和缓存预算；`main/kiiko_board_config.h` 是兼容入口。沿用 Moku 的板级门面 `components/kiiko_board`，其初始化不得重新拉起已由 BSP 初始化的外设。`board.cmake` 读取对应 `board.json`，芯片冲突立即报错，不强制改写旧构建目录。

S31 稳定 ID 为 `metalio47`，兼容 Moku 的 `metalio_s31` 构建别名。Windows 加载对应 ESP-IDF 后可运行 `tools/build_board.ps1 -Board metalio_s31`，该入口共用 Python 构建器，分别生成源码视图、配置和依赖锁。`all` 需要分别配置两块板的 SDK 环境变量，参见上面的 Python 参数。

私有 S31 仓库的 CI 同时编译两块板并验证芯片、项目名、分区、启动地址和应用大小。网页完整性回归使用 `tools/fixtures/flasher` 中的固定脚本及模拟清单，不依赖未提交的官网固件；官网文件和 Pages 部署继续由公开 Pico 仓库维护。

English: Moku's board manifest/facade design centralizes capabilities and unchanged budgets while keeping native backends and strict target/config guards. `metalio_s31` aliases stable `metalio47`. The PowerShell wrapper shares the Python materializer/build driver. Private CI builds both chips and validates package identities/layouts; web regressions use self-contained fixtures without publishing a site.

S31 SDMMC沿用Metalio官方内部DMA中转：固定4096字节内部对齐缓冲、最多8扇区，外部RAM不直接进入SD DMA；失败挂载与成功卸载回收，卸载失败保留缓冲和卡所有权。时钟降档、四线引脚、FAT锁及USB独占流程保持。传书每两秒只对文件名/字节/进度条做局部黑白差分，失败刷新重试，网页字节发送完成后继续计时并等待设备保存确认。

显式本地`--s31-transfer-startup-test`仅用于无人值守对照，启动仍保留PIN/恢复门禁，随后进入已保存网络的传书页。正常构建强制关闭并在manifest/verification记录该开关；此测试包不得交付。

### Metalio 字体恢复与文件名

S31正常待机后继续恢复TF上的字体与书籍句柄；临时读卡/内存失败按1、3、10、30秒间隔重试，成功手动加载可清除旧拒绝记录。已判定无效的字体仍限制自动重试，异常启动仍使用内建字库，不覆盖保存的选择。字体列表最多128款，按实际路径区分文件，标签按完整UTF-8字符截短并区分重名；仅收录`.ttf`和`.otf`，扩展名不代表所有内部格式都支持。原本超过设置路径容量的文件名才会缩短并附校验标识，保留字体内容、避开已有文件。字体上传自动规范化不适合FAT的符号，其他上传类型的路径检查保持原样。

S31 resumes pending SD font/book handles after normal standby. Transient I/O/OOM failures retry after1/3/10/30seconds; a successful manual load clears old rejection. Invalid faces still limit automatic retries, and abnormal-start protection retains built-in fonts without changing saved choices. The128-entry catalogue identifies actual paths, keeps UTF-8 labels valid and distinguishes duplicates. Only `.ttf`/`.otf` are listed; extensions do not guarantee support for every outline format. Only names exceeding persisted-path capacity are shortened with a hash, retaining font bytes and avoiding existing files. Font uploads normalize FAT-incompatible symbols; other upload path checks remain unchanged.
