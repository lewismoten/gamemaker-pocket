// Host test for F5/F6 saves (src/gmsave.h), the Top Ten (src/gmhiscore.h) and the DOS menu's touch map (src/gmmenu.h).
// Build: c++ -std=c++17 -O1 -g -fsanitize=address,undefined -Isrc host/gamesavetest.cpp -o gamesavetest   (add -DPOCKET_CYD28 for 320 x 240)
#include <stdio.h>
#include <memory>
#include <vector>
#include "gmcore.h"
#include "gmhiscore.h"
#include "gmmenu.h"
#include "gmsave.h"

using namespace gm;
static int fails = 0;
static void check(bool ok, const char* what) { if (!ok) { fails++; printf("FAIL %s\n", what); } }

struct Buf { std::vector<uint8_t> v; size_t at = 0; };
static bool wr(void* u, const void* p, size_t n) { auto* b = (Buf*)u; b->v.insert(b->v.end(), (const uint8_t*)p, (const uint8_t*)p + n); return true; }
static bool rd(void* u, void* p, size_t n) { auto* b = (Buf*)u; if (b->at + n > b->v.size()) return false; memcpy(p, b->v.data() + b->at, n); b->at += n; return true; }

static void fill(World& w) {
  memset(&w, 0, sizeof w);
  for (int i = 0; i < MLEN * MWID; i++) { w.map[i] = (uint8_t)(i % 7 == 0 ? i % 150 : 0); w.mapMon[i] = LASTMM; }
  w.mapMon[5 * MLEN + 6] = 3;
  for (int i = 0; i <= BACKBL; i++) { w.blkmap[i] = (uint8_t)i; w.nxtchg[i] = 0xffffffffu - i; }
  for (int i = 0; i < LASTMM; i++) { w.mm[i] = {(uint8_t)(i % 5 ? LASTMON : 2), (uint8_t)i, (uint8_t)(i / 2)}; w.mi[i].curx[0] = i; w.mi[i].nextMove = 77 + i; }
  for (int i = 0; i < MAXINV; i++) w.inv[i] = BACKBL;
  w.inv[2] = 9;
  for (int i = 0; i < 30; i++) w.meter[i] = i * 11 - 5;
  w.level = 3; w.cx[0] = 40; w.cx[1] = 7; w.cy[0] = 12; w.cy[1] = 2; w.cseq = 4; w.cframe = 2;
  w.hitpts = 61; w.lives = 2; w.score = 12345; w.clock = 99999; w.ticks = 500; w.seed = 7; w.rngA = 1234567;
  w.mx = 31; w.my = 8; w.nblkMon = 2; w.blkMonBl[1] = 17; w.blkMonNext[1] = 4242; w.sc.nx = 12; w.sc.upStop = 9;
  w.entry.magic = ENTRY_MAGIC; w.entry.level = 3; w.entry.sx = 30; w.entry.sy = 20; w.entry.lives = 3;
  w.noMons = false;
}

