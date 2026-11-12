// Card and loading messages: the card chip on the home page, the notice that replaces an empty page, the loading bar and the
// failure card shown instead of a reset. Pure drawing and decisions (no hardware), so host/librarytest.cpp renders them all.
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "picker_draw.h"
namespace pk {
enum CardState : uint8_t { CARD_UNKNOWN, CARD_READING, CARD_READY, CARD_NONE, CARD_UNREADABLE, CARD_NOFOLDER };
enum LoadFail : uint8_t { FAIL_MEMORY, FAIL_FILE, FAIL_CARD };
// What the boot scan found: no mount = no card (the driver cannot tell an empty slot from a card it cannot start);
// mounted but the top folder will not open = unreadable; mounted, root opens, /pocket does not = no folder.
inline CardState cardStateAfterMount(bool mounted, bool rootOpens, bool pocketOpens) {
  if (!mounted) return CARD_NONE;
  if (!rootOpens) return CARD_UNREADABLE;
  return pocketOpens ? CARD_READING : CARD_NOFOLDER;
}
// Why a game could not start. A game on the card whose card is gone first; a missing buffer with little memory is memory.
inline LoadFail whyFailed(bool onCard, bool cardThere, bool missingBuffer, uint32_t largestFree) {
  if (onCard && !cardThere) return FAIL_CARD;
  if (missingBuffer && largestFree < 70000) return FAIL_MEMORY;
  return FAIL_FILE;
}
inline void cardChipText(CardState s, int count, char* out, size_t cap) {
  switch (s) {
    case CARD_READING: snprintf(out, cap, "Reading card: %d", count); break;
    case CARD_READY: snprintf(out, cap, count == 1 ? "Card: %d game" : "Card: %d games", count); break;
    case CARD_NONE: snprintf(out, cap, "No card"); break;
    case CARD_UNREADABLE: snprintf(out, cap, "Card unreadable"); break;
    case CARD_NOFOLDER: snprintf(out, cap, "No /pocket folder"); break;
    default: out[0] = 0;
  }
}
// In the footer of the home page, to the right of the page arrows (the pack's SD READY). Problems are yellow.
template <class G> void drawCardChip(G& g, int W, CardState s, int count) {
  char t[32];
  cardChipText(s, count, t, sizeof t);
  if (!t[0]) return;
  int sc = W == 320 ? 1 : 2, f = footer(W), h = (W == 320 ? 240 : 320) - f - 6, w = (int)strlen(t) * 6 * sc;
  int x1 = W - 4, x0 = x1 - w - 12;
  if (x0 < 120) return;  // never over the page arrows
  bool bad = s == CARD_NONE || s == CARD_UNREADABLE || s == CARD_NOFOLDER;
  g.fillRoundRect(x0, f + 3, w + 12, h, 6, C_BLACK);
  g.drawRoundRect(x0, f + 3, w + 12, h, 6, bad ? C_YELLOW : C_GRAY);
  Tile clip = {x0, f + 3, w + 12, h, 0, 0};
  clippedText(g, clip, t, x0 + 6, f + 3 + (h - 8 * sc) / 2, sc, bad ? C_YELLOW : C_WHITE);
}
// Words wrapped to `cols` characters, at most `max` lines of 40.
inline int wrapText(const char* s, int cols, char (*lines)[41], int max) {
  int n = 0;
  size_t at = 0, len = strlen(s);
  if (cols > 40) cols = 40;
  while (n < max && at < len) {
    size_t take = len - at < (size_t)cols ? len - at : (size_t)cols;
    if (at + take < len) { size_t w = take; while (w && s[at + w] != ' ') w--; if (w) take = w; }
    memcpy(lines[n], s + at, take);
    lines[n][take] = 0;
    n++;
    at += take;
    while (s[at] == ' ') at++;
  }
  return n;
}
// A boxed message between rows `top` and `bottom`: a heading and wrapped text. Returns the box's bottom edge.
template <class G> int drawNotice(G& g, int W, int top, int bottom, const char* heading, const char* body, uint16_t accent) {
  int sc = W == 320 ? 1 : 2, cols = (W - 48) / (6 * sc);
  char lines[8][41];
  int n = wrapText(body, cols, lines, 8);
  int hh = 24, h = hh + 6 + n * 10 * sc + 8;
  int y = top + (bottom - top - h) / 2;
  if (y < top + 2) y = top + 2;
  g.fillRoundRect(12, y, W - 24, h, 6, C_DARK);
  g.drawRoundRect(12, y, W - 24, h, 6, accent);
  Tile all = {12, y, W - 24, h, 0, 0};
  int hw = (int)strlen(heading) * 12;
  clippedText(g, all, heading, (W - hw) / 2, y + 8, 2, accent);
  for (int i = 0; i < n; i++) clippedText(g, all, lines[i], 24, y + hh + 6 + i * 10 * sc, sc, C_WHITE);
  return y + h;
}
// The "Check card" button under the empty-library notice: the same 44 pixel height on both boards. A tap reads the card again.
inline Tile emptyButton(int W) { int bottom = footer(W) - 4; return {12, bottom - 44, W - 24, 44, 0, 0}; }
inline bool inEmptyButton(int W, int x, int y) { Tile b = emptyButton(W); return x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h; }
// The page area when no game is listed (a player with no Starter courses and no card games): what to do, in words, and a button
// that reads the card again (the board cannot sense a card going in). The notice's bottom edge; 0 when there is nothing to say.
template <class G> int drawEmptyNotice(G& g, int W, CardState s, int count) {
  int top = header(W)+12, bottom = emptyButton(W).y - 4;
  int end = 0;
  switch (s) {
    case CARD_NONE: case CARD_READY:
      end = drawNotice(g, W, top, bottom, "No games yet", "Copy .GMP games into /pocket on a FAT32 microSD card. Insert it, then tap Check card.", C_WHITE); break;
    case CARD_UNREADABLE: end = drawNotice(g, W, top, bottom, "Can't read the card", "Take it out and put it back in. The card should be FAT32. Then tap Check card.", C_YELLOW); break;
    case CARD_NOFOLDER: end = drawNotice(g, W, top, bottom, "No POCKET folder", "The card has no folder named POCKET. Make one on a computer and copy .GMP games into it, then tap Check card.", C_YELLOW); break;
    case CARD_READING: { char b[48]; snprintf(b, sizeof b, "Looking for games. Found so far: %d.", count); return drawNotice(g, W, top, bottom + 48, "Reading the card", b, C_WHITE); }
    default: return 0;
  }
  Tile b = emptyButton(W);
  drawButton(g, b.x, b.y, b.x + b.w, b.y + b.h, "Check card");
  return end;
}
// Loading: the game's name, a bar. drawLoadingBar alone repaints the bar for the next step.
template <class G> void drawLoadingBar(G& g, int W, int H, int step, int of) {
  int x0 = 40, x1 = W - 40, y = H / 2 + 12;
  g.fillRect(x0, y, x1 - x0, 16, C_BLACK);
  g.drawRect(x0, y, x1 - x0, 16, C_GRAY);
  int fill = (x1 - x0 - 4) * (step < 0 ? 0 : step > of ? of : step) / (of ? of : 1);
  g.fillRect(x0 + 2, y + 2, fill, 12, C_YELLOW);
}
// The title over at most two lines of `cols` characters: break at the last space that fits, the second line ellipsized.
inline void wrapTitle(const char* name, int cols, char* l1, char* l2, size_t cap) {
  l1[0] = l2[0] = 0;
  if (!cap || cols < 4) return;
  if ((int)strlen(name) <= cols) { snprintf(l1, cap, "%s", name); return; }
  int cut = cols;
  for (int i = cols; i > cols / 2; i--) if (name[i] == ' ') { cut = i; break; }
  snprintf(l1, cap < (size_t)cut + 1 ? cap : (size_t)cut + 1, "%s", name);
  const char* rest = name + cut + (name[cut] == ' ' ? 1 : 0);
  ellipsize(rest, l2, cap, cols);
}
// The same Details composition is used by firmware and headless screenshots.
template <class G> void detailsScreen(G& g,int W,int H,const char* name,const char* source,const char* advisory,int mask,bool pinned) {
  g.fillRect(0,0,W,H,C_BLACK);
  g.fillRect(0,0,W,header(W),C_DARK);
  Tile all={12,0,W-24,H,0,0};
  clippedText(g,all,"GAME DETAILS",12,header(W)/2-8,2,C_GRAY);
  cx::draw(g,W);
  char a[64],b[64];wrapTitle(name,(W-24)/12,a,b,sizeof a);
  clippedText(g,all,a,12,header(W)+10,2,C_WHITE);
  if(b[0])clippedText(g,all,b,12,header(W)+30,2,C_WHITE);
  clippedText(g,all,source,12,header(W)+60,1,C_YELLOW);
  if(advisory)detailsTouch(g,W,detailsPagesTop(H)-20,advisory);
  detailsPages(g,W,H,mask);
  if(!mask)clippedText(g,all,"No extra pages in this game",12,detailsPagesTop(H)+14,1,C_GRAY);
  detailsFooter(g,W,H,pinned);
  if(pinned)detailsNote(g,W,H,"In Favorites");
}
template <class G> void emptyResults(G& g,int W,bool search) {
  drawNotice(g,W,header(W)+12,footer(W),search?"No matching titles":"No games here",search?"Try a shorter title, search All games, or tap Clear.":"Try another collection. For card games, insert the card and use Settings > Card > Rescan card.",C_WHITE);
}
template <class G> void drawLoading(G& g, int W, int H, const char* name, int step, int of) {
  g.fillRect(0, 0, W, H, C_BLACK);
  char t[64], t2[64];
  wrapTitle(name, (W - 32) / 12, t, t2, sizeof t);
  Tile all = {0, 0, W, H, 0, 0};
  clippedText(g, all, t, (W - (int)strlen(t) * 12) / 2, t2[0] ? H / 2 - 50 : H / 2 - 32, 2, C_WHITE);
  if (t2[0]) clippedText(g, all, t2, (W - (int)strlen(t2) * 12) / 2, H / 2 - 32, 2, C_WHITE);
  clippedText(g, all, "Loading", (W - 7 * 12) / 2, H / 2 - 10, 2, C_GRAY);
  drawLoadingBar(g, W, H, step, of);
}
inline Tile failButton(int W, int H) { return {W / 2 - 80, H - 54, 160, 44, 0, 0}; }
inline const char* failHeading(LoadFail f) { return f == FAIL_MEMORY ? "Not enough memory" : f == FAIL_CARD ? "Card removed" : "A file is missing"; }
inline const char* failBody(LoadFail f) {
  return f == FAIL_MEMORY ? "This game needs more memory than is free right now. Go back and try it again first. If it keeps failing, it is too big for this board."
       : f == FAIL_CARD ? "Put the card back in the slot, then try the game again."
                        : "The game's files are damaged or incomplete. Copy the game to the card again.";
}
// Shown instead of a reset when a game or one of its levels cannot load. `name` may be empty. The button returns to the list.
template <class G> void drawFailCard(G& g, int W, int H, const char* name, LoadFail f) {
  g.fillRect(0, 0, W, H, C_BLACK);
  Tile all = {0, 0, W, H, 0, 0};
  const char* top = "Could not load";
  clippedText(g, all, top, (W - (int)strlen(top) * 12) / 2, 8, 2, C_WHITE);
  if (name && name[0]) { char t[48]; ellipsize(name, t, sizeof t, (W - 16) / 6); clippedText(g, all, t, (W - (int)strlen(t) * 6) / 2, 30, 1, C_GRAY); }
  drawNotice(g, W, 44, H - 58, failHeading(f), failBody(f), C_YELLOW);
  Tile b = failButton(W, H);
  drawButton(g, b.x, b.y, b.x + b.w, b.y + b.h, "Back to games");
}
inline bool inFailButton(int W, int H, int x, int y) { // No neighboring action: add four pixels above and below the drawn button.
  Tile b = failButton(W, H); return x >= b.x && x < b.x + b.w && y >= b.y - 4 && y < b.y + b.h + 4; }
} // namespace pk
