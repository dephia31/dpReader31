# e0470_epaper_waveform

**E0470A01** 4.7 寸单色墨水屏（684 × 1216，40 pin）的 epdiy 波形表，以及一个在开机时从完整表派生更短表的裁剪器。

epdiy waveform tables for the **E0470A01** 4.7" monochrome e-paper panel (684 × 1216, 40-pin), plus a trimmer that derives shorter tables from the full ones at boot.

组件名 / Registry name: `mindreset/e0470_epaper_waveform`

## 波形表 / Tables

| 符号 / Symbol | 内容 / Content | 用途 / Use |
| --- | --- | --- |
| `E0470_WAVEFORM` | 默认表。厂家 2.0.76 起为完整 GC16 / GL16 48 相（`E0470_WAVEFORM_TRIM=1` 才开机裁成 GC36 / GL37）；附阈值 DU（目标 0–7 → 黑，8–15 → 白） / Default. Full GC16 / GL16 48 phases since vendor 2.0.76 (GC36 / GL37 only with `E0470_WAVEFORM_TRIM=1`), plus a threshold DU (dest 0–7 → black, 8–15 → white) | 固件的页面刷新 / Page refresh in the firmware |
| `E0470_FULL_WAVEFORM` | 完整表：GC16 / GL16 各 48 相，DU 20 相，单一 0–50 °C 温度档 / Full tables: GC16 / GL16 48 phases, DU 20 phases, one 0–50 °C temperature range | 对照与 A/B 比较 / Reference and A/B comparison |
| `E0470_GRAY8_WAVEFORM` | 旧接口兼容名；S31 本地版沿用完整 48 相表（未提供 30 相 8 灰表），仅相位耗时至少约 576 ms / Legacy compatibility alias: the S31 build keeps the full 48-phase tables (no 30-phase 8-gray calibration), at least about 576 ms for phases alone | 刷新测试页的灰阶对照 / Gray comparison on the refresh test page |
| `E0470_FOLLOW_WAVEFORM` | 开机生成的 8 帧短 DU（黑 7 / 白 8），配合 FAST 扫描档使用 / 8-frame short DU (7 black / 8 white) built at boot, meant for the FAST scan profile | 触摸笔迹与实时读数 / Touch ink and live digits |

开机在第一次刷新前调用一次 `e0470_waveform_init()`，它负责生成默认表与跟随表。

Call `e0470_waveform_init()` once before the first refresh; it builds the default and follow tables.

## 裁剪器 / Trimmer

裁剪默认关闭（厂家 2.0.76-debug 起 `E0470_WAVEFORM_TRIM 0`）：裁剪把黑目标的饱和期从 18 帧减到 13 帧，强光下墨色偏淡、原生 16 灰阶出现级别合并与非单调跳变；三份参考的原始数组逐字节相同，问题出在裁剪本身。DU 表与原始 GC48 / GL48 保持一致，`E0470_WAVEFORM_TRIM=1` 仍可回退到 GC36 / GL37 的快速页面表。

Trimming is off by default (vendor 2.0.76-debug sets `E0470_WAVEFORM_TRIM 0`): it cut the black saturation from 18 to 13 frames, which left ink pale in strong light and merged / broke the monotonic native 16-gray ladder; the raw arrays are byte-identical across the three references, so trimming was the cause. DU and the raw GC48 / GL48 tables are unchanged, and `E0470_WAVEFORM_TRIM=1` still selects the faster GC36 / GL37 page tables.

`e0470_waveform_trim()` 在不改变灰阶梯子的前提下缩短完整的 GC16 / GL16 表。完整表中每对 (from, to) 的序列共用同一个骨架：保持补齐、往反向轨道擦除、在目标轨道饱和、灰阶尾、结尾保持。灰阶档位全部位于尾段；裁剪器保留尾段不动，只裁擦除上限和饱和开头，再把所有序列右对齐。代价是擦除余量变小，旧内容的残影会先出现，因此参数需要在真机上对着残影调整。

`e0470_waveform_trim()` shortens a full GC16 / GL16 table without changing its gray ladder. Every (from, to) sequence in the full table shares one skeleton: hold pad, erase toward the opposite rail, saturate on the target rail, gray tail, trailing hold. The gray levels live in the tail; the trimmer keeps the tail intact, cuts the erase maximum and the saturation head, then right-aligns every sequence. The cost is less erase margin, so ghosting from old content appears first. Parameters should be tuned on hardware against that ghosting.

裁剪参数：`erase_max = 11`，`sat_cut = 5`，`white_sat_cut = 0`，`hold = 3`（仅 `E0470_WAVEFORM_TRIM=1` 时生效）。

Trim parameters: `erase_max = 11`, `sat_cut = 5`, `white_sat_cut = 0`, `hold = 3` (only with `E0470_WAVEFORM_TRIM=1`).

## 辅助函数 / Helpers

- `e0470_waveform_phases()`：取某条波形中 `MODE_GC16` / `MODE_GL16` / `MODE_DU` 的相位表。/ The phase table for `MODE_GC16` / `MODE_GL16` / `MODE_DU` in a given waveform.
- `e0470_phase_action()`：查 (from → to) 在指定相位的 2 bit 动作（保持 / 压黑 / 擦白）。/ The 2-bit action (hold / darken / erase) for (from → to) at a given phase.
- `e0470_follow_lut_build()`：生成任意帧数的跟随表。/ Build a follow table of any frame count.

## 说明 / Notes

自行调整屏幕波形会使设备失去保修。`waveforms/*.h` 是数据表，不应手工编辑。

Changing panel waveforms voids the device warranty. The `waveforms/*.h` files are data tables and are not meant to be edited by hand.

帧周期固定为 11 090 µs（约 90 Hz），取自面板算法文档的设计点。扫描时序由 `read_pico`（`read_pico_epd_timing.h`）负责，不在本组件内。

The frame period is fixed at 11 090 µs (about 90 Hz), the design point in the panel's algorithm document. Scan timing is owned by `read_pico` (`read_pico_epd_timing.h`), not by this component.

## 依赖 / Dependencies

`epdiy`（提供 `epd_waveform.h`）。

`epdiy` (for `epd_waveform.h`).

## 许可 / License

Apache-2.0，见 [LICENSE](LICENSE)。波形表随本板附带，按现状提供。

Apache-2.0, see [LICENSE](LICENSE). The waveform tables ship with the board and are provided as-is.
