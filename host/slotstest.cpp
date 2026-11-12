// Host test for the ten save slots (src/saveslots.h) and their list screen (src/slotdraw.h): listing, naming, overwrite and load.
// Build: c++ -std=c++17 -O1 -Isrc host/slotstest.cpp -o slotstest   (add -DPOCKET_CYD28 for 320 x 240)
#include <assert.h>
#include <stdio.h>
#include <map>
#include <string>
namespace textdatum_t { constexpr int middle_center = 4, top_left = 0; }
#include "canvas.h"
#include "slotdraw.h"
#include "spaced.h"
#include "gmsave.h"

using namespace gm;
typedef std::map<std::string, std::string> Fs;
static bool ex(void* u, const char* p) { return ((Fs*)u)->count(p) > 0; }
static bool rd(void* u, const char* p, void* b, size_t n) { auto& f = *(Fs*)u; auto it = f.find(p); if (it == f.end() || it->second.size() != n) return false; memcpy(b, it->second.data(), n); return true; }
static bool wr(void* u, const char* p, const void* b, size_t n) { (*(Fs*)u)[p] = std::string((const char*)b, n); return true; }
static bool failWr(void*, const char*, const void*, size_t) { return false; }

int main(int argc, char** argv) {
  Fs fs;
  SlotStore st{&fs, ex, rd, wr};
  const uint32_t game = 0xC0FFEE01u, other = 0x12345678u;
  SlotList l;
  // listing: nothing saved, ten empty slots, nothing to load
  slotsList(st, game, l);
  assert(!slotsAny(l));
  for (int i = 0; i < SLOTS; i++) assert(!l.used[i] && !strcmp(slotLabel(l, i), "empty"));
  // naming: a slot's file and name; the names table is DOS's 210 bytes and keeps the other names
  char path[24];
  slotPath(path, sizeof path, game, 3);
  assert(!strcmp(path, "/c0ffee01.s3"));
  fs[path] = "state";
  assert(slotSetName(st, game, 3, "Before the boss"));
  slotPath(path, sizeof path, game, 0);
  fs[path] = "state0";
  assert(slotSetName(st, game, 0, "  Level one  "));
  char np[24];
  slotNamesPath(np, sizeof np, game);
  assert(fs[np].size() == (size_t)SLOT_TABLE && SLOT_TABLE == 210);
  assert(fs[np].substr(3 * 21, 15) == std::string("Before the boss") && fs[np][3 * 21 + 15] == 0);
  slotsList(st, game, l);
  assert(slotsAny(l) && l.used[0] && l.used[3] && !l.used[1]);
  assert(!strcmp(l.name[3], "Before the boss") && !strcmp(l.name[0], "Level one") && !strcmp(slotLabel(l, 1), "empty"));
  slotsList(st, other, l);  // another game's slots are its own
  assert(!slotsAny(l));
  // a name is cut to 20 letters and printable ASCII; a slot with a file but no name reads "Saved game"
  assert(slotSetName(st, game, 5, "ABCDEFGHIJKLMNOPQRSTUVWXYZ"));
  slotPath(path, sizeof path, game, 5);
  fs[path] = "s";
  slotPath(path, sizeof path, game, 7);
  fs[path] = "s";
  slotsList(st, game, l);
  assert(!strcmp(l.name[5], "ABCDEFGHIJKLMNOPQRST") && !strcmp(l.name[7], "Saved game"));
  assert(!slotSetName(st, game, 10, "x") && !slotSetName(st, game, -1, "x"));
  // an unreadable (short) names table still lists the slots
  fs[np] = "short";
  slotsList(st, game, l);
  assert(l.used[3] && !strcmp(l.name[3], "Saved game"));
  // overwrite: an empty slot saves at once; a used one asks once, for 3 seconds
  slotsList(st, game, l);
  SlotArm a;
  assert(a.tap(l, 1, true, 100) == SLOT_GO);
  assert(a.tap(l, 3, true, 100) == SLOT_ARMED && a.armed == 3);
  assert(a.tap(l, 3, true, 2000) == SLOT_GO && a.armed == -1);
  assert(a.tap(l, 3, true, 100) == SLOT_ARMED && a.tap(l, 0, true, 200) == SLOT_ARMED && a.armed == 0);  // another slot re-arms
  assert(a.tap(l, 0, true, 3201) == SLOT_ARMED);  // too late
  assert(a.expire(6202) && a.armed == -1 && !a.expire(6203));
  assert(a.tap(l, -1, true, 0) == SLOT_NONE);
  // load: a used slot goes, an empty one does nothing, and nothing asks
  assert(a.tap(l, 3, false, 0) == SLOT_GO && a.tap(l, 1, false, 0) == SLOT_NONE && a.armed == -1);
  // a failed names write is reported (the caller says "Could not save")
  SlotStore bad{&fs, ex, rd, failWr};
  assert(!slotSetName(bad, game, 2, "x"));
  // a saved world goes through a slot's file and comes back (gmsave.h)
  {
    struct Buf { std::string v; size_t at = 0; } b;
    auto w = [](void* u, const void* p, size_t n) { ((Buf*)u)->v.append((const char*)p, n); return true; };
    auto r = [](void* u, void* p, size_t n) { auto* x = (Buf*)u; if (x->at + n > x->v.size()) return false; memcpy(p, x->v.data() + x->at, n); x->at += n; return true; };
    static World world;
    memset(&world, 0, sizeof world);
    world.level = 2; world.score = 777; world.entry.magic = ENTRY_MAGIC; world.entry.level = 2;
    assert(saveWorld(world, w, &b));
    slotPath(path, sizeof path, game, 9);
    assert(st.writeAll(&fs, path, b.v.data(), b.v.size()) && slotSetName(st, game, 9, "Last one"));
    Buf in;
    in.v = fs[path];
    assert(saveCheck(r, &in, in.v.size()));
    in.at = 0;
    EntryState e;
    assert(saveReadEntry(r, &in, e) && e.level == 2);
    slotsList(st, game, l);
    assert(l.used[9] && !strcmp(l.name[9], "Last one"));
  }
  // the list's rows: ten, inside the screen, 36 px or taller, apart, clear of the Close X, and slotAt finds each by its middle
  for (int W : {320, 480}) {
    int H = W == 320 ? 240 : 320;
    for (int i = 0; i < SLOTS; i++) {
      int x0, y0, x1, y1;
      slotRect(W, H, i, x0, y0, x1, y1);
      assert(y1 - y0 >= 36 && x1 - x0 >= 100 && x0 >= 0 && x1 <= W && y1 <= H);
      assert(slotAt(W, H, (x0 + x1) / 2, (y0 + y1) / 2) == i && slotAt(W, H, x0, y0) == i && slotAt(W, H, x1 - 1, y1 - 1) == i);
      assert(!cx::hit(W, x1 - 1, y0) || y0 >= cx::hitBox(W).y1);
      for (int j = i + 1; j < SLOTS; j++) {
        int a0, b0, a1, b1;
        slotRect(W, H, j, a0, b0, a1, b1);
        assert(a1 <= x0 || a0 >= x1 || b1 <= y0 || b0 >= y1);
      }
    }
    assert(slotAt(W, H, 0, 0) == -1);
  }
  // letter spacing: every glyph has a gap after it, and a long label is cut with "..." inside its room, never clipped
  {
    char out[40];
    sp::ellipsize("1 Twenty chars of name", out, sizeof out, 143, 1);
    assert(strlen(out) == 20 && !strcmp(out + 17, "..."));
    assert(sp::width((int)strlen(out), 1) <= 143 && sp::adv(1) == 7 && sp::adv(2) == 14);
    sp::ellipsize("Short", out, sizeof out, 143, 1);
    assert(!strcmp(out, "Short"));
  }
  // proportional menu text: 1 font px between letters, narrow i/l, wide W/M, fixed digits, 3 px space, cut inside its room
  {
    const sp::InkTable& t = sp::ink();
    assert(t.w['i' - 32] < t.w['W' - 32] && t.w['l' - 32] < t.w['M' - 32] && t.w['W' - 32] <= 6);
    assert(sp::pwidth("il", 1) == t.w['i' - 32] + 1 + t.w['l' - 32]);  // ink + exactly 1 px + ink
    assert(sp::pwidth("il", 2) == (t.w['i' - 32] + 1 + t.w['l' - 32]) * 2);
    assert(sp::pUnits(' ') == 3 && sp::pUnits('1') == sp::pUnits('8') && sp::pUnits('0') == sp::pUnits('9'));
    assert(sp::pwidth("Level 1", 1) == sp::pwidth("Level 8", 1));
    assert(sp::pwidth("illi t1", 2) < 7 * 6 * 2 && sp::pwidth("WM", 1) == 2 * sp::ink().w['W' - 32] + 1);  // tighter than the monospace cell
    char out[40];
    sp::pellipsize("A Very Long Game Title Here", out, sizeof out, 100, 1);
    assert(sp::pwidth(out, 1) <= 100 && !strcmp(out + strlen(out) - 3, "..."));
    sp::pellipsize("Short", out, sizeof out, 100, 1);
    assert(!strcmp(out, "Short") && sp::pfit("Short", 4, 1) < 5);
  }
  // the list draws (one render per state, to look at when a directory is given)
  {
    Canvas g(scr::W, scr::H);
    slotsList(st, game, l);
    const char* names[3] = {"save", "load", "overwrite"};
    for (int k = 0; k < 3; k++) {
      slotdraw::screen(g, scr::W, scr::H, l, k != 1, k == 2 ? 3 : -1);
      if (argc > 1) {  // a PPM per state to look at: slotstest <dir>
        std::string f = std::string(argv[1]) + "/slots-" + std::to_string(scr::W) + "-" + names[k] + ".ppm";
        FILE* o = fopen(f.c_str(), "wb");
        fprintf(o, "P6 %d %d 255\n", scr::W, scr::H);
        for (uint16_t c : g.px) { uint8_t rgb[3] = {(uint8_t)((c >> 11) << 3), (uint8_t)(((c >> 5) & 63) << 2), (uint8_t)((c & 31) << 3)}; fwrite(rgb, 1, 3, o); }
        fclose(o);
      }
    }
  }
  printf("slotstest %d: passed\n", scr::W);
  return 0;
}
