// Self-contained play-core edge cases. No board, external games, sanitizer, or signal handler.
// c++ -std=c++17 -O1 -g -Wall -Wextra -Isrc host/coreedgetest.cpp -lz -o /tmp/coreedgetest
// /tmp/coreedgetest
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <zlib.h>
#include "gmcfg.h"

using namespace gm;
using Bytes = std::vector<uint8_t>;
static int failures = 0, checks = 0;
static void check(bool ok, const char* name) {
  ++checks;
  if (!ok) { ++failures; printf("FAIL %s\n", name); }
}
static void put16(Bytes& b, size_t p, int v) {
  b.at(p) = uint8_t(v); b.at(p + 1) = uint8_t(unsigned(v) >> 8);
}
static Bytes pack(const Bytes& raw) {
  z_stream z = {};
  check(deflateInit2(&z, 6, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) == Z_OK, "deflate init");
  Bytes out(compressBound(raw.size()) + 16);
  for (int i = 0; i < 4; ++i) out[i] = uint8_t(raw.size() >> (8 * i));
  z.next_in = const_cast<Bytef*>(raw.data()); z.avail_in = uInt(raw.size());
  z.next_out = out.data() + 4; z.avail_out = uInt(out.size() - 4);
  check(deflate(&z, Z_FINISH) == Z_STREAM_END, "deflate fixture");
  out.resize(z.total_out + 4); deflateEnd(&z);
  return out;
}
static bool inflateRaw(const uint8_t* src, size_t n, uint8_t* dst, size_t cap) {
  z_stream z = {};
  if (inflateInit2(&z, -15) != Z_OK) return false;
  z.next_in = const_cast<Bytef*>(src); z.avail_in = uInt(n);
  // readz allocates len + 1, including for a valid zero-length deflate stream.
  z.next_out = dst; z.avail_out = uInt(cap + 1);
  int r = inflate(&z, Z_FINISH);
  bool ok = r == Z_STREAM_END && z.total_out == cap;
  inflateEnd(&z); return ok;
}
static std::map<std::string, Bytes> disk;
static uint8_t* readFile(const char* name, size_t* n) {
  auto it = disk.find(name);
  if (it == disk.end()) return nullptr;
  *n = it->second.size();
  auto p = static_cast<uint8_t*>(malloc(*n + 1));
  if (!p) return nullptr;
  if (*n) memcpy(p, it->second.data(), *n);
  return p;
}
struct Text {
  std::string text;
  struct Cursor { const std::string* text; size_t pos; };
  static void* open(void* user, const char*) { return new Cursor{&static_cast<Text*>(user)->text, 0}; }
  static bool next(void* h, char* line, int cap) {
    auto& c = *static_cast<Cursor*>(h);
    if (c.pos == c.text->size()) return false;
    size_t end = c.text->find('\n', c.pos);
    if (end == std::string::npos) end = c.text->size();
    size_t n = end - c.pos;
    if (n >= size_t(cap)) n = size_t(cap - 1);
    memcpy(line, c.text->data() + c.pos, n); line[n] = 0;
    c.pos = end < c.text->size() ? end + 1 : end;
    return true;
  }
  static void close(void* h) { delete static_cast<Cursor*>(h); }
  Lines lines() { return {open, next, close, this}; }
};
static const char* levelLine = "level map bbl cbl chr pal 50 50 255 255 255 255 1 mon mbl 64 100\n";
struct Fixture {
  std::unique_ptr<World> w = std::make_unique<World>();
  Cfg cfg = {};
  Text text{levelLine};
  Lines lines = text.lines();
  Assets a;
  Fixture() {
    disk.clear();
    Bytes bbl(BLOCK_BYTES, 0), cbl(CHARBL * BLOCK_BYTES, 255), chr(3784, 0);
    bbl[400 + 11] = bbl[400 + 14] = 0;
    for (int s = 0; s < MAXSEQ; ++s) {
      for (int f = 0; f < MAXFRAME; ++f) {
        size_t p = s * SEQ_BYTES + f * FRAME_BYTES;
        put16(chr, p + 6, CHARBL); put16(chr, p + 8, CHARBL); put16(chr, p + 10, 1);
      }
      chr[s * SEQ_BYTES + 172] = 255; chr[s * SEQ_BYTES + 174] = BACKBL;
    }
    for (int i = 0; i < MAXINV; ++i) chr[3714 + i] = BACKBL;
    for (int i = 0; i < 30; ++i) put16(chr, 3724 + 2 * i, 100);
    Bytes map(MLEN * MWID * 2 + LASTMM * 3, 0);
    memset(map.data() + MLEN * MWID, LASTMM, MLEN * MWID);
    for (int i = 0; i < LASTMM; ++i) map[MLEN * MWID * 2 + i * 3] = LASTMON;
    disk["map"] = pack(map); disk["bbl"] = pack(bbl); disk["cbl"] = pack(cbl);
    disk["chr"] = pack(chr); disk["pal"] = pack(Bytes(768, 0));
    Bytes monsters(LASTMON * MONSTER_BYTES, 0);
    for (int i = 0; i < LASTMON; ++i) monsters[i * MONSTER_BYTES + 98] = LASTMON;
    disk["mon"] = pack(monsters); disk["mbl"] = pack(Bytes(MONBL * BLOCK_BYTES, 255));
    a.attach(*w, cfg, readFile, inflateRaw); a.lines = &lines;
    strcpy(a.cfgName, "fixture.cfg");
  }
  ~Fixture() { a.release(); }
  bool begin() { return parseCfg(text.text.c_str(), *w, cfg) && w->begin(); }
};

