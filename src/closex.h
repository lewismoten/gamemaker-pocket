// The Close X: one button, one glyph, one place (top right, a few pixels in from the glass) on every screen that can be
// closed. What it does is the screen's old Back: it only says "close". Drawn with fillRect so any panel or the host canvas
// can use it; no text font. Sizes: at least 36 px tall on the 2.8 inch and 40 x 48 on the 3.5 inch (the same hit rectangle).
#pragma once
#include <stdint.h>
namespace cx {
constexpr uint16_t BLUE = 0x2A9A, YELLOW = 0xFEAA, WHITE = 0xFFFF;  // the Search action keys' colors (pk::C_ACTION, C_YELLOW)
constexpr int INSET = 2;
struct Rect { int x0, y0, x1, y1; bool has(int x, int y) const { return x >= x0 && x < x1 && y >= y0 && y < y1; } };
inline int heightFor(int W) { return W <= 320 ? 36 : 40; }
inline int widthFor(int W) { return W <= 320 ? 40 : 48; }
inline int bandH(int W) { return INSET + heightFor(W); }  // a header that holds the X is at least this tall
inline Rect box(int W) { return {W - INSET - widthFor(W), INSET, W - INSET, INSET + heightFor(W)}; }
// The hit area takes the few pixels above the glass inset too, so a tap at the very top edge still lands.
inline bool hit(int W, int x, int y) { Rect b = box(W); return x >= b.x0 && x < b.x1 && y >= 0 && y < b.y1; }
template <class G> void draw(G& g, int W) {
  Rect b = box(W);
  int w = b.x1 - b.x0, h = b.y1 - b.y0;
  g.fillRoundRect(b.x0, b.y0, w, h, 6, BLUE);
  g.drawRoundRect(b.x0, b.y0, w, h, 6, YELLOW);
  g.drawRoundRect(b.x0 + 1, b.y0 + 1, w - 2, h - 2, 5, YELLOW);
  int n = W <= 320 ? 14 : 16, t = W <= 320 ? 4 : 5;  // the X: n x n, strokes t thick
  int cx = (b.x0 + b.x1) / 2 - n / 2, cy = (b.y0 + b.y1) / 2 - n / 2;
  for (int i = 0; i < n; i++) {
    g.fillRect(cx + i, cy + i, t, 1, WHITE);
    g.fillRect(cx + n - t - i, cy + i, t, 1, WHITE);
  }
}
}  // namespace cx