int main() {
  auto a = std::make_unique<World>(), b = std::make_unique<World>();
  fill(*a);
  Buf f;
  check(saveWorld(*a, wr, &f), "save writes");
  printf("save is %zu bytes\n", f.v.size());
  check(f.v.size() < 40000, "the save is smaller than the raw state");
  f.at = 0;
  check(saveCheck(rd, &f, f.v.size()), "the CRC and size check");
  Buf bad = f;
  bad.v[200] ^= 1; bad.at = 0;
  check(!saveCheck(rd, &bad, bad.v.size()), "a flipped byte fails the CRC");
  // a new world that has just entered the level (as World::restore does) gets the state over it
  memset(b.get(), 0, sizeof(World));
  f.at = 0;
  EntryState e;
  check(saveReadEntry(rd, &f, e) && e.level == 3 && e.sx == 30, "the entry reads back");
  f.at = 0;
  check(loadWorld(*b, rd, &f), "load applies");
  check(!memcmp(a->map, b->map, sizeof a->map), "map");
  check(!memcmp(a->mapMon, b->mapMon, sizeof a->mapMon), "monster map");
  check(!memcmp(a->mm, b->mm, sizeof a->mm) && !memcmp(a->mi, b->mi, sizeof a->mi), "monsters");
  check(!memcmp(a->nxtchg, b->nxtchg, sizeof a->nxtchg) && !memcmp(a->blkmap, b->blkmap, sizeof a->blkmap), "block timers");
  check(b->score == 12345 && b->lives == 2 && b->hitpts == 61 && b->level == 3, "score, lives, hit points, level");
  check(b->cx[0] == 40 && b->cx[1] == 7 && b->cy[0] == 12 && b->cseq == 4 && b->cframe == 2, "hero");
  check(b->clock == 99999 && b->seed == 7 && b->rngA == 1234567 && b->ticks == 500, "clock and seed");
  check(!memcmp(a->meter, b->meter, sizeof a->meter) && b->inv[2] == 9, "meters and inventory");
  check(b->nblkMon == 2 && b->blkMonBl[1] == 17 && b->blkMonNext[1] == 4242 && b->sc.nx == 12 && b->sc.upStop == 9, "block monsters and scroll");
  check(b->update && b->gen == 1, "the screen is flagged to redraw");
  // a short file does not apply
  Buf shortf = f;
  shortf.v.resize(shortf.v.size() / 2); shortf.at = 0;
  check(!loadWorld(*b, rd, &shortf), "a short file is refused");
  check(!saveCheck(rd, &shortf, shortf.v.size()), "a short file fails the check");

  // ---- the Top Ten ----
  HiTable t;
  hiParse(nullptr, 0, t);
  check(hiPlace(t, 0) == 0 && hiPlace(t, -5) == 0, "no score of 0 or less makes the list");
  check(hiPlace(t, 1) == 1, "any score beats an empty list");
  int p = hiInsert(t, 500); hiSetName(t, p, "ADA");
  p = hiInsert(t, 900); hiSetName(t, p, "BOB");
  check(p == 1, "a higher score takes first place");
  p = hiInsert(t, 700); hiSetName(t, p, "CY");
  check(p == 2 && !strcmp(t.top[0].name, "BOB") && !strcmp(t.top[1].name, "CY") && !strcmp(t.top[2].name, "ADA"), "the rest move down");
  p = hiInsert(t, 700);
  check(p == 3, "a tie goes below the record it matches (score > record)");
  for (int i = 0; i < 12; i++) hiInsert(t, 1000 + i);
  check(t.top[9].score >= 700 && t.top[0].score == 1011, "ten records, the lowest fall off");
  check(hiPlace(t, 10) == 0, "a score below the tenth does not get in");
  hiSetName(t, 1, "A NAME THAT IS MUCH TOO LONG TO FIT");
  check(strlen(t.top[0].name) == 22, "a name is cut at 22 letters");
  uint8_t raw[HIS_BYTES];
  hiSerialize(t, raw);
  HiTable u;
  hiParse(raw, sizeof raw, u);
  check(!memcmp(&t, &u, sizeof t) || [&] { for (int i = 0; i < 10; i++) if (t.top[i].score != u.top[i].score || strcmp(t.top[i].name, u.top[i].name)) return false; return true; }(), "the .his bytes round trip");
  check(HIS_BYTES == 340 && raw[0] == (uint8_t)t.top[0].score, "DOS's 10 x (long + 30 byte name) record");
  check(hiRowY(0) == 27 && hiRowY(1) == 49 && hiRowY(2) == 69, "ShowHighScores' row heights");

  // ---- the menu's touch map ----
  using namespace gmmenu;
  for (int c = 1; c <= M_COUNT; c++) {
    int sx = scr::up(WORD_X + 40), sy = scr::up(rowY(c) + 4);
    check(rowHit(sx, sy) == c, "the middle of a drawn row picks it");
  }
  check(rowHit(scr::up(WORD_X), scr::up(rowY(1) - 4)) == 1, "the first row reaches 4 px above its text");
  check(rowHit(scr::up(WORD_X), scr::up(rowY(1) - 6)) == 0, "above the menu is nothing");
  check(rowHit(scr::up(WORD_X), scr::up(rowY(6) + 11)) == 6, "the last row reaches below its text");
  check(rowHit(scr::up(10), scr::up(rowY(3))) == 0, "left of the menu is nothing");
  check(rowHit(scr::W / 2, stripTop() + 4) == 0, "the strip is not a row");
  int seen = 0;
  for (int y = 0; y < scr::H; y++) { int c = rowHit(scr::up(WORD_X + 20), y); if (c) seen |= 1 << c; }
  check(seen == 0x7e, "every row can be hit");
  for (int k = 0; k < K_COUNT; k++) {
    int x0, y0, x1, y1;
    keyRect(k, x0, y0, x1, y1);
    check(x1 - x0 >= 24 && y1 - y0 >= 24, "a strip key is at least 24 x 24");
    check(keyHit((x0 + x1) / 2, (y0 + y1) / 2) == k, "the strip key is found where drawn");
  }
  check(step(1, -1) == M_QUIT && step(M_QUIT, 1) == M_PLAY && step(2, 1) == 3, "Up and Down wrap");
  check(strcmp(word(M_SCORES), "See Highest Scores") == 0, "PLAYGAME's words");
  printf(fails ? "FAILED %d\n" : "ok\n", fails);
  return fails != 0;
}