static void damagedAssets() {
  // Firmware consumes converted .GAM metadata as cfg, not a raw .GAM parser.
  // Every mandatory file is truncated at EVERY compressed byte, then retried intact.
  Fixture f;
  for (const char* name : {"map", "bbl", "cbl", "chr", "pal", "mon", "mbl"}) {
    Bytes good = disk[name];
    for (size_t cut = 0; cut < good.size(); ++cut) {
      disk[name] = Bytes(good.begin(), good.begin() + cut);
      check(!f.begin(), "every packed prefix rejected"); f.a.release();
    }
    disk[name] = good;
    check(f.begin(), "intact asset retry succeeds"); f.a.release();
  }
  struct Size { const char* name; int min; bool exact; };
  for (auto s : {Size{"map", 20600, true}, {"bbl", 420, false}, {"cbl", 21000, false},
                 {"chr", 3784, false}, {"pal", 768, false}, {"mon", 19200, false}, {"mbl", 42000, false}}) {
    Bytes good = disk[s.name];
    for (int n : {0, 1, s.min - 1, s.min, s.min + 1}) {
      disk[s.name] = pack(Bytes(n, 0));
      check(f.begin() == (s.exact ? n == s.min : n >= s.min), "unpacked minimum/exact length boundary");
      f.a.release();
    }
    disk[s.name] = good;
  }
  // A block-only map is permitted even when monster files are configured.
  disk["map"] = pack(Bytes(10000, 255));
  check(f.begin() && f.w->noMons && f.w->mapBlock(0, 0) == BACKBL, "block-only map and invalid cell");
  f.a.release();
  for (uint32_t len : {0xfffffu, 0x100000u, 0x7fffffffu, 0xffffffffu}) {
    disk["map"] = {uint8_t(len), uint8_t(len >> 8), uint8_t(len >> 16), uint8_t(len >> 24), 255};
    check(!f.begin(), "damaged packed size/deflate rejected"); f.a.release();
  }
  disk["empty"] = pack({});
  size_t n = 123;
  uint8_t* empty = f.a.readz("empty", &n);
  check(empty && n == 0, "valid empty packed payload is distinguishable from failed read");
  free(empty);
}

