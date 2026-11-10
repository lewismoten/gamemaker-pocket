// PLAYGAME's Top Ten (HISCOREL.C): ten records of a 32 bit score and a 30 byte name, in <game>.his, the same bytes DOS wrote.
// A score goes in above the first one it beats (CheckHighScores), the ones below move down, the name is typed (22 letters at most).
// No board code in it: the Mac test builds the table, inserts and reads it back (host/gamesavetest.cpp).
#pragma once
#include <stdint.h>
#include <string.h>
#include <stdio.h>

namespace gm {

constexpr int HIS_COUNT = 10, HIS_NAME = 30, HIS_NAME_MAX = 22, HIS_REC = 4 + HIS_NAME, HIS_BYTES = HIS_COUNT * HIS_REC;
struct HiScore { int32_t score; char name[HIS_NAME]; };
struct HiTable { HiScore top[HIS_COUNT]; };

inline void hiParse(const uint8_t* b, size_t n, HiTable& t) {  // a short or missing file is an empty table
  memset(&t, 0, sizeof t);
  if (!b || n < (size_t)HIS_BYTES) return;
  for (int i = 0; i < HIS_COUNT; i++) {
    const uint8_t* r = b + i * HIS_REC;
    t.top[i].score = (int32_t)(r[0] | r[1] << 8 | r[2] << 16 | (uint32_t)r[3] << 24);
    memcpy(t.top[i].name, r + 4, HIS_NAME);
    t.top[i].name[HIS_NAME - 1] = 0;
  }
}
inline void hiSerialize(const HiTable& t, uint8_t* b) {
  for (int i = 0; i < HIS_COUNT; i++) {
    uint8_t* r = b + i * HIS_REC;
    uint32_t s = (uint32_t)t.top[i].score;
    r[0] = (uint8_t)s; r[1] = (uint8_t)(s >> 8); r[2] = (uint8_t)(s >> 16); r[3] = (uint8_t)(s >> 24);
    memcpy(r + 4, t.top[i].name, HIS_NAME);
  }
}
// The place (1..10) a score would take, or 0 (CheckHighScores: it must beat the record there; a score of 0 or less never does,
// since an empty record is 0).
inline int hiPlace(const HiTable& t, long score) {
  for (int j = 0; j < HIS_COUNT; j++) if (score > t.top[j].score) return j + 1;
  return 0;
}
// Puts the score in its place with an empty name (the typed name goes to hiSetName). Returns the place, or 0.
inline int hiInsert(HiTable& t, long score) {
  int p = hiPlace(t, score);
  if (!p) return 0;
  for (int k = HIS_COUNT - 1; k > p - 1; k--) t.top[k] = t.top[k - 1];
  t.top[p - 1].score = (int32_t)score;
  t.top[p - 1].name[0] = 0;
  return p;
}
inline void hiSetName(HiTable& t, int place, const char* name) {
  if (place < 1 || place > HIS_COUNT) return;
  snprintf(t.top[place - 1].name, HIS_NAME_MAX + 1, "%s", name);
}
// ShowHighScores' row heights: 22 pixels, the step shrinking by 1 after each row and 1 more after each even one (y starts at 27).
inline int hiRowY(int j) { int y = 27, sp = 22; for (int i = 0; i < j; i++) { y += sp; if ((i % 2) == 0) sp--; sp--; } return y; }

}  // namespace gm
