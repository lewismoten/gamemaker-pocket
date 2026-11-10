// Renders of the game menu, the Top Ten and the name entry as the board draws them (src/dosmenudraw.h, gmmenu.h, picker_draw.h and the
// card bands), at the size this is built for: dosmenushots <data dir> <out dir> ["<title>"]; build with -lz, add -DPOCKET_CYD28 for 320 x 240.
// Writes <out>/dosmenu-<w>.png (choice 1 over the default menu screen: a game with no menu picture packed), dosmenu-<w>-3.png (Read Storyline
// chosen), topten-<w>.png, name-<w>.png and name-<w>-typed.png. Also checks the touch map against what is drawn: each word's pixels sit in its row.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
namespace textdatum_t { constexpr int middle_center = 4, top_left = 0; }
#include "canvas.h"
#include "carddraw.h"
#include "dosmenudraw.h"
#include "picker_draw.h"
#include "softgfx.h"
#include "gmcards.h"
#include <zlib.h>

struct HC : Canvas {
  HC(int w, int h) : Canvas(w, h) {}
  void pushImage(int x, int y, int w, int h, const uint16_t* d) { for (int r = 0; r < h; r++) for (int c = 0; c < w; c++) put(x + c, y + r, carddraw::native(d[r * w + c])); }
};
static uint16_t rgb565(int r6, int g6, int b6) { auto v8 = [](int c) { c &= 63; return (c << 2) | (c >> 4); }; int r = v8(r6), g = v8(g6), b = v8(b6); return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)); }
static void setPal(uint16_t* pal, const uint8_t* p) { for (int i = 0; i < 256; i++) pal[i] = carddraw::native(rgb565(p[i * 3], p[i * 3 + 1], p[i * 3 + 2])); }
static std::vector<uint8_t> pakBytes;
static bool inflateRaw(const uint8_t* src, size_t n, uint8_t* dst, size_t cap) {
  z_stream z = {};
  if (inflateInit2(&z, -15) != Z_OK) return false;
  z.next_in = (Bytef*)src; z.avail_in = (uInt)n; z.next_out = dst; z.avail_out = (uInt)cap;
  int r = inflate(&z, Z_FINISH);
  bool ok = r == Z_STREAM_END && z.total_out == cap;
  inflateEnd(&z);
  return ok;
}
static uint8_t* readRange(size_t off, size_t len) {
  if (off + len > pakBytes.size()) return nullptr;
  uint8_t* p = (uint8_t*)malloc(len + 1);
  memcpy(p, pakBytes.data() + off, len);
  return p;
}
static std::vector<uint8_t> slurp(const std::string& path) {
  std::vector<uint8_t> v;
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) return v;
  uint8_t b[4096];
  size_t n;
  while ((n = fread(b, 1, sizeof b, f)) > 0) v.insert(v.end(), b, b + n);
  fclose(f);
  return v;
}
static int fails = 0;
static void check(bool ok, const char* what) { if (!ok) { fails++; printf("FAIL %s\n", what); } }