static void metadata() {
  Fixture f;
  check(!parseCfg("", *f.w, f.cfg), "empty configuration rejected");
  // Each required level field missing in turn, including a missing final count.
  std::string prefix = "level";
  const char* fields[] = {"map", "bbl", "cbl", "chr", "pal", "0", "0", "255", "255", "255", "255", "0"};
  for (auto field : fields) {
    check(!parseLevelLine(prefix.c_str(), nullptr, nullptr), "truncated required level fields rejected");
    prefix += " "; prefix += field;
  }
  check(parseLevelLine(prefix.c_str(), nullptr, nullptr), "minimum complete level record accepted");
  // All numeric level fields at signed DOS/file and host-parser limits.
  for (int v : {INT_MIN, -32768, -1, 0, 1, 99, 100, 150, 255, 32767, INT_MAX}) {
    std::string n = std::to_string(v), line = "level m b c h p";
    for (int i = 0; i < 7; ++i) line += " " + n;
    line += " mo mb " + n + " " + n;
    LevelFiles l = {}; Level lv = {};
    check(parseLevelLine(line.c_str(), &l, &lv), "numeric boundary parses");
    check(lv.startX == v && lv.startY == v && lv.scrollUp == v && lv.scrollDown == v &&
          lv.scrollLeft == v && lv.scrollRight == v, "coordinates and scroll fields preserved");
    check(l.nbl == (v < 0 ? 0 : v > BACKBL ? BACKBL : v) &&
          l.nmon == (v <= 0 ? 0 : v > LASTMON ? LASTMON : v) &&
          l.nmbl == (v <= 0 ? 0 : v > MONBL ? MONBL : v), "all counts clamped independently");
  }
  for (int n : {0, 1, 14, 15, 16, 40, 128}) {
    std::string label(n, 'x');
    parseHeaderLine(("title " + label).c_str(), *f.w, f.cfg);
    check(strlen(f.cfg.title) == size_t(n < 39 ? n : 39), "title length and terminator");
    Cfg cfg = {};
    parseHeaderLine(("key 0 " + label).c_str(), *f.w, cfg);
    parseHeaderLine(("ctr 29 " + label).c_str(), *f.w, cfg);
    check(cfg.nkeys == 1 && strlen(cfg.keys[0].label) == size_t(n < 15 ? n : 15), "key label including empty");
    check(cfg.nctr == 1 && strlen(cfg.ctr[0].label) == size_t(n < 19 ? n : 19), "counter label including empty");
  }
  // Every filename slot at its maximum valid length; absent required names reject.
  std::string name(15, 'n'); LevelFiles l = {};
  std::string line = "level";
  for (int i = 0; i < 5; ++i) line += " " + name;
  line += " 0 0 255 255 255 255 0 " + name + " " + name + " 1 1";
  check(parseLevelLine(line.c_str(), &l, nullptr), "15-byte file names accepted");
  for (const char* p : {l.map, l.bbl, l.cbl, l.chr, l.pal, l.mon, l.mbl}) check(strlen(p) == 15, "filename terminator");
  check(!parseLevelLine("level      0 0 0 0 0 0 0", &l, nullptr), "empty required file names rejected");
  for (int length : {16, 17, 255}) {
    std::string longName(length, 'n');
    check(!parseLevelLine(("level " + longName + " b c h p 0 0 0 0 0 0 0").c_str(), &l, nullptr),
          "overlong required filename rejects shifted fields");
  }
  resetHeader(*f.w, f.cfg);
  for (int i = 0; i < MAXLEVELS + 1; ++i) parseHeaderLine(levelLine, *f.w, f.cfg);
  for (int i = 0; i < MAXKEYSET + 1; ++i) parseHeaderLine("key 127 ", *f.w, f.cfg);
  for (int i = 0; i < 7; ++i) parseHeaderLine("ctr 29 ", *f.w, f.cfg);
  check(f.w->nlevels == MAXLEVELS && f.cfg.nkeys == MAXKEYSET && f.cfg.nctr == 6, "header table capacities");
  f.text.text = levelLine;
  for (int i = 0; i < MAXLINKS + 1; ++i) f.text.text += "link 0 -32768 32767 127 32767 -32768\n";
  f.text.text += "link 0 1 2 3 4\ncbln 0 0\ncbln 0 51\nsnd 0\n";
  Level lv = {};
  check(readLevelInfo(f.lines, "fixture.cfg", 0, l, lv) && lv.nlinks == MAXLINKS && l.ncbl == CHARBL && !l.snd[0],
        "link cap and truncated optional records");
  for (auto& k : lv.links) check(k.fromX == -32768 && k.fromY == 32767 && k.toLevel == 127 && k.toX == 32767 && k.toY == -32768,
                                "damaged GAM link fields preserved");
  check(!readLevelInfo(f.lines, "fixture.cfg", 1, l, lv), "missing level record rejected");
  for (int n : {1, 50}) {
    f.text.text = std::string(levelLine) + "cbln 0 " + std::to_string(n) + "\n";
    check(readLevelInfo(f.lines, "fixture.cfg", 0, l, lv) && l.ncbl == n, "hero count endpoints");
  }
  f.text.text = "level map bbl - - pal 0 0 255 255 255 255 1\n";
  check(!f.begin(), "absent inherited hero on first level rejected");
}

