# S31 display inversion - RC8 experiment

In Settings > Display > Invert colors ("Dao mau" in Vietnamese), enable inversion of the whole UI, text and images. Default is off. The preference persists in NVS and does not change frontlight brightness. Available in Full and Love Book.

The renderer, prepared pages, image caches and application framebuffer remain in normal logical polarity. The S31 output adapter uses one lazily allocated PSRAM framebuffer to prepare the physical target, temporarily converts the previous-frame history to physical polarity, scans synchronously and restores logical history. Source images and font caches are not modified.

## Refresh routing

- DU20 when every changed pixel transitions between black and white.
- GL48 when either the old or new value of a changed pixel is gray. Unchanged gray does not promote an otherwise binary update to GL.
- Full GC48 for polarity changes, explicit full requests (including reader entry/cleaning), periodic cleaning and fault recovery.

The existing derived DU20/GL48 tables hold every equal-value pair, including black backgrounds. All changed pairs retain native vendor pulses. White-background animation shortcuts are bypassed; dark page turns use direct differential output. Waveform source tables, VCOM and power sequencing are unchanged.

Local menus and sliders preserve the requested logical crop through all four rotations. Pixels outside the crop stay at the physical baseline, even when sharing a packed byte or aligned 32-pixel scan column. Pending changes outside that crop remain pending. No-change submissions do not scan or advance the cleaning counter. Only successful changed page submissions advance the adapter's periodic counter; local controls do not. The reader's configured full-refresh interval is still respected.

AA text normally uses GL. Disable AA to exercise DU on binary text pages. Images are inverted as well; original image polarity is not preserved. Full refresh can still flash visibly. Ghosting and speed on a black background require optical validation on the S31.

## Validation

`tools/invert_display_host_test.py` checks the actual output adapter against the real highlevel rotation routine and an aligned-scan simulation: DU/GL/GC routing, all gray values, unchanged gray, exact crops, pending neighboring pixels, clipping, all rotations, no-op/control/page cycles, allocation failure, draw failure and toggle history. It also checks actual DU20/GL48 LUTs: equal pixels receive no pulse and every changed pair retains its native drive sequence.

`tools/invert_settings_host_test.py` checks NVS persistence, errors and default-off behavior. These host checks do not establish optical quality or on-device latency.

Hardware acceptance: repeated toggles; at least 50 turns with AA on/off; illustrated pages; opening/closing menus and frontlight sliders; rotation; sleep/wake/reboot; manual and periodic full refresh. Compare white-text edges, black-background ghosting, visible flashes and page latency.

## RC9 controls

Quick Settings includes a sixth contrast-icon toggle (bottom row, right). Its badge shares the persistent inversion preference with Settings > Display. Toggling keeps the quick layer open and performs one full presentation in the new polarity; a failed NVS save keeps the previous mode and reports a notice.

The "Dao mau" action can be assigned to AI short press, volume buttons, front buttons and configurable holds. Existing stored action IDs are unchanged; the new IDs are appended. These bindings request full presentation only after persistence succeeds. USB/OTA ownership guards remain in place. The configurable action menu keeps its existing ten-action capacity and content; this addition targets Quick Settings and hardware bindings.
