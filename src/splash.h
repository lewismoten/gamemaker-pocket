// The boot splash: the castle art from the UI pack (art/ui-pack, splash_data.h made by tools/splashPack.py), run-length coded in app flash
// and drawn one row at a time, so it needs no frame buffer. It exists at 320x240 and 480x320, like the boards; any other size gets a plain
// background. panelOrder swaps the bytes for the panel, as thumbnail() does. The pack leaves (0, H-24, W, 16) blank for the live status line.
#pragma once
#include <stdint.h>
#include <string.h>
#include "splash_data.h"
#include "softfont.h"
namespace pk {
constexpr uint16_t SPLASH_BG = 0x10A5;  // #101428, the pack's background
template <class G> void drawSplash(G& g, int W, int H, bool panelOrder = false) {
  const uint16_t *pal = nullptr, *rle = nullptr;
  int n = 0;
#if !defined(ARDUINO) || defined(POCKET_CYD28)
  if (W == 320 && H == 240) { pal = SPLASH320_PAL; rle = SPLASH320_RLE; n = SPLASH320_RUNS; }
#endif
#if !defined(ARDUINO) || !defined(POCKET_CYD28)
  if (W == 480 && H == 320) { pal = SPLASH480_PAL; rle = SPLASH480_RLE; n = SPLASH480_RUNS; }
#endif
  g.fillRect(0, 0, W, H, panelOrder ? (uint16_t)((SPLASH_BG << 8) | (SPLASH_BG >> 8)) : SPLASH_BG);
  if (!rle) return;
  uint16_t row[480];
  int x = 0, y = 0;
  for (int i = 0; i < n; i++) {
    uint16_t c = pal[rle[i] >> 12];
    if (panelOrder) c = (c << 8) | (c >> 8);
    for (int k = rle[i] & 0xFFF; k > 0; k--) row[x++] = c;
    if (x >= W) { g.pushImage(0, y++, W, 1, row); x = 0; }
  }
}
// One line of text on the pack's status row (what the board is doing), at most W/12 characters, centered, in the 5x7 soft font at 2x, in
// the pack's pale blue-gray. The row is cleared first. (The old fillRect color 0 was black; the pack's background is #101428.)
template <class G> void splashLine(G& g, int W, int H, const char* text, bool panelOrder = false) {
  int y = H - 24, n = (int)strlen(text), x0 = (W - n * 12) / 2;
  uint16_t bg = SPLASH_BG, fg = 0xA5B9;  // #A0B4C8
  if (panelOrder) { bg = (bg << 8) | (bg >> 8); fg = (fg << 8) | (fg >> 8); }
  g.fillRect(0, y, W, 16, bg);
  for (int i = 0; i < n; i++) {
    unsigned char ch = (unsigned char)text[i];
    if (ch < 32 || ch > 126) continue;
    for (int yy = 0; yy < 8; yy++) for (int xx = 0; xx < 6; xx++)
      if (SOFT_FONT[ch - 32][yy] & (1 << xx)) g.fillRect(x0 + i * 12 + xx * 2, y + yy * 2, 2, 2, fg);
  }
}
}  // namespace pk