static void recordFields() {
  // Exercise every interpreted Attr field at both byte and signed-word endpoints.
  for (int v : {0, 1, 127, 128, 150, 199, 255}) {
    Bytes raw(BLOCK_BYTES, uint8_t(v));
    Attr a = World::attrOf(raw.data(), 1, 0);
    int word = int(int16_t(v | (v << 8))), byte = int(int8_t(v));
    check(a.birthMon == v && a.birthTime == word && a.solid == v && a.scorePts == word &&
          a.chHitPt == word && a.nextBl == v && a.ntime == word && a.touchBl == v &&
          a.grav == byte && a.obj == v && a.effect == v && a.eamt == word, "all block fields decoded");
  }
  Fixture f; check(f.begin(), "record fixture loads");
  for (int v : {-32768, -1, 0, 1, 32767}) {
    Bytes raw(BLOCK_BYTES, 0);
    for (int off : {401, 407, 409, 412, 418}) put16(raw, off, v);
    Attr a = World::attrOf(raw.data(), 1, 0);
    check(a.birthTime == v && a.scorePts == v && a.chHitPt == v && a.ntime == v && a.eamt == v, "signed attribute endpoints");
    Bytes mo(MONSTER_BYTES, 0);
    for (int off : {10, 12, 99, 122, 124, 126, 128, 130, 132, 134, 136, 138}) put16(mo, off, v);
    auto march = mon::marchAt(mo.data(), 0, 0);
    auto before = mon::marchAt(mo.data(), 0, -1);
    check(mon::pic(mo.data(), 0) == v && mon::picTime(mo.data(), 0) == v && mon::upscore(mo.data()) == v &&
          mon::activate(mo.data()) == v && mon::end(mo.data()) == v && mon::towards(mo.data()) == v &&
          march.x == v && march.y == v && march.pic == v && march.orient == v &&
          before.x == v && before.y == v && before.pic == v && before.orient == v, "monster signed fields and DOS pre-array read");
    for (int s : {0, MAXSEQ - 1}) {
      f.w->cseq = s; f.w->cframe = MAXFRAME - 1;
      size_t p = s * SEQ_BYTES + (MAXFRAME - 1) * FRAME_BYTES;
      for (int off : {0, 2, 4, 6, 8, 10}) {
        f.a.chr[p + off] = uint8_t(v); f.a.chr[p + off + 1] = uint8_t(unsigned(v) >> 8);
      }
      Frame fr = f.w->currentFrame();
      check(fr.x == v && fr.y == v && fr.orient == uint16_t(v) && fr.pic1 == uint16_t(v) &&
            fr.pic2 == uint16_t(v) && fr.pause == uint16_t(v), "all frame fields at first/last sequence");
      f.w->cframe = MAXFRAME;
      fr = f.w->currentFrame();
      check(fr.x == v && fr.y == v && fr.pic2 == CHARBL + 1, "past last frame uses DOS end marker");
    }
  }
  f.w->cseq = 0; f.w->cframe = -1;
  Frame fr = f.w->currentFrame();
  check(fr.x == 0 && fr.y == 0 && fr.pic1 == 0 && fr.pic2 == 0 && fr.pause == 0, "frame before first sequence is zero");
  f.w->cseq = 1;
  // PLAYGAME CHRFRAME permits frame -1: tail of the preceding sequence.
  const uint8_t* tail = f.a.chr + SEQ_BYTES - FRAME_BYTES;
  fr = f.w->currentFrame();
  check(fr.x == i16(tail) && fr.y == i16(tail + 2) && fr.pic2 == u16(tail + 8), "DOS preceding-sequence frame");
  for (int v : {0, 127, 128, 255}) {
    Bytes mo(MONSTER_BYTES, uint8_t(v));
    check(mon::power(mo.data()) == v && mon::thru(mo.data()) == int8_t(v) && mon::newmon(mo.data()) == int8_t(v),
          "monster byte field signedness");
    for (int s : {0, MAXSEQ - 1}) {
      auto p = f.a.chr + s * SEQ_BYTES;
      p[170] = p[171] = p[172] = p[174] = p[175] = p[178] = p[179] = uint8_t(v);
      check(f.w->seqKey(s) == (v | (v << 8)) && f.w->seqSound(s) == int8_t(v) && f.w->seqBloc(s) == v &&
            f.w->seqGround(s) == int8_t(v) && f.w->seqNorepeat(s) == int8_t(v) && f.w->seqMomentum(s) == int8_t(v),
            "all sequence metadata field boundaries");
    }
  }
  for (int b : {-1, 1, BACKBL, 255}) {
    Attr a = World::attrOf(nullptr, 0, b);
    check(!a.solid && !a.scorePts && !a.chHitPt && a.nextBl == b && a.touchBl == b && !a.effect, "missing block is inert");
  }
}

