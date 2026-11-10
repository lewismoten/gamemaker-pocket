// PLAYGAME's game menu (DoGame, GameMenu, GVertMenu), with no board code so the Mac test and the host renders share it.
// It is drawn on the game's menu picture (scene 0 slot 2) in 320 x 200 game pixels: the title at (96, 104), six words at x = 80 from
// y = 120 down in steps of 8, and GVertMenu's marker (a 6 x 4 box at x = 70) beside the chosen one. On the board each row is also a
// touch target, spread to meet its neighbours (the DOS rows are 8 pixels, 8 to 12 on the panel: too thin for a finger alone), and a
// strip at the foot has Up, Down, Enter and Esc (the keys GVertMenu reads).
#pragma once
#include <stdint.h>
#include <string.h>
#include "screen.h"

namespace gmmenu {

enum { M_PLAY = 1, M_INSTRUCTIONS, M_STORY, M_CREDITS, M_SCORES, M_QUIT, M_COUNT = M_QUIT };  // GVertMenu's choice numbers
inline const char* word(int c) {
  static const char* const w[] = {"", "Play", "Read Instructions", "Read Storyline", "See Credits", "See Highest Scores", "Quit"};
  return c >= 1 && c <= M_COUNT ? w[c] : "";
}
constexpr int TITLE_X = 96, TITLE_Y = 104, WORD_X = 80, WORD_Y = 15 * 8, MARK_X = 70, MARK_Y = 15 * 8 + 1, STEP = 8, WIDTH = 30 * 8;
// The marker's rectangle for choice c (BoxFill(x, y - 8 + c * 8, x + 5, y + 4 - 8 + c * 8)): game pixels, inclusive.
inline void marker(int c, int& x0, int& y0, int& x1, int& y1) { x0 = MARK_X; x1 = MARK_X + 5; y0 = MARK_Y - 8 + c * STEP; y1 = y0 + 4; }
// The text row of choice c in game pixels (8 high).
inline int rowY(int c) { return WORD_Y + (c - 1) * STEP; }

// The key strip at the foot: Up, Down, Enter and Esc as four equal buttons.
enum { K_NONE = -1, K_UP, K_DOWN, K_ENTER, K_ESC, K_COUNT };
inline const char* keyLabel(int k) { static const char* const l[] = {"Up", "Down", "Enter", "Esc"}; return k >= 0 && k < K_COUNT ? l[k] : ""; }
constexpr int STRIP_BTN_H = scr::SMALL ? 32 : 28;
inline int stripTop() { return scr::H - STRIP_BTN_H - 2; }
inline void keyRect(int k, int& x0, int& y0, int& x1, int& y1) { x0 = k * scr::W / K_COUNT + 2; x1 = (k + 1) * scr::W / K_COUNT - 2; y0 = stripTop(); y1 = scr::H - 2; }
inline int keyHit(int x, int y) {
  for (int k = 0; k < K_COUNT; k++) { int x0, y0, x1, y1; keyRect(k, x0, y0, x1, y1); if (x >= x0 && x < x1 && y >= y0 && y < y1) return k; }
  return K_NONE;
}
// The choice under a touch on the picture, or 0. A row's rectangle runs from the middle of the gap above it to the middle of the gap
// below, across the menu (x 64 to 316 in game pixels), so the six rows tile with no dead space and the first and last reach 4 pixels
// past their text. Taps on the strip are not rows.
inline int rowHit(int sx, int sy) {
  if (sy >= stripTop()) return 0;
  int gx = scr::down(sx), gy = scr::down(sy);
  if (gx < 64 || gx >= 316) return 0;
  int top = rowY(1) - 4, bottom = rowY(M_COUNT) + 8 + 4;
  if (gy < top || gy >= bottom) return 0;
  int c = gy < rowY(1) ? 1 : 1 + (gy - rowY(1)) / STEP;
  return c > M_COUNT ? M_COUNT : c;
}
// The choice after a key (GVertMenu: Up and Down wrap).
inline int step(int choice, int dir) { choice += dir; return choice < 1 ? M_COUNT : choice > M_COUNT ? 1 : choice; }

}  // namespace gmmenu
