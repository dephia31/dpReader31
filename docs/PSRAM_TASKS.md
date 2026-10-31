# S31 worker memory placement

Applies to Metalio47 only. ESP-IDF task stack sizes below are bytes.

| Worker | Stack | Placement |
| --- | ---: | --- |
| `book_prep` | 16384 | PSRAM for `/sdcard/` books; internal for flash books |
| `covers` | 12288 | PSRAM for SD requests; internal when queued work includes flash paths |
| avatar | 12288 | PSRAM; decoder accepts SD paths only |
| transfer stream writer | 4096 | PSRAM for SD uploads; flash uploads use the existing synchronous HTTP path |

These stack capacities total 44 KiB; they are not necessarily live simultaneously. This is an allocation change, not a measured peak-heap or speed improvement. The internal static reaper, TCB, queue and state cost approximately 2288 bytes in the S31 build. Worker TCBs and synchronization objects remain internal.

`metalio_task.h` requests explicit stack capabilities without silently falling back to SRAM on PSRAM allocation failure. Callers retain their existing failure/defer/synchronous behavior. Preserve priority, core affinity and resource ownership. Self-exiting workers release their resources, enqueue their handle and suspend; one static internal reaper uses `vTaskDeleteWithCaps(handle)`. External owners stop/join work before `metalio_task_delete`. Never use ordinary `vTaskDelete` for these WithCaps workers.

The helper suspends each target and waits until it leaves both CPU cores before calling external WithCaps deletion, so it does not depend on a local SDK synchronization patch; self-deletion through `vTaskDeleteWithCaps(NULL)` is deliberately avoided because it allocates another cleanup task. Host tests exercise allocation failure, lifecycle and stack capability selection; actual device stack watermarks and free SRAM still require hardware validation.

A PSRAM cover worker hands a later flash request to an internal worker before touching the file. Opening another book stops/deletes the previous prefetch worker under its draw lock before choosing the new storage class. Avatar and SD transfer paths cannot write internal flash. The SDMMC driver retains its existing internal DMA bounce buffer.

Main/settings/NVS, OTA, network/font download/sync, SD mount/recovery, display feed, keys/touch/haptics and BLE tasks retain internal stacks: these may use flash/cache-off, driver, interrupt or timing-sensitive paths and have not been proven eligible. DMA endpoints, ISR stacks and retention storage remain internal. Large image/font/reader data buffers already use PSRAM; no blanket heap or task migration is applied.

Reproduce the targeted lifecycle check with `python3 tools/metalio_psram_task_host_test.py`. Generate the selected host view with `python3 tools/board_source_view.py --board metalio47 --tests --output build/host/metalio47`, then run its cover, avatar, reader prefetch and transfer-stream host tests. Compile using `tools/build_firmware.py --board metalio47` with the matching SDK. Worker creation logs identify the actual stack capability and byte count.