static int wrap(int n, int size) { return (n % size + size) % size; }
static void mapEdges() {
  Fixture f; check(f.begin(), "edge fixture loads");
  auto& w = *f.w;
  // Eight edge/corner locations; distinctive cells detect row aliasing and wrong neighbors.
  for (int y = 0; y < MWID; ++y) for (int x = 0; x < MLEN; ++x) w.map[y * MLEN + x] = uint8_t((x * 7 + y * 11) % BACKBL);
  for (int x : {0, 50, 99}) for (int y : {0, 50, 99}) {
    if (x == 50 && y == 50) continue;
    for (int ox : {0, 1, BLEN - 1}) for (int oy : {0, 1, BLEN - 1}) for (bool top : {false, true}) {
      w.cx[0] = x; w.cy[0] = y; w.cx[1] = ox; w.cy[1] = oy; w.topthere = top;
      Touch t = w.getTouch();
      int xs[] = {x, (x + 1) % MLEN, x, (x + 1) % MLEN, x, (x + 1) % MLEN};
      int ys[] = {(y + MWID - 1) % MWID, (y + MWID - 1) % MWID, y, y, (y + 1) % MWID, (y + 1) % MWID};
      bool active[] = {top, top && ox > 0, true, ox > 0, oy > 0, ox > 0 && oy > 0};
      for (int i = 0; i < 6; ++i)
        check(active[i] ? t.bx[i] == xs[i] && t.by[i] == ys[i] && t.num[i] == w.map[ys[i] * MLEN + xs[i]] :
                         t.bx[i] == MLEN && t.by[i] == MWID && t.num[i] == BACKBL, "exact edge/corner touch cells");
    }
    for (int dx : {-1, 0, 1}) for (int dy : {-1, 0, 1}) {
      w.cx[0] = x; w.cy[0] = y; w.cx[1] = dx; w.cy[1] = dy;
      w.normalizeChr(); w.chkMapBounds();
      check(w.cx[0] == wrap(x + (dx < 0 ? -1 : 0), MLEN) && w.cy[0] == wrap(y + (dy < 0 ? -1 : 0), MWID) &&
            w.cx[1] == wrap(dx, BLEN) && w.cy[1] == wrap(dy, BLEN), "DOS pixel normalization across map edge");
    }
  }
  for (int cell : {0, 149, 150, 151, 255}) {
    w.map[0] = uint8_t(cell);
    check(w.mapBlock(-100, 200) == (cell > BACKBL ? BACKBL : cell), "cell boundary wraps and empties");
  }
  // A full tick crosses each corner with the screen centered on the hero.
  memset(w.map, 0, sizeof w.map);
  for (int x : {0, 99}) for (int y : {0, 99}) {
    check(w.enterLevel(0, x, y), "corner entry");
    w.cx[1] = x ? 19 : 0; w.cy[1] = y ? 19 : 0;
    w.gravx = x ? 2 : -2; w.gravy = y ? 2 : -2;
    check(w.tick() == 0 && w.cx[0] == (x ? 0 : 99) && w.cy[0] == (y ? 0 : 99), "tick crosses both corner axes");
    uint8_t row[SIZEX + 2]; row[0] = row[SIZEX + 1] = 123;
    for (int gy = 0; gy < SIZEY; ++gy) w.renderRow(gy, row + 1);
    check(row[0] == 123 && row[SIZEX + 1] == 123, "edge rendering preserves row guards");
  }
  // Damaged entries must be normalized before scrolling or direct map writes.
  for (int v : {INT_MIN, -32768, -101, -1, 100, 200, 32767, INT_MAX}) {
    check(w.enterLevel(0, v, v), "damaged coordinate fixture enters");
    check(w.cx[0] == wrap(v, MLEN) && w.cy[0] == wrap(v, MWID), "damaged entry immediately wraps");
    check(w.mx == wrap(w.cx[0] - SCRNBLEN / 2, MLEN) &&
          w.my == wrap(w.cy[0] - SCRNBWID / 2, MWID), "damaged entry centers bounded viewport");
    Touch t = w.getTouch();
    check(t.bx[2] == wrap(v, MLEN) && t.by[2] == wrap(v, MWID), "damaged coordinates have bounded touch indices");
    w.dropSlot = 0; w.inv[0] = 42;
    check(w.dropBlock() && w.map[wrap(v, MWID) * MLEN + wrap(v, MLEN)] == 42, "damaged entry permits direct map write");
    w.cx[0] = w.cy[0] = v;
    w.chkMapBounds();
    check(w.cx[0] == wrap(v, MLEN) && w.cy[0] == wrap(v, MWID), "damaged coordinates wrap by full map size");
  }
}

