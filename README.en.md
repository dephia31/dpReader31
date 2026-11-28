# dpReader — Give your Metalio a new chapter

[Tiếng Việt](README.md) · **English**

<p align="center">
  <a href="https://dephia31.github.io/dpReader31/?lang=en"><img src="https://img.shields.io/badge/Web%20Flash-Install%20from%20your%20browser-D52843?style=for-the-badge&amp;logo=googlechrome&amp;logoColor=white" alt="Open Web Flash to install dpReader"></a>
  <a href="https://github.com/dephia31/dpReader31/releases/tag/v1.7.3-vn"><img src="https://img.shields.io/badge/dpReader-1.7.3-222?style=for-the-badge&amp;logo=github" alt="dpReader 1.7.3"></a>
</p>

**Install or update dpReader directly in Chrome or Edge.** Connect your Metalio with a USB-C data cable and open Web Flash. No desktop installer is required.

| <a href="web/assets/home-full.webp"><img src="docs/screenshots/interface/readme/home-full.png" width="170" height="255" alt="Full Home"></a> | <a href="web/assets/home-love-book.webp"><img src="docs/screenshots/interface/readme/home-love-book.png" width="170" height="255" alt="I Love Book"></a> | <a href="web/assets/home-new.webp"><img src="docs/screenshots/interface/readme/home-new.png" width="170" height="255" alt="New Home"></a> | <a href="docs/screenshots/interface/library.png"><img src="docs/screenshots/interface/readme/library.png" width="170" height="255" alt="Library"></a> |
| :---: | :---: | :---: | :---: |
| Full Home | I Love Book | New Home | Library |

## dpReader 1.7.3

Firmware for reading EPUB and TXT on the **Metalio E-Ink 4 Plus** (`metalio4.7`, 4.7 inches, ESP32-S31, 16 MB flash).

<p align="center">
  <img src="docs/screenshots/showcase/hero.jpg" width="460" alt="dpReader on Metalio E-Ink 4 Plus">
</p>