int main(int argc, char** argv) {
  if (argc < 3) return 2;
  std::string dir = argv[1], out = argv[2];
  const char* title = argc > 3 ? argv[3] : "Moving and Pushing";
  std::vector<uint8_t> font(95 * 8);
  FILE* f = fopen((dir + "/font.bin").c_str(), "rb");
  if (!f || fread(font.data(), 1, font.size(), f) != font.size()) { printf("no font\n"); return 2; }
  fclose(f);
  static uint16_t pal[256];
  static uint16_t band[scr::W * 30];
  std::vector<uint8_t> pic(gm::PIC_BYTES);
  std::string w = std::to_string(scr::W);

  for (int choice : {1, 3}) {
    HC g(scr::W, scr::H);
    gm::defaultMenuScreen(pic.data());
    uint8_t under[gmmenu::M_COUNT][gm::MARK_W * gm::MARK_H];
    gm::menuUnder(pic.data() + 768, under);
    gm::drawGameMenu(pic.data(), font.data(), title);
    gm::drawMenuMarker(pic.data() + 768, choice, true, gm::dosBlue(pic.data()), under);
    setPal(pal, pic.data());
    carddraw::pushPicture(g, pic.data() + 768, pal, band, 30);
    cx::draw(g, scr::W);
    for (int k = 0; k < gmmenu::K_COUNT; k++) { int x0, y0, x1, y1; gmmenu::keyRect(k, x0, y0, x1, y1); pk::drawActionButton(g, x0, y0, x1, y1, gmmenu::keyLabel(k)); }
    writePng(out + "/dosmenu-" + w + (choice == 1 ? "" : "-3") + ".png", g.W, g.H, rgbOf(g));
    if (choice == 1) {  // each word's pixels are inside the touch row of its choice
      const uint8_t* px = pic.data() + 768;
      for (int c = 1; c <= gmmenu::M_COUNT; c++) {
        bool inside = false;
        for (int y = gmmenu::rowY(c); y < gmmenu::rowY(c) + 8 && !inside; y++)
          for (int x = gmmenu::WORD_X; x < gmmenu::WORD_X + 8 * 18; x++)
            if (px[y * 320 + x] == gm::dosBlue(pic.data()) && gmmenu::rowHit(scr::up(x), scr::up(y)) == c) { inside = true; break; }
        check(inside, "a word's drawn pixels are in its touch row");
      }
    }
  }
  {  // the Top Ten with a few records and the latest score
    HC g(scr::W, scr::H);
    gm::HiTable t;
    memset(&t, 0, sizeof t);
    const char* names[] = {"DOC LEO", "KIT", "TEDDY", "GWINN", "AVA"};
    long sc[] = {48200, 31050, 22900, 9100, 640};
    for (int i = 4; i >= 0; i--) { int p = gm::hiInsert(t, sc[i]); gm::hiSetName(t, p, names[i]); }
    std::fill(pic.begin(), pic.end(), 0);
    pic[3] = 63; pic[4] = 10; pic[5] = 10; pic[6] = 10; pic[7] = 10; pic[8] = 63;
    gm::drawTopTen(pic.data(), font.data(), t, 12345);
    setPal(pal, pic.data());
    carddraw::pushPicture(g, pic.data() + 768, pal, band, 30);
    writePng(out + "/topten-" + w + ".png", g.W, g.H, rgbOf(g));
  }
  for (int typed = 0; typed < 2; typed++) {  // the name entry
    HC g(scr::W, scr::H);
    int titleH = pk::searchTitleH(scr::W);
    const char* name = typed ? "DOC LEO" : "";
    g.fillScreen(0);
    pk::drawKeyboard(g, scr::W, scr::H, titleH, name, "Type your name");
    g.fillRect(0, 0, scr::W, titleH, 0x10A5);
    g.setTextSize(2);
    g.setTextColor(pk::C_YELLOW);
    g.setCursor(8, (titleH - 16) / 2);
    g.print(scr::SMALL ? "Top Ten! Number 2" : "Congratulations! You are number 2");
    writePng(out + "/name-" + w + (typed ? "-typed" : "") + ".png", g.W, g.H, rgbOf(g));
  }
  if (argc > 4) {  // a game packed for the card (pocketData.ts --out): <base>.cfg and <base>.pak, its menu, credits and Top Ten pictures
    std::string base = argv[4];
    std::vector<uint8_t> cv = slurp(base + ".cfg");
    pakBytes = slurp(base + ".pak");
    std::string cfgText(cv.begin(), cv.end());
    gm::Assets assets;
    assets.inflate = inflateRaw;
    auto first = [&](const char* where, int skip) -> uint8_t* {
      gm::Card cards[gm::MAX_CARDS];
      int n = gm::cardsOf(cfgText.c_str(), where, cards, gm::MAX_CARDS);
      for (int i = 0, seen = 0; i < n; i++)
        if ((cards[i].kind == 'g' || cards[i].kind == 'p') && seen++ == skip) return gm::loadCardPicture(assets, readRange, cards[i]);
      return nullptr;
    };
    auto show = [&](HC& g, const uint8_t* p) { setPal(pal, p); carddraw::pushPicture(g, p + 768, pal, band, 30); };
    uint8_t* m = first("m", 0);
    check(m != nullptr, "the pack has a menu picture");
    if (m) {
      HC g(scr::W, scr::H);
      uint8_t under[gmmenu::M_COUNT][gm::MARK_W * gm::MARK_H];
      gm::menuUnder(m + 768, under);
      gm::drawGameMenu(m, font.data(), title);
      gm::drawMenuMarker(m + 768, 1, true, gm::dosBlue(m), under);
      show(g, m);
      cx::draw(g, scr::W);
      for (int k = 0; k < gmmenu::K_COUNT; k++) { int x0, y0, x1, y1; gmmenu::keyRect(k, x0, y0, x1, y1); pk::drawActionButton(g, x0, y0, x1, y1, gmmenu::keyLabel(k)); }
      writePng(out + "/dosmenu-pic-" + w + ".png", g.W, g.H, rgbOf(g));
      free(m);
    }
    for (int k = 0; k < 3; k++) {  // credits pictures (a patch card prints as the picture it makes)
      uint8_t* c = first("c", k);
      if (!c) break;
      HC g(scr::W, scr::H);
      show(g, c);
      cx::draw(g, scr::W);
      writePng(out + "/credits-" + w + "-" + std::to_string(k + 1) + ".png", g.W, g.H, rgbOf(g));
      free(c);
    }
    uint8_t* b = first("b", 0);
    check(b != nullptr, "the pack has a Top Ten background");
    if (b) {
      gm::HiTable t;
      memset(&t, 0, sizeof t);
      const char* names[] = {"DOC LEO", "KIT", "TEDDY", "GWINN", "AVA"};
      long sc[] = {48200, 31050, 22900, 9100, 640};
      for (int i = 4; i >= 0; i--) { int p = gm::hiInsert(t, sc[i]); gm::hiSetName(t, p, names[i]); }
      std::vector<uint8_t> copy(b, b + gm::PIC_BYTES);
      {
        HC g(scr::W, scr::H);
        gm::drawTopTen(b, font.data(), t, 12345);
        show(g, b);
        cx::draw(g, scr::W);
        writePng(out + "/topten-bg-" + w + ".png", g.W, g.H, rgbOf(g));
      }
      {  // the name entry over the same picture
        HC g(scr::W, scr::H);
        int titleH = pk::searchTitleH(scr::W);
        show(g, copy.data());
        pk::drawKeyboard(g, scr::W, scr::H, titleH, "DOC LEO", "Type your name", false);
        g.fillRect(0, 0, scr::W, titleH, 0x10A5);
        g.setTextSize(2);
        g.setTextColor(pk::C_YELLOW);
        g.setCursor(8, (titleH - 16) / 2);
        g.print(scr::SMALL ? "Top Ten! Number 2" : "Congratulations! You are number 2");
        writePng(out + "/name-bg-" + w + ".png", g.W, g.H, rgbOf(g));
      }
      free(b);
    }
  }
  printf(fails ? "dosmenushots FAILED\n" : "dosmenushots ok\n");
  return fails != 0;
}
