// F5 / F6 for the Pocket Player: what PLAYGAME's SaveGame kept (the level's map, the monsters, the blocks' timers, the hero and
// the clock) written to one file per game, and put back over the level LoadGame loads fresh (gmcore.h World::restore, then
// applyState). The file is this firmware's own (a run-length packed copy of the play core's state), not a DOS .SAV: the board's
// World is not PLAYGAME's memory. No board code in it: the Mac test round-trips it (host/gamesavetest.cpp).
// Layout: "GMS1", the level's EntryState (so the level loads as it was entered), the state, then a CRC-32 of everything before it.
#pragma once
#include <stdint.h>
#include <string.h>
#include "gmcore.h"

namespace gm {

constexpr uint32_t SAVE_MAGIC = 0x31534d47;  // "GMS1"
// Byte sinks and sources: the board's file, the test's buffer. Return false on a failed write or a short read.
typedef bool (*SaveWrite)(void* user, const void* p, size_t n);
typedef bool (*SaveRead)(void* user, void* p, size_t n);

inline uint32_t saveCrc(uint32_t c, const void* data, size_t n) {  // running CRC-32: start at 0, feed the pieces in order
  c = ~c;
  const uint8_t* p = (const uint8_t*)data;
  while (n--) { c ^= *p++; for (int i = 0; i < 8; i++) c = (c >> 1) ^ ((c & 1) ? 0xedb88320u : 0); }
  return ~c;
}

namespace savedetail {
struct Out { SaveWrite w; void* user; uint32_t crc = 0; bool ok = true;
  void put(const void* p, size_t n) { if (!ok) return; crc = saveCrc(crc, p, n); ok = w(user, p, n); } };
struct In { SaveRead r; void* user; uint32_t crc = 0; bool ok = true;
  void get(void* p, size_t n) { if (!ok) return; ok = r(user, p, n); if (ok) crc = saveCrc(crc, p, n); } };
// Runs of one byte: (count 1..255, value) pairs.
inline void rlePut(Out& o, const uint8_t* a, int n) {
  int i = 0;
  while (i < n) { int j = i + 1; while (j < n && j - i < 255 && a[j] == a[i]) j++; uint8_t pr[2] = {(uint8_t)(j - i), a[i]}; o.put(pr, 2); i = j; }
}
inline void rleGet(In& in, uint8_t* a, int n) {
  int i = 0;
  while (i < n && in.ok) { uint8_t pr[2]; in.get(pr, 2); if (!in.ok) return; if (!pr[0] || i + pr[0] > n) { in.ok = false; return; } memset(a + i, pr[1], pr[0]); i += pr[0]; }
}
}  // namespace savedetail

// Writes the world's entry and state. The length of an RLE run list is not known ahead, so each packed array is preceded by its size.
inline bool saveWorld(const World& w, SaveWrite write, void* user) {
  using namespace savedetail;
  Out o{write, user};
  uint32_t m = SAVE_MAGIC;
  o.put(&m, 4);
  o.put(&w.entry, sizeof w.entry);
  int32_t i32[] = {w.level, w.noMons, w.firstEmptyMM, w.monsolid, w.mmSolid, w.nblkMon, w.mx, w.my, w.doScroll, w.prevToX, w.prevToY,
                   w.cx[0], w.cx[1], w.cy[0], w.cy[1], w.cframe, w.cseq, w.hitpts, w.lives, w.score, w.topthere, w.bottomthere,
                   w.curtop, w.curbottom, w.nextx, w.nexty, w.scrx, w.scry, w.oldx, w.oldy, w.netx, w.nety, w.gravx, w.gravy};
  o.put(i32, sizeof i32);
  uint32_t u32[] = {w.seed, w.rngA, w.clock, w.gclk, w.nextAni, w.ticks};
  o.put(u32, sizeof u32);
  o.put(&w.sc, sizeof w.sc);
  o.put(w.seqon, sizeof w.seqon);
  o.put(w.inv, sizeof w.inv);
  o.put(w.meter, sizeof w.meter);
  o.put(w.blkmap, sizeof w.blkmap);
  o.put(w.nxtchg, sizeof w.nxtchg);
  o.put(w.birthFlag, sizeof w.birthFlag);
  o.put(w.blkMonBl, sizeof w.blkMonBl);
  o.put(w.blkMonNext, sizeof w.blkMonNext);
  rlePut(o, (const uint8_t*)w.mm, sizeof w.mm);
  rlePut(o, (const uint8_t*)w.mi, sizeof w.mi);
  rlePut(o, w.map, MLEN * MWID);
  rlePut(o, w.mapMon, MLEN * MWID);
  uint32_t crc = o.crc;
  o.ok = o.ok && write(user, &crc, 4);
  return o.ok;
}

// Checks the whole file (read it from the start): magic and CRC. True when it is a save this build can read.
inline bool saveCheck(SaveRead read, void* user, size_t size) {
  uint8_t buf[256];
  if (size < 12) return false;
  uint32_t crc = 0;
  size_t left = size - 4;
  while (left) { size_t n = left < sizeof buf ? left : sizeof buf; if (!read(user, buf, n)) return false; crc = saveCrc(crc, buf, n); left -= n; }
  uint32_t want;
  return read(user, &want, 4) && want == crc;
}

// The first part of a save (read from its start): the level to enter, as World::restore takes it.
inline bool saveEntry(savedetail::In& in, EntryState& e) {
  uint32_t m = 0;
  in.get(&m, 4);
  in.get(&e, sizeof e);
  return in.ok && m == SAVE_MAGIC && e.magic == ENTRY_MAGIC;
}
inline bool saveReadEntry(SaveRead read, void* user, EntryState& e) { savedetail::In in{read, user}; return saveEntry(in, e); }

// Puts the saved state over a world that has just entered the saved level (World::restore(entry)). The source is read from its start.
// False when the file is short or its packed arrays do not add up; the world is then not to be played.
inline bool loadWorld(World& w, SaveRead read, void* user) {
  using namespace savedetail;
  In in{read, user};
  EntryState e;
  if (!saveEntry(in, e)) return false;
  int32_t i32[34];
  in.get(i32, sizeof i32);
  uint32_t u32[6];
  in.get(u32, sizeof u32);
  Scroll sc;
  in.get(&sc, sizeof sc);
  in.get(w.seqon, sizeof w.seqon);
  in.get(w.inv, sizeof w.inv);
  in.get(w.meter, sizeof w.meter);
  in.get(w.blkmap, sizeof w.blkmap);
  in.get(w.nxtchg, sizeof w.nxtchg);
  in.get(w.birthFlag, sizeof w.birthFlag);
  in.get(w.blkMonBl, sizeof w.blkMonBl);
  in.get(w.blkMonNext, sizeof w.blkMonNext);
  rleGet(in, (uint8_t*)w.mm, sizeof w.mm);
  rleGet(in, (uint8_t*)w.mi, sizeof w.mi);
  rleGet(in, w.map, MLEN * MWID);
  rleGet(in, w.mapMon, MLEN * MWID);
  if (!in.ok) return false;
  int k = 0;
  w.level = i32[k++]; w.noMons = i32[k++] != 0; w.firstEmptyMM = i32[k++]; w.monsolid = i32[k++]; w.mmSolid = i32[k++]; w.nblkMon = i32[k++];
  w.mx = i32[k++]; w.my = i32[k++]; w.doScroll = i32[k++] != 0; w.prevToX = i32[k++]; w.prevToY = i32[k++];
  w.cx[0] = i32[k++]; w.cx[1] = i32[k++]; w.cy[0] = i32[k++]; w.cy[1] = i32[k++]; w.cframe = i32[k++]; w.cseq = i32[k++];
  w.hitpts = i32[k++]; w.lives = i32[k++]; w.score = i32[k++]; w.topthere = i32[k++] != 0; w.bottomthere = i32[k++] != 0;
  w.curtop = i32[k++]; w.curbottom = i32[k++]; w.nextx = i32[k++]; w.nexty = i32[k++]; w.scrx = i32[k++]; w.scry = i32[k++];
  w.oldx = i32[k++]; w.oldy = i32[k++]; w.netx = i32[k++]; w.nety = i32[k++]; w.gravx = i32[k++]; w.gravy = i32[k++];
  w.seed = u32[0]; w.rngA = u32[1]; w.clock = u32[2]; w.gclk = u32[3]; w.nextAni = u32[4]; w.ticks = u32[5];
  w.sc = sc;
  if (w.nblkMon < 0 || w.nblkMon > BACKBL || w.cseq < 0 || w.cseq >= MAXSEQ) return false;
  for (int i = 0; i < LASTMM; i++) if (w.mm[i].monnum > LASTMON) return false;
  w.gen++;
  w.update = true;
  w.resetKeys();
  memset(w.held, 0, sizeof w.held);
  w.npending = 0;
  w.nsnd = 0;
  w.won = false;
  return true;
}

}  // namespace gm
