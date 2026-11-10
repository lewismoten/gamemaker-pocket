// Host regression for the play core's bounds fixes (src/gmcore.h): a hero position far outside the map wraps
// as DOS does, a map cell above 150 is an empty block, and a block effect of 30 or more has no counter.
// Build: c++ -std=c++17 -O1 -g -fsanitize=address,undefined -Isrc host/coretest.cpp -o coretest
#include <stdio.h>
#include <stdlib.h>
#include <memory>
#include "gmcore.h"
#include "gmcfg.h"

using namespace gm;
static int fails = 0;
static void check(bool ok, const char* what) { if (!ok) { fails++; printf("FAIL %s\n", what); } }

int main() {
  auto w = std::make_unique<World>();
  memset(w.get(), 0, sizeof(World));
  for (int i = 0; i <= BACKBL; i++) w->blkmap[i] = (uint8_t)i;
  w->map[5 * MLEN + 5] = 255;       // a cell above 150
  w->map[7 * MLEN + 3] = 42;
  check(w->mapBlock(5, 5) == BACKBL, "cell 255 is BACKBL");
  check(w->mapBlock(3 + MLEN * 3, 7 + MWID) == 42, "positive wrap");
  check(w->mapBlock(3 - MLEN, 7 - MWID * 2) == 42, "negative wrap");
  // DICE.GAM bytes 351..354 = FF 7F FF 7F: tx = ty = 32767
  int a = w->mapBlock(32767, 32767), b = w->mapBlock(-32768, -32768);
  check(a >= 0 && a <= BACKBL && b >= 0 && b <= BACKBL, "link 32767 / -32768 stays on the map");
  w->cx[0] = 32767; w->cy[0] = 32767; w->cx[1] = 0; w->cy[1] = 0;
  (void)w->getTouch();
  // effect 199 must not read meter[199]
  Attr at; memset(&at, 0, sizeof at); at.effect = 199; at.eamt = 1;
  (void)w->doBlockTouchChar(at);
  // A level line's counts come from a file anyone can make: they are clamped to what the engine can index (no wrap, no negative).
  LevelFiles L;
  memset(&L, 0, sizeof L);
  check(parseLevelLine("level a.map a.bbl a.cbl a.chr a.pal 1 1 0 0 0 0 10226186 a.mon a.mbl 9999 -5", &L, nullptr), "parse hostile level line");
  check(L.nbl == BACKBL && L.nmon == 0 && L.nmbl == 0, "nbl clamped, negative nmbl is none");
  check(parseLevelLine("level a.map a.bbl a.cbl a.chr a.pal 1 1 0 0 0 0 -7 a.mon a.mbl 9999 9999", &L, nullptr) && L.nbl == 0 && L.nmon == LASTMON && L.nmbl == MONBL, "negative nbl is 0, big nmon/nmbl clamped");
  // swap: a 30.8 KB .bbl must not pass for a level that claims 150 blocks (63 KB), even where 32-bit size_t would have wrapped.
  {
    struct Fake { static uint8_t* rd(const char*, size_t* n) { *n = 5; uint8_t* f = (uint8_t*)malloc(5); f[0] = 0x68; f[1] = 0x78; f[2] = f[3] = f[4] = 0; return f; }  // len 30824
                  static bool inf(const uint8_t*, size_t, uint8_t* out, size_t len) { memset(out, 0, len); return true; } };
    Assets as;
    as.read = Fake::rd; as.inflate = Fake::inf;
    uint8_t* slot = nullptr; char have[16] = "";
    check(!Assets::swap(slot, have, "a.bbl", (uint64_t)150 * BLOCK_BYTES, as) && !slot, "short .bbl refused for 150 blocks");
    check(Assets::swap(slot, have, "a.bbl", (uint64_t)73 * BLOCK_BYTES, as) && slot, "30.8 KB .bbl fits 73 blocks");
    free(slot);
  }
  printf(fails ? "coretest FAILED\n" : "coretest ok\n");
  return fails != 0;
}
