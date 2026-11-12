// Letter-spaced text for the firmware's own menus. The 6 x 8 bitmap font fills its whole cell, so letters drawn one cell apart touch
// and are hard to read; these draw a glyph at a time with a gap after each (1 px at size 1 and 1.5, 2 px at size 2). Games draw their
// own text (PLAYGAME's font) and never come through here.
#pragma once
#include <string.h>

namespace sp {
inline int gap(float size) { return size >= 2 ? 2 : 1; }
inline int adv(float size) { return (int)(6 * size + 0.5f) + gap(size); }  // one glyph's step, gap included
inline int width(int n, float size) { return n * adv(size) - (n ? gap(size) : 0); }  // n glyphs, ink only
inline int fit(int room, float size) { return room > 0 ? (room + gap(size)) / adv(size) : 0; }  // glyphs that fit in `room` px

// The text with its top left at x, y (the caller has set size and colors).
template <class G> void print(G& g, int x, int y, const char* s, float size) {
  for (; *s; s++, x += adv(size)) {
    char c[2] = {*s, 0};
    g.setCursor(x, y);
    g.print(c);
  }
}
// The text centered on cx, cy.
template <class G> void center(G& g, int cx, int cy, const char* s, float size) {
  print(g, cx - width((int)strlen(s), size) / 2, cy - (int)(8 * size + 0.5f) / 2, s, size);
}
// s cut to `room` px with "..." (into out, outSize bytes); never longer than fits.
inline void ellipsize(const char* s, char* out, int outSize, int room, float size) {
  int n = fit(room, size), len = (int)strlen(s);
  if (n >= outSize) n = outSize - 1;
  if (len <= n) { memcpy(out, s, len + 1); return; }
  if (n <= 3) { out[n > 0 ? n : 0] = 0; if (n > 0) memcpy(out, s, n); return; }
  memcpy(out, s, n - 3);
  memcpy(out + n - 3, "...", 4);
}
}  // namespace sp
