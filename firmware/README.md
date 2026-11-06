# Firmware source ownership

Maintain one copy of common code. Build both editions with `tools/build_firmware.py`.

- `shared/main/`: reader, formats, layout, fonts, common UI/input, settings, networking and storage interfaces. Common fixes apply to both builds.
- `full/library/`: Full shelf/management implementation fragments and the collage-lock implementation. These implementations are not included in Love Book.
- `full/ui/`: Full Home renderer.
- `love_book/ui/`: I Love Book renderer and actions. Full may reuse this optional interface.
- `love_book/apps/`: Books browser and USB entry.
- `love_book/no_library.*`: allocation-free compatibility exits for features absent from Love Book.

`main/` retains the ESP-IDF component metadata/assets and relative symlinks to owned sources for compatibility with existing tools. These links are not separate maintained copies. `layout.json` records ownership. The build driver materializes real compatibility files in ignored device/test views, so SDK/host tools do not need to compile symlinks. Board-specific overrides remain under `boards/metalio47/src/`; hardware configuration is independent of edition.

The reader and Home fragments intentionally share their owning translation unit: they use private reader/Home state without adding a second task, cross-module mutable globals or a duplicate implementation. Full fragments are included only by the Full build; I Love Book fragments are included wherever that interface is supported. Do not compile `.inc` files as standalone translation units.

For host tests, use `board_source_view.py --board metalio47 --tests --output build/<test-view>`; edition fragments are expanded in the generated test view. Direct function-extraction tests use `tools/firmware_sources.py`. Edit the owned files, never generated device views. Keep `layout.json` and compatibility links in sync when moving or adding an owned source.

Love Book's cover worker is shared with the current-book Home and reader lock cover, rather than being a library-only task. Its queue has one slot in Love Book and nine in Full. Shelf scanning remains excluded from Love Book; collage exits perform no allocation, card access or task creation. Remaining core reader/format/font/image buffers still follow shared lifecycle rules; folder separation by itself is not a device RAM measurement.