static void links() {
  for (int dest : {-1, 0, 1, MAXLEVELS - 1, MAXLEVELS, 32767}) {
    Fixture f;
    f.text.text += "link 0 50 50 " + std::to_string(dest) + " 99 0\n";
    check(f.begin(), "link fixture loads");
    f.w->hold = true;
    char event = f.w->tick();
    check(event == (dest < 0 ? 'W' : 'L'), "link event or explicit win sentinel");
    if (dest < 0) { check(f.w->won && f.w->tick() == 'W', "win remains won"); continue; }
    check(f.w->level == 0 && f.w->pendKind == 'L' && f.w->pendLevel == dest &&
          f.w->pendX == 99 && f.w->pendY == 0, "held link preserves current scene and destination");
    bool loaded = f.w->finish();
    check(loaded == (dest == 0) && f.w->pendKind == 0 && !f.w->won, "missing scene returns false, never wins");
    if (loaded) check(f.w->cx[0] == 99 && f.w->cy[0] == 0, "valid link destination");
    else { // Caller must honor false before drawing/ticking a failed load (main.cpp does).
      check(f.w->finish(), "no pending transition after failure");
      check(f.w->restartLevel() == false, "restart of missing scene also returns false");
    }
  }
  for (int v : {-32768, 32767}) {
    Fixture f;
    f.text.text += "link 0 50 50 0 " + std::to_string(v) + " " + std::to_string(v) + "\n";
    check(f.begin(), "damaged link fixture loads");
    f.w->hold = true;
    check(f.w->tick() == 'L' && f.w->finish(), "damaged link transition reports loaded");
    // Link destinations use the same normalized entry path.
    check(f.w->cx[0] == wrap(v, MLEN) && f.w->cy[0] == wrap(v, MWID), "damaged link destination wraps by map size");
  }
}

static void monsters() {
  Fixture f; check(f.begin(), "monster fixture loads");
  auto& w = *f.w;
  check(!w.noMons && w.nmon == LASTMON && w.nmbl == MONBL, "maximum monster files load");
  // Maximum placed-monster map: all 200 records, including the last slot/type/cell.
  Bytes placed(MLEN * MWID * 2 + LASTMM * 3, 0);
  memset(placed.data() + MLEN * MWID, LASTMM, MLEN * MWID);
  for (int i = 0; i < LASTMM; ++i) {
    size_t p = MLEN * MWID * 2 + i * 3;
    placed[p] = uint8_t(i % LASTMON); placed[p + 1] = uint8_t(i % MLEN); placed[p + 2] = uint8_t(i % MWID);
  }
  disk["map"] = pack(placed);
  check(f.begin(), "maximum placed monsters enter from file");
  for (int i = 0; i < LASTMM; ++i)
    check(w.mm[i].monnum == i % LASTMON && w.mi[i].curx[0] == i % MLEN && w.mi[i].cury[0] == i % MWID,
          "every placed record initializes live position");
  for (auto& slot : w.mm) slot.monnum = LASTMON;
  for (int n : {-1, 0, LASTMON - 1, LASTMON, 255}) check((w.monAt(n) != nullptr) == (n >= 0 && n < LASTMON), "monster record boundary");
  for (int i = 0; i < LASTMM; ++i) {
    check(w.birthMon(LASTMM, i % LASTMON, 1 + i % 14, 1 + i / 14 % 8) == i, "all 200 live slots allocated in order");
    w.resetMonInfo(i); w.mi[i].curPic = 0;
    w.mi[i].scrX = 1 + i; w.mi[i].scrY = 20;
  }
  auto before = w.mm[LASTMM - 1];
  check(w.birthMon(LASTMM, 0, 99, 99) == LASTMM && w.mm[LASTMM - 1].monnum == before.monnum,
        "201st monster refused without replacing live slot");
  w.buildDrawList();
  check(w.ndraw == MAXMONSSHOWN, "draw list capped at 50");
  for (int i = 0; i < MAXMONSSHOWN; ++i) check(w.draw[i].x == 1 + i && w.draw[i].pic == 0, "draw order is slot order");
  check(w.killMon(LASTMM - 1), "last live slot can be killed");
  check(w.birthMon(LASTMM, LASTMON - 1, 99, 99) == LASTMM - 1, "last free slot reused");
  // Stationary, equal-power, transparent sprites avoid unrelated collision effects.
  w.mx = w.my = 0; w.scrx = w.scry = OFFSCRN;
  w.moveAllMonsters(); w.buildDrawList();
  int live = 0;
  for (auto& slot : w.mm) live += slot.monnum < LASTMON;
  check(live == LASTMM && w.ndraw <= MAXMONSSHOWN, "all 200 monsters survive a movement/draw pass");
}

