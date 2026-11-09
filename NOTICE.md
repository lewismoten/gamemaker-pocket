# Notices

Copyright (c) 2026 Lewis Moten. Licensed under the [MIT license](LICENSE). The license also carries the copyright line of gandrewstone, because the play core follows RSD Game-Maker 3.0.

This repository was split from [Game-Maker: Sandbox Edition](https://github.com/lewismoten/GameMaker-SandboxEdition) (`hardware/pocket-player/`, with its history). That project's [NOTICE.md](https://github.com/lewismoten/GameMaker-SandboxEdition/blob/develop/NOTICE.md) has the full credits of the games and tools that make the game packs.

## Where the play rules come from

- `src/gmcore.h`, `src/gmmon.h`: a port of the web engine of Game-Maker: Sandbox Edition (`web/packages/engine`, MIT), which follows `PLAYGAME.C` from **RSD Game-Maker 3.0** (Recreational Software Designs, 1991-1995; the source was released under the MIT License in 2014 by gandrewstone, github.com/gandrewstone/GameMaker). `gmmon.h` ports the engine's `monsters.ts`. The tune timing in `src/pcspeaker.h` (36 steps of 1/145.6 s) follows `code/GM/PLAYGAME.C` and the web engine.
- `data/`: the Starter games' own maps, blocks, palettes and hero files, packed by Game-Maker: Sandbox Edition (`web/scripts/pocketData.ts`), MIT as above. `data/library.jsonl` and the thumbnails come from the same packer. The games keep their own credits (their `LICENSE.TXT` files are in that repository).

## Third-party code and data

- **LovyanGFX 1.1.16** (lovyan03, FreeBSD/BSD-style license, with its bundled Adafruit_ILI9341, Adafruit_GFX and TFT_eSPI notices): the display library, fetched by PlatformIO from `platformio.ini` (`lib_deps`).
- **Arduino core for ESP32 2.0.16** (LGPL-2.1), **ESP-IDF v4.4.7** (Apache-2.0) and **Mbed TLS 2.28.7** (Apache-2.0, chosen from its Apache-2.0-or-GPL-2.0-or-later offer) are compiled into the firmware image through the `espressif32` platform (6.7.0). Their versions, copyright lines, source and license URLs, and the complete license texts are in the [firmware notices file](https://github.com/lewismoten/GameMaker-SandboxEdition/blob/develop/web/public/pocket/fw/NOTICES.txt) of Game-Maker: Sandbox Edition. Keep that file with any firmware image you redistribute.
- **ESP Web Tools 10.2.1** (Open Home Foundation / ESPHome, Apache-2.0), bundled unmodified in `docs/ewt/` for the browser installer, with esptool-js (Espressif Systems, Apache-2.0), @material/web (Google, Apache-2.0), improv-wifi-serial-sdk (Apache-2.0), Lit (Google, BSD-3-Clause) and tslib (Microsoft, 0BSD). The Apache-2.0 text is `docs/ewt/LICENSE-esp-web-tools.txt`; the complete per-part notices are `docs/ewt/THIRD-PARTY.txt`. Keep both with the folder. The firmware's notices are `docs/fw/NOTICES.txt`. The install page (`docs/index.html`, `docs/install.js`) follows the one in Game-Maker: Sandbox Edition (MIT).
- **PlatformIO** and its `espressif32` platform are build tools, not code redistributed in the firmware.
- **miniz / tinfl** (Rich Geldreich, public domain, with its later MIT-style option): the firmware calls `tinfl_decompress` through ESP-IDF's `rom/miniz.h`. The decoder is already in the ESP32's ROM, so no miniz source is compiled into the image.
- `src/picturetune.h` resends LovyanGFX's ST7789 and ILI9341 init values and offers alternate panel voltage and gamma-table values after **TFT_eSPI**'s ST7789 init (Bodmer, FreeBSD license) and common ST7789 and ILI9341 module examples. Table values only.
- **8x8 text font** (`data/font.bin`, `src/softfont.h`): the printable ASCII glyphs of Daniel Hepper's public-domain `font8x8_basic` (github.com/dhepper/font8x8), as used by Game-Maker: Sandbox Edition (`cp437Glyphs.ts`). The headers credit Marcel Sondaar and International Business Machines (public-domain VGA fonts). The glyph for A was compared with the upstream table; the rest is taken from that project's NOTICE.
- **Board pins**: the ESP32-3248S035 pin numbers come from community tables (homeding.github.io/boards/esp32/panel-3248S035.htm and community.home-assistant.io/t/help-making-esp32-3248s035-work/748332). The speaker pin (GPIO 26, the ESP32's DAC2, on the board's 2-pin speaker connector) comes from rzeldent's platformio-espressif32-sunton board definition `esp32-3248S035C.json` (`SPEAK=26`; github.com/rzeldent/platformio-espressif32-sunton, MIT). Only numbers are used, none of the code.
- The touch drivers (`src/gt911.h`, `src/xpt2046.h`) use no touch library; they read the GT911 over I2C and the XPT2046 over GPIO directly, written for this project.

## AI tools and models

Directed by Lewis Moten. Credits are as recorded in Game-Maker: Sandbox Edition's NOTICE.md and this code's history.

- **Claude** (Sonnet 5.5, Anthropic), through Claude Code: wrote the firmware (the play loop, touch, display setup and the software turn, settings, the library browser and the other screens, the speaker and PC-speaker tune player), the host tests, the board tools in `scripts/` and this repository's documentation.
- **OpenAI Codex** (GPT-6; the later host-hardening work through **Hermes Agent** running GPT-6.1 Sol): the category and six-thumbnail library browser, library metadata and card cache, search, display options and setup flow, touch packet handling, text-page and full-panel clearing, resistive slide fixes, host failure probes, per-level host frame profiling, and the heap audit tools (`scripts/pocket-heap-*.py`, `pocket-inventory-test.py`). Host timings are not CYD measurements.
- **ChatGPT image generation** (OpenAI): the "How to hold it" hand art (`art/hand-left.svg`), from Lewis Moten's prompts and shaping. Claude converted it to firmware polygons (`tools/svgHand.py`, `src/handshape.h`), wrote the drawing code (`src/handdraw.h`) and chose the mint-green coloring.
- **ChatGPT** (GPT-6.1 Sol, OpenAI): the social preview (`docs/images/social-preview-*.png`) and the favicon (`art/favicon.ico`), from Lewis Moten's directions.
- **Claude** (Sonnet 5.5) drew the 19 small category icons in code (`src/icons_data.h`, generated by Game-Maker: Sandbox Edition's `web/scripts/pocketArt.ts`), the Search and Settings icons and the Starter tiles' captions (`src/picker_draw.h`). The boot splash (`src/splash_data.h`) is the last frame of that project's own Sandbox sting. The Bizarre icon is the rooster in blue spectacles from the top of The Bard of Bizarre's logo (made with **DeepAI**), redrawn by Claude as 16x16 pixel art.
