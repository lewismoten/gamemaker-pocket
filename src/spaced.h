// Letter-spaced text for the firmware's own menus. The 6 x 8 bitmap font fills its whole cell, so letters drawn one cell apart touch
// and are hard to read; these draw a glyph at a time with a gap after each (1 px at size 1 and 1.5, 2 px at size 2). Games draw their
// own text (PLAYGAME's font) and never come through here.
#pragma once
#include <string.h>
#include "softfont.h"

namespace sp {
inline int gap(float size) { return size >= 2 ? 2 : 1; }
inline int adv(float size) { return (int)(6 * size + 0.5f) + gap(size); }  // one glyph's step, gap included
inline int width(int n, float size) { return n * adv(size) - (n ? gap(size) : 0); }  // n glyphs, ink only
// Button labels are tighter: no extra gap (the font has its own blank column) (the keyboard, slot list and search keep gap()).
constexpr int BTN_GAP = 0;
inline int btnWidth(int n, float size) { return n * ((int)(6 * size + 0.5f) + BTN_GAP) - (n ? BTN_GAP : 0); }
inline int fit(int room, float size) { return room > 0 ? (room + gap(size)) / adv(size) : 0; }  // glyphs that fit in `room` px

// The text with its top left at x, y (the caller has set size and colors).
template <class G> void print(G& g, int x, int y, const char* s, float size, int gp = -1) {
  int step = gp < 0 ? adv(size) : (int)(6 * size + 0.5f) + gp;
  for (; *s; s++, x += step) {
    char c[2] = {*s, 0};
    g.setCursor(x, y);
    g.print(c);
  }
}
// The text centered on cx, cy.
template <class G> void centerBtn(G& g, int cx, int cy, const char* s, float size) {
  print(g, cx - btnWidth((int)strlen(s), size) / 2, cy - (int)(8 * size + 0.5f) / 2, s, size, BTN_GAP);
}
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
// ---- Proportional text (the pause menu and its buttons): each glyph is drawn from its leftmost ink column and the next one starts
// exactly 1 font pixel after its rightmost, so a word has one pixel between letters ("i" and "l" are narrow, "W" and "M" wide).
// Positions count in font pixels and are scaled once (size 1.5 and 2 give the scaled pixel). A space is 3 font pixels. Digits keep
// one fixed width (the widest digit's), so a number such as a level does not jiggle. The table is built once at startup.
struct InkTable {
  uint8_t l[95], w[95];  // leftmost ink column and ink width of each glyph (0 wide = blank)
  uint8_t dl, dw;        // digits: the shared left column and the shared width
  InkTable() {
    int dmin = 6, dmax = -1;
    for (int g = 0; g < 95; g++) {
      int lo = 6, hi = -1;
      for (int r = 0; r < 8; r++) for (int c = 0; c < 6; c++) if (SOFT_FONT[g][r] >> c & 1) { if (c < lo) lo = c; if (c > hi) hi = c; }
      l[g] = hi < 0 ? 0 : lo;
      w[g] = hi < 0 ? 0 : hi - lo + 1;
      if (g >= '0' - 32 && g <= '9' - 32 && hi >= 0) { if (lo < dmin) dmin = lo; if (hi > dmax) dmax = hi; }
    }
    dl = dmin; dw = dmax - dmin + 1;
  }
};
inline const InkTable& ink() { static const InkTable t; return t; }
constexpr int SPACE_UNITS = 3;
inline int gIdx(char c) { return (unsigned char)c >= 32 && (unsigned char)c <= 126 ? (unsigned char)c - 32 : 0; }
inline int pLeft(char c) { return (c >= '0' && c <= '9') ? ink().dl : ink().l[gIdx(c)]; }
// The font pixels one glyph moves the pen: its ink plus the 1 px gap.
inline int pUnits(char c) {
  if (c >= '0' && c <= '9') return ink().dw + 1;
  int w = ink().w[gIdx(c)];
  return w ? w + 1 : SPACE_UNITS;
}
inline int pScale(int units, float size) { return (int)(units * size + 0.5f); }
// n glyphs of s: its width in px, ink only (the last glyph's gap is left off).
inline int pwidth(const char* s, float size, int n = -1) {
  int u = 0, k = 0;
  for (; s[k] && (n < 0 || k < n); k++) u += pUnits(s[k]);
  return k ? pScale(u - 1, size) : 0;
}
// s with its first ink at x, top at y (the caller has set size and colors).
template <class G> void pprint(G& g, int x, int y, const char* s, float size) {
  int u = 0;
  for (; *s; s++) {
    if (*s != ' ') {
      char c[2] = {*s, 0};
      g.setCursor(x + (int)(u * size) - (int)(pLeft(*s) * size), y);
      g.print(c);
    }
    u += pUnits(*s);
  }
}
template <class G> void pcenter(G& g, int cx, int cy, const char* s, float size) {
  pprint(g, cx - pwidth(s, size) / 2, cy - (int)(8 * size + 0.5f) / 2, s, size);
}
// How many leading glyphs of s fit in `room` px.
inline int pfit(const char* s, int room, float size) {
  int n = 0;
  while (s[n] && pwidth(s, size, n + 1) <= room) n++;
  return n;
}
// s cut to `room` px with "..." (into out, outSize bytes); never wider than room.
inline void pellipsize(const char* s, char* out, int outSize, int room, float size) {
  int len = (int)strlen(s);
  if (len < outSize && pwidth(s, size) <= room) { memcpy(out, s, len + 1); return; }
  int n = pfit(s, room, size);
  if (n > outSize - 4) n = outSize - 4;
  while (n > 0) {
    memcpy(out, s, n); memcpy(out + n, "...", 4);
    if (pwidth(out, size) <= room) return;
    n--;
  }
  strcpy(out, "...");
  if (pwidth(out, size) > room) out[0] = 0;
}
}  // namespace sp