static void counters() {
  Fixture f; check(f.begin(), "counter fixture loads");
  auto& w = *f.w;
  Attr a = {}; a.effect = 1;
  // PLAYGAME DoBlockTouchChar uses a 16-bit int for lives, a 32-bit long for score.
  for (auto pair : {std::pair<int, int>{32767, 1}, {-32768, -1}, {0, -32768}, {32767, 32767}}) {
    w.lives = pair.first; a.eamt = pair.second;
    check(w.doBlockTouchChar(a) == 1 && w.lives == int16_t(pair.first + pair.second), "lives wrap as DOS int16");
  }
  a = {}; w.score = 32767; a.scorePts = 1;
  check(w.doBlockTouchChar(a) == 1 && w.score == 32768, "score does not wrap at 16 bits");
  w.score = INT_MAX - 32767; a.scorePts = 32767;
  check(w.doBlockTouchChar(a) == 1 && w.score == INT_MAX, "score reaches positive int32 endpoint");
  w.score = INT_MIN + 32768; a.scorePts = -32768;
  check(w.doBlockTouchChar(a) == 1 && w.score == INT_MIN, "score reaches negative int32 endpoint");
  // DOS signed-long overflow is undefined; the core clamps at int32 limits.
  for (int delta : {-32768, -1, 0, 1, 32767}) {
    for (int start : {INT_MIN, INT_MIN + 1, -32768, 0, 32767, INT_MAX - 1, INT_MAX}) {
      w.score = start; a.scorePts = delta;
      int64_t sum = int64_t(start) + delta;
      int expected = sum < INT_MIN ? INT_MIN : sum > INT_MAX ? INT_MAX : int(sum);
      check(w.doBlockTouchChar(a) == 1 && w.score == expected, "block score clamps only outside int32 range");
    }
  }
  for (int effect : {0, 2, 29, 30, 31, 255}) {
    a = {}; a.effect = effect; a.eamt = 1;
    w.score = 0;
    for (int& m : w.meter) m = 10;
    check(w.doBlockTouchChar(a) == 1, "effect boundary returns successful touch");
    for (int i = 0; i < 30; ++i) check(w.meter[i] == (i == effect && effect > 2 && effect < 30 ? 11 : 10), "effect modifies only legal counter");
  }
  for (int effect : {3, 20, 29}) {
    a = {}; a.effect = effect; a.eamt = -1; a.scorePts = 7;
    w.meter[effect] = 1; w.score = 0;
    check(w.doBlockTouchChar(a) == 0 && w.meter[effect] == 1 && w.score == 0, "empty money/custom counter refuses entire touch");
    w.meter[effect] = INF_REPEAT; a.eamt = 32767;
    check(w.doBlockTouchChar(a) == 1 && w.meter[effect] == INF_REPEAT, "infinite counter remains infinite");
    w.meter[effect] = 32766; a.eamt = 1;
    check(w.doBlockTouchChar(a) == 1 && w.meter[effect] == 32766, "finite counter saturates below infinity");
  }
  a = {}; a.chHitPt = 1; w.hitpts = 32766; w.cseq = IDLESEQ;
  check(w.doBlockTouchChar(a) == 1 && w.hitpts == 32766, "hitpoints saturate below infinity");
  w.lives = 0; w.hold = true; w.cseq = DIESEQ; w.cframe = MAXFRAME - 1; w.nextAni = w.clock;
  check(w.tick() == 'D' && w.finish() && w.lives == 0, "death at zero lives does not underflow");
}

