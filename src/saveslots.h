// Save slots, the way PLAYGAME's F5 / F6 pick them (SelectGameToSave, SelectGameToLoad): ten slots, each with a name of up to 20
// letters typed when it is saved, kept in a 210-byte names table (10 records of 21 bytes, the layout of SavNames.dat). The DOS slot
// files were game0.sav to game9.sav; here a game's slot n is "/<game hash>.s<n>" and the table "/<game hash>.nam". Pure logic (no
// board code): the file system comes in as a small Store, so the Mac test runs it over a map. The slot's contents are gmsave.h's.
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace gm {

constexpr int SLOTS = 10, SLOT_NAME = 21, SLOT_NAME_MAX = SLOT_NAME - 1, SLOT_TABLE = SLOTS * SLOT_NAME;  // 210 bytes, as DOS

struct SlotStore {  // whole-file access: the board's LittleFS, the test's map
  void* user;
  bool (*exists)(void* user, const char* path);
  bool (*readAll)(void* user, const char* path, void* buf, size_t n);        // true only when exactly n bytes were read
  bool (*writeAll)(void* user, const char* path, const void* buf, size_t n);  // replaces the file; a failed write leaves the old one
};

struct SlotList {  // 220 bytes, a stack local
  char name[SLOTS][SLOT_NAME];
  bool used[SLOTS];
};

inline void slotPath(char* out, size_t cap, uint32_t game, int slot) { snprintf(out, cap, "/%08x.s%d", (unsigned)game, slot); }
inline void slotNamesPath(char* out, size_t cap, uint32_t game) { snprintf(out, cap, "/%08x.nam", (unsigned)game); }

// A name as it is kept: printable ASCII only, at most 20 characters, no leading or trailing spaces.
inline void slotCleanName(char* dst, const char* src) {
  int n = 0;
  while (*src == ' ') src++;
  for (; *src && n < SLOT_NAME_MAX; src++) if ((uint8_t)*src >= 32 && (uint8_t)*src < 127) dst[n++] = *src;
  while (n && dst[n - 1] == ' ') n--;
  dst[n] = 0;
}

// Reads which slots hold a game and their names. A slot with a file but no name (or an unreadable table) is "Saved game".
inline void slotsList(const SlotStore& s, uint32_t game, SlotList& l) {
  char path[24];
  char table[SLOT_TABLE];
  slotNamesPath(path, sizeof path, game);
  bool haveTable = s.exists(s.user, path) && s.readAll(s.user, path, table, sizeof table);
  for (int i = 0; i < SLOTS; i++) {
    slotPath(path, sizeof path, game, i);
    l.used[i] = s.exists(s.user, path);
    l.name[i][0] = 0;
    if (!l.used[i]) continue;
    if (haveTable) { table[i * SLOT_NAME + SLOT_NAME_MAX] = 0; slotCleanName(l.name[i], table + i * SLOT_NAME); }
    if (!l.name[i][0]) strcpy(l.name[i], "Saved game");
  }
}
inline bool slotsAny(const SlotList& l) { for (int i = 0; i < SLOTS; i++) if (l.used[i]) return true; return false; }
inline const char* slotLabel(const SlotList& l, int i) { return l.used[i] ? l.name[i] : "empty"; }

// Records `name` for a slot in the names table (the other nine names stay). False when the table cannot be written.
inline bool slotSetName(const SlotStore& s, uint32_t game, int slot, const char* name) {
  if (slot < 0 || slot >= SLOTS) return false;
  char path[24], table[SLOT_TABLE];
  slotNamesPath(path, sizeof path, game);
  if (!(s.exists(s.user, path) && s.readAll(s.user, path, table, sizeof table))) memset(table, 0, sizeof table);
  memset(table + slot * SLOT_NAME, 0, SLOT_NAME);
  slotCleanName(table + slot * SLOT_NAME, name);
  return s.writeAll(s.user, path, table, sizeof table);
}

// The slot list's two-tap overwrite: saving into a slot that holds a game asks once ("Tap again to overwrite"); a tap elsewhere or
// 3 seconds disarms it. An empty slot goes straight on. Loading never asks (the list is the choice) and an empty slot is inert.
enum { SLOT_NONE, SLOT_ARMED, SLOT_GO };
struct SlotArm {
  int armed = -1;
  uint32_t at = 0;
  static constexpr uint32_t MS = 3000;
  int tap(const SlotList& l, int slot, bool saving, uint32_t now) {
    if (slot < 0 || slot >= SLOTS) { armed = -1; return SLOT_NONE; }
    if (!saving) { armed = -1; return l.used[slot] ? SLOT_GO : SLOT_NONE; }
    if (!l.used[slot]) { armed = -1; return SLOT_GO; }
    if (armed == slot && now - at <= MS) { armed = -1; return SLOT_GO; }
    armed = slot; at = now;
    return SLOT_ARMED;
  }
  bool expire(uint32_t now) { if (armed < 0 || now - at <= MS) return false; armed = -1; return true; }
};

// The list's layout: a header band (the title and the Close X) over two columns of five rows, slots 1 to 5 at the left and 6 to 10 at
// the right (PLAYGAME listed ten down one column; that does not fit a 240 px screen at 36 px a row). Rows are at least 36 px tall.
struct SlotGeom { int top, rowH, colW, gap, x0; };
inline SlotGeom slotGeom(int W, int H) {
  SlotGeom g;
  g.top = W <= 320 ? 40 : 52;
  g.gap = W <= 320 ? 2 : 4;
  g.x0 = 4;
  g.colW = (W - 2 * g.x0 - g.gap) / 2;
  g.rowH = (H - g.top - 2) / 5 - g.gap;
  return g;
}
inline void slotRect(int W, int H, int slot, int& x0, int& y0, int& x1, int& y1) {
  SlotGeom g = slotGeom(W, H);
  x0 = g.x0 + slot / 5 * (g.colW + g.gap);
  y0 = g.top + slot % 5 * (g.rowH + g.gap);
  x1 = x0 + g.colW;
  y1 = y0 + g.rowH;
}
inline int slotAt(int W, int H, int x, int y) {
  for (int i = 0; i < SLOTS; i++) {
    int x0, y0, x1, y1;
    slotRect(W, H, i, x0, y0, x1, y1);
    if (x >= x0 && x < x1 && y >= y0 && y < y1) return i;
  }
  return -1;
}

}  // namespace gm
