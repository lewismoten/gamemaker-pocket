// Review fixes of 2026-11-10: a CHR whose sequence key scan code is 128 or more is refused at load and cannot index keydn;
// settings.txt is kept after a failed or partial read and may be created when it is missing.
//   c++ -std=c++17 -O1 -Isrc host/savesfixtest.cpp -lz -o savesfixtest && ./savesfixtest
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory>
#include "gmcore.h"
#include "gmcfg.h"
#include "settingsguard.h"
using namespace gm;
static int fails = 0;
static void check(bool ok, const char* what) { if (!ok) { fails++; printf("FAIL %s\n", what); } }
int main() {
  auto w = std::make_unique<World>();
  memset(w.get(), 0, sizeof(World));
  static uint8_t chr[3784];
  w->chr = chr;
  chr[DROPSEQ * SEQ_BYTES + 171] = 128;  // Drop's key: scan code 128
  w->keydn[127] = 1;
  uint8_t after[128];
  memcpy(after, w->keydn, 128);
  check(!w->seqKeyDown(DROPSEQ), "key 128 is never down");
  chr[DROPSEQ * SEQ_BYTES + 171] = 255;
  check(!w->seqKeyDown(DROPSEQ), "key 255 is never down");
  chr[DROPSEQ * SEQ_BYTES + 171] = 127;
  check(w->seqKeyDown(DROPSEQ), "key 127 reads keydn[127]");
  check(memcmp(after, w->keydn, 128) == 0, "keydn unchanged");
  check(pk::keepSettingsFile(true, false, 0, 0), "existing file that will not open is kept");
  check(pk::keepSettingsFile(true, true, 10, 40), "partly read file is kept");
  check(!pk::keepSettingsFile(true, true, 40, 40), "fully read file may be saved");
  check(!pk::keepSettingsFile(false, false, 0, 0), "missing file may be created");
  printf(fails ? "savesfixtest FAILED\n" : "savesfixtest ok\n");
  return fails != 0;
}