[Web Flash in English](https://dephia31.github.io/dpReader31/?lang=en) · [Download firmware 1.7.3](https://github.com/dephia31/dpReader31/releases/tag/v1.7.3-vn)

### 1. New Lock Screens: Cover Collage (C1/C3/C4) & Current Book Cover (B1/B3)

<p align="center">
  <img src="docs/screenshots/showcase/lock-collage.jpg" width="680" alt="Cover Collage Lock Screen — C3 & C4">
</p>

- **Cover Collage (Full)**: Much faster rendering with 3 layouts — **C1** (classic tilted), **C3** (3-column masonry + floating card), and **C4** (8° tilt, clean gray shadow without Polaroid frame, translucent dark band). Pick up to **48 books**, 3 densities, and **Recent / Random per lock / Fixed random** order, plus an instant **Shuffle** button with tap highlight and `#N` counter.

<p align="center">
  <img src="docs/screenshots/showcase/lock-currentbook.jpg" width="680" alt="Current Book Cover Lock Screen — B1 & B3">
</p>

- **Current Book Cover (Full & Love Book)**: Displays your active book cover, chapter, `%` progress bar, and personal signature in 2 styles: **B1** (floating rounded card) and **B3** (translucent dark info band above a solid black signature strip).

### 2. Print-Sharp Text: `dpMetal Core`, `dpMetal Bold` & new `dpMetal Dark`

<p align="center">
  <img src="docs/screenshots/showcase/dpmetal-macro.jpg" width="680" alt="dpMetal macro typography rendered on E-Ink">
</p>

- **Deeper blacks without trailing washout**: Tuned `GC48`, `GC36`, `GL37`, and `22-phase` waveforms with **Core 1 RISC-V PIE SIMD** acceleration keep strokes dark and crisp through the final phase.
- **New `dpMetal Dark` mode (under `dpMetal Core`)**: Preserves `dpMetal Core`'s crisp thin strokes with fast 22-phase page turns and `GC48` periodic full refreshes for noticeably deeper black contrast.
- **`dpMetal Bold` & crisp UI labels**: Uniform black strokes across all font sizes without end-of-turn dark flashes; reading menus, TOC, choice popups, and Quick Settings use `dpMetal Bold` label rasterization.
- **Smart defaults**: Keeps **`dpMetal Core`** as default, enables **Large raised Drop Cap**, and turns **Embedded book fonts** off by default.

### 3. Dual Water-Ripple Turns (22-Phase & 37-Phase), SRAM Safety & Swipe Back

<p align="center">
  <img src="docs/screenshots/showcase/menus.jpg" width="680" alt="Fast reading menus and page-turn options">
</p>

- **Dual Water-Ripple options**: Choose **Fast water ripple (22-phase)** for speed or **Smooth water ripple (37-phase)** for fluid page transitions with deep black text.
- **Internal SRAM safety & power protection**: Places I2C, touch, I/O-expander, and accelerometer structures in internal SRAM (`CONFIG_I2C_ISR_IRAM_SAFE=y`), adds critical-battery safe shutdown (saving progress and syncing SD), and cleanly tears down I/O, haptics, and display clocks for Deep Sleep.
- **Fast reading menus & left-edge swipe back**: Opens and closes top/bottom toolbars in a single refresh pass without touching body text; swipe right from the left edge to go back across all reading menus (portrait & landscape) and TOC.

### 4. Quick Settings & Soft-Start Frontlight Wake

<p align="center">
  <img src="docs/screenshots/showcase/quick-settings.jpg" width="460" alt="Quick Settings 10 toggles and 3 custom frontlight presets">
</p>

- Pull down from the top edge for **Quick Settings** with **10 quick toggles**, **3 custom frontlight presets** (UTF-8 Vietnamese renameable), and **brightness / warmth sliders**.
- **Soft-start frontlight ramp** unified across both **Light Sleep** and **Deep Sleep** wake.

### 5. In-Book Search, Smart TOC & Connectivity

<p align="center">
  <img src="docs/screenshots/showcase/search-toc.jpg" width="680" alt="In-book search and Table of Contents">
</p>

- **In-book search (TXT & EPUB)**: Accent-insensitive & accented Vietnamese matching, recent queries, instant Stop & view results, and quick search from text selection.
- **Nested TOC & percentage (%) jump**: Full portrait and landscape support.
- **Connectivity**: **KOSync** progress sync, **OPDS** catalogs, **Wi-Fi / Hotspot / USB** transfers, and **Bluetooth page-turner** support.

## Choose an edition and a Home layout

<p align="center">
  <img src="docs/screenshots/showcase/editions.jpg" width="760" alt="Full Home, I Love Book, and New Home layouts">
</p>

- **Full** includes a cover library, search, favorites, history, book management, and the **Cover Collage** lock screen. Full Home shows your current book, shortcuts and recent books.
- **Love Book** opens books from a file picker. The I Love Book layout centers on the current book, with a partly open cover, progress, and **Current Book Cover** lock screen.
- **New Home** is available in both editions. Tap **Continue reading** to open your book or tap **Today** to see your statistics. Change layouts under **Settings → Personalization → Home layout**.

## Reading and progress

Adjust fonts, text size, margins, line spacing and page-turn buttons. **Reading statistics** shows today's minutes, monthly reading time and active days, total reading time and a seven-day chart. Set a daily target under **Reading goal**.

<p align="center">
  <img src="docs/screenshots/interface/reading-statistics.png" width="190" alt="Reading statistics">
  <img src="docs/screenshots/interface/welcome.png" width="160" alt="Welcome and language selection">
</p>

## Install or update

1. Back up your settings to microSD. Keep a separate copy of your pre-upgrade backup if you may return to 1.5.1. Backups carrying a language choice in 1.5.2 use format V33.
2. Open **Web Flash** in a browser with Web Serial support, connect using a USB-C data cable and choose Full or Love Book. If the device does not appear, follow the page's BOOT instructions.
3. Choose **Application update** for dpReader 1.4.2 or later with the compatible 256 KiB NVS layout. Choose **Full install** when switching from other firmware, upgrading from 1.4.1 or earlier, or recovering a device. Full installation erases internal storage and preserves the microSD card.
4. Check the firmware version after restart. Devices with a compatible layout can update over Wi-Fi from 1.4.3 or use the microSD update package.

[Full USB bundle](releases/dpReader-metalio47-1.7.3-vn-USB.zip) · [Love Book USB bundle](releases/love-book/dpReader-metalio47-1.7.3-love-vn-USB.zip) · [Backup guide in Vietnamese](docs/BACKUP_AND_RESTORE.md)

## Transfer books and change language

Copy EPUB or TXT books to the books folder on microSD, or open **Files** to transfer over Wi-Fi, hotspot or USB. Use connection settings to browse OPDS catalogs and configure KOSync.

The first-run **Welcome** screen offers Vietnamese and English. Change the language under **Settings → Personalization → Language**. Book titles, author names and book contents retain their original text.

[Detailed Vietnamese guide](docs/CAI_DAT.md) · [License](LICENSE) · [Third-party notices](THIRD_PARTY_NOTICES.md)
