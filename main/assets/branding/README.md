# dpReader logo assets

The user supplied `dpreader-logo-source.png`. Preserve its wordmark and open-book underline.

## Firmware

`../startup_logo_4bpp.bin` is generated from the original monochrome source at 540 × 178, centered by the startup renderer. The runtime reads its packed pixels directly from flash; it does not allocate a logo framebuffer. The existing startup caption and retained lock-screen path are preserved.

Regenerate with Pillow installed:

```sh
python3 tools/gen_startup_logo.py
```

The generated header records the source SHA-256. Do not hand-edit the packed bitmap or its dimensions.

## Red Studio

`dpreader-logo-red-studio-master.png` is the selected output from the built-in image_gen tool. The design uses near-white “dp” and a red “Reader”/book underline on a dark background, based on the existing UI palette: accent `#FF3C50`, foreground `#F1F1F5`, background `#0B0B10`.

`web/assets/dpreader-logo-red-studio.png` is a 512 × 176 web-size derivative. It crops empty dark margins (luminance above 80 finds the mark, with 20 source pixels of padding), then resizes with Lanczos. The master remains unchanged.

Both web headers consume the same PNG. The local transfer page embeds its bytes to work without Internet or an additional device route. After changing the web PNG, run:

```sh
python3 tools/sync_brand_logo.py
```

These are local source changes; publishing Web Flasher remains a separate release action.

## Final image-generation prompt

Mode: built-in image_gen edit, original user image as the edit target, opaque background.

Use case: precise-object-edit. Edit the ORIGINAL attached black-on-white dpReader logo, preserving its exact serif glyph silhouettes, spacing, and open-book underline. Change only colors: lowercase 'dp' solid flat #F1F1F5, 'Reader' solid flat #FF3C50, entire book underline solid flat #FF3C50. Replace white backdrop with a perfectly uniform solid #0B0B10 backdrop, matching a dark web UI. Critical cleanup: absolutely smooth continuous filled letter interiors, clean antialiased edges, clean transparent-looking counters using the same background #0B0B10. NO stray specks, grunge, texture, distressed edges, outlines, shadow, glow, bevel, lighting, gradient, extra marks, or new text. This is a clean flat production brand asset, not a textured illustration. The text must be exactly 'dpReader'. Keep full logo uncropped in a 3:1 horizontal canvas with modest background padding. Use only the stated colors apart from antialiasing at edges.