static void timerWrap() {
  Fixture f; check(f.begin(), "timer fixture loads");
  auto& w = *f.w;
  w.noMons = true; w.clock = UINT32_MAX - 1; w.ticks = UINT32_MAX - 1;
  w.gclk = w.clock; w.nextAni = UINT32_MAX; w.cframe = 0;
  check(w.tick() == 0 && w.clock == UINT32_MAX && w.ticks == UINT32_MAX && w.cframe == 1 && w.nextAni == 0,
        "animation deadline wraps with clock addition");
  check(w.tick() == 0 && w.clock == 0 && w.ticks == 0 && w.cframe == 2 && w.nextAni == 1,
        "clock and tick counter wrap to zero and animation continues");
  check(w.tick() == 0 && w.clock == 1 && w.cframe == 3, "tick after timer wrap");
  // PLAYGAME moveChars/changeBlocks use raw unsigned >= / <= comparisons.
  // Pin their inherited behavior, including an early deadline after addition wraps.
  w.clock = UINT32_MAX - 1; w.nextAni = 1; w.cframe = 0; w.gclk = w.clock;
  check(!w.moveChars() && w.cframe == 1, "DOS raw deadline comparison fires wrapped animation early");
  w.clock = 0; w.nextAni = UINT32_MAX; w.cframe = 0; w.gclk = 0;
  check(!w.moveChars() && w.cframe == 0, "DOS pre-wrap animation deadline waits after wrap");
  f.a.bbl[411] = 1; // next block, outside the one-block file: empty attributes.
  w.blkmap[0] = 0; w.nxtchg[0] = UINT32_MAX; w.clock = 0;
  w.changeBlocks(); check(w.blkmap[0] == 0, "pre-wrap block deadline waits after wrap");
  w.clock = UINT32_MAX; w.changeBlocks();
  check(w.blkmap[0] == 1, "block changes at unsigned deadline endpoint");
  // Monster deadlines follow the same unsigned comparison; use stationary records.
  w.mm[0].monnum = 0; w.resetMonInfo(0); w.mi[0].curx[0] = w.mi[0].cury[0] = 1;
  w.mi[0].nextAni = UINT32_MAX; w.mi[0].nextMove = UINT32_MAX; w.clock = 0;
  w.moveAllMonsters();
  check(w.mi[0].curPic == -1 && w.mi[0].nextMove == UINT32_MAX, "monster waits on pre-wrap deadline");
  w.clock = UINT32_MAX; w.moveAllMonsters();
  check(w.mi[0].curPic == 0 && w.mi[0].nextMove == 0, "monster movement deadline wraps");
}

static void monsterScores() {
  Fixture f; check(f.begin(), "monster score fixture loads");
  auto& w = *f.w;
  memset(f.a.mbl, 0, BLOCK_BYTES);  // Opaque, overlapping pictures ensure collision.
  for (int stronger : {0, 1}) for (int delta : {-32768, -1, 0, 1, 32767}) {
    for (int start : {INT_MIN, INT_MIN + 32768, 0, INT_MAX - 32767, INT_MAX}) {
      for (int m = 0; m < 2; ++m) {
        uint8_t* mo = f.a.mon + m * MONSTER_BYTES;
        mo[8] = m == stronger ? 2 : 1;
        mo[99] = uint8_t(delta); mo[100] = uint8_t(unsigned(delta) >> 8);
        w.mm[m] = {uint8_t(m), 1, 1};
        w.resetMonInfo(m); w.mi[m].curPic = 0;
        w.mi[m].fromchar = m == stronger ? 1 : 0;
      }
      w.score = start;
      int64_t sum = int64_t(start) + delta;
      int expected = sum < INT_MIN ? INT_MIN : sum > INT_MAX ? INT_MAX : int(sum);
      check(w.doCollision(0, 1, 0) == LASTMM && w.score == expected &&
            w.mm[0].monnum == LASTMON && w.mm[1].monnum == LASTMON,
            "both monster score paths clamp only outside int32 range");
    }
  }
}

int main() {
  damagedAssets(); metadata(); recordFields(); mapEdges(); links(); monsters(); counters(); monsterScores(); timerWrap();
  printf("coreedgetest: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
