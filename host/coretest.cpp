// Host regression for the play core's bounds fixes (src/gmcore.h): a hero position far outside the map wraps
// as DOS does, a map cell above 150 is an empty block, and a block effect of 30 or more has no counter.
// Build: c++ -std=c++17 -O1 -g -fsanitize=address,undefined -Isrc host/coretest.cpp -o coretest
#include <stdio.h>
#include <stdlib.h>
#include <memory>
#include "gmcore.h"

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
  printf(fails ? "coretest FAILED\n" : "coretest ok\n");
  return fails != 0;
}
