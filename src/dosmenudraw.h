// What PLAYGAME draws for the game menu, the Top Ten and the name entry, as 320 x 200 palette indices (the 8 x 8 font of
// gmcards.h), so the board and the Mac renders (host/dosmenushots.cpp) show the same pixels. Colors are found the way the engine
// finds them: the palette entry closest to RGBdata(10,10,63) for "Blue" and (63,10,10) for "Red" (Match).
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "gmcards.h"
#include "gmhiscore.h"
#include "gmmenu.h"

namespace gm {

// RGBdata::Match: the palette entry (6 bit values, `pal768`) nearest to r, g, b by squared distance; the first of equals.
inline int matchColor(const uint8_t* pal768, int r, int g, int b) {
  int best = 0, bd = 1 << 30;
  for (int i = 0; i < 256; i++) {
    int dr = (pal768[i * 3] & 63) - r, dg = (pal768[i * 3 + 1] & 63) - g, db = (pal768[i * 3 + 2] & 63) - b, d = dr * dr + dg * dg + db * db;
    if (d < bd) { bd = d; best = i; }
  }
  return best;
}
inline int dosBlue(const uint8_t* pal768) { return matchColor(pal768, 10, 10, 63); }
inline int dosRed(const uint8_t* pal768) { return matchColor(pal768, 63, 10, 10); }

// GWrite: the string at (x, y), 8 x 8 glyphs, one color (bit 0 of a row is the left pixel). Returns the right edge.
inline int dosWrite(uint8_t* px, const uint8_t* font, int x, int y, int col, const char* s) {
  for (; *s; s++, x += 8) {
    unsigned char ch = (unsigned char)*s;
    if (ch < 32 || ch > 126) ch = '?';
    const uint8_t* g = font + (ch - 32) * 8;
    for (int r = 0; r < 8; r++) {
      if (y + r < 0 || y + r >= PIC_H) continue;
      for (int b = 0; b < 8; b++) if ((g[r] & (1 << b)) && x + b >= 0 && x + b < PIC_W) px[(y + r) * PIC_W + x + b] = (uint8_t)col;
    }
  }
  return x;
}
inline void dosBox(uint8_t* px, int x0, int y0, int x1, int y1, int col) {  // BoxFill, inclusive
  for (int y = y0 < 0 ? 0 : y0; y <= y1 && y < PIC_H; y++) for (int x = x0 < 0 ? 0 : x0; x <= x1 && x < PIC_W; x++) px[y * PIC_W + x] = (uint8_t)col;
}

// DrawDefaultScrn, for a game with no menu picture: nested boxes shrinking to the middle, each one palette entry (the engine used
// random colors; this is a steady ramp so the screen is the same every time), entries 250 and 251 are Blue and Red. Fills the 768 + 64000.
inline void defaultMenuScreen(uint8_t* pic) {
  uint8_t* pal = pic;
  uint8_t* px = pic + 768;
  memset(pic, 0, PIC_BYTES);
  for (int l = 0; l <= 100; l++) {  // a blue to violet ramp, dark at the edge
    pal[l * 3] = (uint8_t)(6 + l * 20 / 100);
    pal[l * 3 + 1] = (uint8_t)(4 + l * 8 / 100);
    pal[l * 3 + 2] = (uint8_t)(18 + l * 30 / 100);
  }
  pal[250 * 3] = 10; pal[250 * 3 + 1] = 10; pal[250 * 3 + 2] = 63;
  pal[251 * 3] = 63; pal[251 * 3 + 1] = 10; pal[251 * 3 + 2] = 10;
  for (int l = 0; l <= 100; l++) {  // Box(40 - l*4/10, 100 - l, 280 + l*4/10, 180 + l*2/10, l), as frames, the inner ones over the outer
    int x0 = 40 - l * 4 / 10, y0 = 100 - l, x1 = 280 + l * 4 / 10, y1 = 180 + l * 2 / 10;
    for (int x = x0; x <= x1; x++) for (int y : {y0, y1}) if (x >= 0 && x < PIC_W && y >= 0 && y < PIC_H) px[y * PIC_W + x] = (uint8_t)l;
    for (int y = y0; y <= y1; y++) for (int x : {x0, x1}) if (x >= 0 && x < PIC_W && y >= 0 && y < PIC_H) px[y * PIC_W + x] = (uint8_t)l;
  }
}

// GameMenu: the title at (96, 104) and the six words, all in Blue.
inline void drawGameMenu(uint8_t* pic, const uint8_t* font, const char* title) {
  int blue = dosBlue(pic);
  char t[41];
  snprintf(t, sizeof t, "%s", title ? title : "");
  dosWrite(pic + 768, font, gmmenu::TITLE_X, gmmenu::TITLE_Y, blue, t);
  for (int c = 1; c <= gmmenu::M_COUNT; c++) dosWrite(pic + 768, font, gmmenu::WORD_X, gmmenu::rowY(c), blue, gmmenu::word(c));
}
// GVertMenu's marker beside choice c, in Blue (draw = true), or the saved pixels put back (false): `under` holds the marker column
// (6 x 4 per row of the menu) as it was before any marker, filled by menuUnder.
constexpr int MARK_W = 6, MARK_H = 4;
inline void menuUnder(const uint8_t* px, uint8_t under[gmmenu::M_COUNT][MARK_W * MARK_H]) {
  for (int c = 1; c <= gmmenu::M_COUNT; c++) {
    int x0, y0, x1, y1;
    gmmenu::marker(c, x0, y0, x1, y1);
    for (int r = 0; r < MARK_H; r++) for (int b = 0; b < MARK_W; b++) under[c - 1][r * MARK_W + b] = px[(y0 + r) * PIC_W + x0 + b];
  }
}
inline void drawMenuMarker(uint8_t* px, int c, bool on, int blue, const uint8_t under[gmmenu::M_COUNT][MARK_W * MARK_H]) {
  int x0, y0, x1, y1;
  gmmenu::marker(c, x0, y0, x1, y1);
  for (int r = 0; r < MARK_H; r++) for (int b = 0; b < MARK_W; b++) px[(y0 + r) * PIC_W + x0 + b] = on ? (uint8_t)blue : under[c - 1][r * MARK_W + b];
}
// The game pixels the marker of choice c covers, as a TextRect for carddraw::pushTextRect.
inline TextRect markerRect(int c) { int x0, y0, x1, y1; gmmenu::marker(c, x0, y0, x1, y1); TextRect r; r.x0 = x0; r.y0 = y0; r.x1 = x1 + 1; r.y1 = y1 + 1; return r; }

// ShowHighScores: "Top Ten Scores" at (104, 0) in Red, the ten records from y = 27 (hiRowY) at x = 20 in Blue as "%10ld name",
// the latest score under them when it is above 0, and "Hit any key" at (116, 192) in Red.
inline void drawTopTen(uint8_t* pic, const uint8_t* font, const HiTable& t, long latest) {
  uint8_t* px = pic + 768;
  int red = dosRed(pic), blue = dosBlue(pic);
  dosWrite(px, font, 104, 0, red, "Top Ten Scores");
  for (int j = 0; j < HIS_COUNT; j++) {
    if (t.top[j].score == 0) continue;
    char s[50];
    snprintf(s, sizeof s, "%10ld %s", (long)t.top[j].score, t.top[j].name);
    dosWrite(px, font, 20, hiRowY(j), blue, s);
  }
  if (latest > 0) {
    char s[50];
    snprintf(s, sizeof s, "%10ld Your Latest Score", latest);
    dosWrite(px, font, 20, hiRowY(HIS_COUNT), blue, s);
  }
  dosWrite(px, font, 116, 192, red, "Hit any key");
}

}  // namespace gm
