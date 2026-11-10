// Touch-target pictures: renders the library, Details, each Settings tab, the Search keyboard, the Pause menu and the
// load-failure dialog the way the firmware draws them, then outlines every touch rectangle (a color per action) at 2x, nearest
// neighbor. Host only; run it once on a before tree and once on an after tree (-I<tree>/src) to compare two commits.
//   hitshots <out dir> <tag>       build with -lz; add -DPOCKET_CYD28 for the 320 x 240 board. Writes <screen>-<w>-<tag>.png
// Rectangles come from the real hit code (prs::under, st::hit, ovl::pauseHit, pk::inFailButton) by testing every pixel. The
// search keyboard's taps live in main.cpp, so its rectangles are written out here from that code (HIT_BEFORE: the old ones).
#include <string>
#include <vector>
namespace textdatum_t { constexpr int middle_center = 4, top_left = 0; }
#include "canvas.h"
#include "cardmsg.h"
#include "pressrect.h"
#include "library.h"
using Rects = std::vector<prs::Rect>;

static Rects scan(int W, int H, bool (*look)(void*, int, int, prs::Rect&), void* u) {
  Rects out;
  for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
    prs::Rect r;
    if (!look(u, x, y, r)) continue;
    bool seen = false;
    for (auto& o : out) if (o.x0 == r.x0 && o.y0 == r.y0 && o.x1 == r.x1 && o.y1 == r.y1) { seen = true; break; }
    if (!seen) out.push_back(r);
  }
  return out;
}
static void save(const std::string& dir, const std::string& name, const Canvas& c, const Rects& rs) {
  static const uint8_t pal[][3] = {{255, 64, 64}, {64, 255, 96}, {80, 160, 255}, {255, 0, 255}, {0, 255, 255}, {255, 160, 0}, {255, 255, 255}, {170, 120, 255}};
  std::vector<uint8_t> src = rgbOf(c), img((size_t)c.W * 2 * c.H * 2 * 3);
  int W2 = c.W * 2;
  for (int y = 0; y < c.H * 2; y++) for (int x = 0; x < W2; x++) for (int k = 0; k < 3; k++) img[((size_t)y * W2 + x) * 3 + k] = src[((size_t)(y / 2) * c.W + x / 2) * 3 + k];
  auto mix = [&](int x, int y, const uint8_t* col, int a) {  // a in 0..100
    if (x < 0 || y < 0 || x >= W2 || y >= c.H * 2) return;
    for (int k = 0; k < 3; k++) { uint8_t& p = img[((size_t)y * W2 + x) * 3 + k]; p = (p * (100 - a) + col[k] * a) / 100; }
  };
  int n = 0;
  for (auto& r : rs) {
    const uint8_t* col = pal[n++ % 8];
    int x0 = r.x0 * 2, y0 = r.y0 * 2, x1 = r.x1 * 2, y1 = r.y1 * 2;
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) mix(x, y, col, (x - x0 < 3 || x1 - x <= 3 || y - y0 < 3 || y1 - y <= 3) ? 85 : 20);
  }
  writePng(dir + "/" + name + ".png", W2, c.H * 2, img);
}
struct Shot : Canvas {
  using Canvas::Canvas;
  void pushImage(int x, int y, int w, int h, const uint16_t* p) { for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) put(x + i, y + j, p[j * w + i]); }
};
int main(int argc, char** argv) {
  if (argc != 3) { puts("usage: hitshots <out dir> <tag>"); return 2; }
  std::string dir = argv[1], tag = argv[2], w = std::to_string(scr::W);
  const int W = scr::W, H = scr::H;
  auto name = [&](const std::string& s) { return s + "-" + w + "-" + tag; };
  prs::Ctx ctx;
  ctx.W = W; ctx.H = H; ctx.tiles = 6; ctx.back = ctx.prev = ctx.next = ctx.action = true;
  auto under = [](void* u, int x, int y, prs::Rect& r) { return prs::under(*(prs::Ctx*)u, x, y, r); };
  {  // library
    Shot g(W, H);
    pk::clearTiles(g, W); pk::libraryFrame(g, W, H, "Collections", 0, 2, "Details");
    const char* names[] = {"Continue", "Recently played", "Favorites", "Starters", "Sandbox", "Bizarre"};
    for (int i = 0; i < 6; i++) pk::tileLabel(g, W, i, names[i], i == 0, i == 0 ? 0 : i == 1 ? 19 : i == 2 ? 20 : i == 3 ? 6 : i == 4 ? 9 : 10);
    ctx.screen = prs::SC_PICKER;
    save(dir, name("library"), g, scan(W, H, under, &ctx));
  }
  {  // details
    Shot g(W, H);
    pk::detailsScreen(g, W, H, "Moving and Pushing", "Starters  /  Built-in Starter", "Plays well on touch", 7, true);
    ctx.screen = prs::SC_DETAILS; ctx.pages = 7;
    save(dir, name("details"), g, scan(W, H, under, &ctx));
  }
  {  // settings: every tab, every page
    ctx.screen = prs::SC_SETTINGS;
    for (int t = 0; t < st::T_COUNT; t++) for (int p = 0; p < st::pages((st::Tab)t); p++) {
      Shot g(W, H);
      st::View v; v.tab = (st::Tab)t; v.page = p; v.last = st::I_DEAD;
      st::Values vals;
      st::CardInfo card; card.builtIn = 30;
      snprintf(card.state, sizeof card.state, "Card: 12 games"); snprintf(card.space, sizeof card.space, "Card: 14.2 GB free of 14.8 GB");
      snprintf(card.flash, sizeof card.flash, "Built-in: 380 KB free"); snprintf(card.board, sizeof card.board, "Board C fw 1a2b3c4");
      ctl::Need n; n.left = n.right = n.up = n.down = n.space = n.enter = n.jump = true;
      st::screen(g, v, vals, card, ctl::makeLayout(vals.s, n), n, 0, 0, 0);
      ctx.settings = v;
      save(dir, name(std::string("settings-") + st::tabName(t) + "-p" + std::to_string(p)), g, scan(W, H, under, &ctx));
    }
  }
  {  // search keyboard: its taps are in main.cpp's loop()
    Shot g(W, H);
    pk::drawKeyboard(g, W, H, 32, "TIN");
    pk::drawButton(g, 0, 0, 40, 32, "<");
#ifdef HIT_BEFORE
    pk::drawButton(g, 42, 0, W, 32, "All games");
#else
    pk::drawButton(g, 44, 0, W, 32, "All games");
#endif
    pk::drawSmallIcon(g, 0, 50, 8, true);
    Rects rs;
#ifdef HIT_BEFORE
    rs.push_back({0, 0, 40, 32}); rs.push_back({40, 0, W, 32});
    for (int k = 0; k < 4; k++) rs.push_back({k * W / 4, H - 36, (k + 1) * W / 4, H});
#else
    rs.push_back({0, 0, 40, pk::SEARCH_HEADER_HIT_H}); rs.push_back({44, 0, W, pk::SEARCH_HEADER_HIT_H});
    for (int k = 0; k < 4; k++) { int x0 = -1, x1 = -1; for (int x = 0; x < W; x++) if (pk::keyboardAction(W, x) == k) { if (x0 < 0) x0 = x; x1 = x + 1; } rs.push_back({x0, H - 36, x1, H}); }
#endif
    int top = 58, step = (H - top - 38) / 4;  // a key is the whole cell tx*9/W by (ty-top)/step
    for (int i = 0; i < 36; i++) rs.push_back({i % 9 * W / 9, top + i / 9 * step, (i % 9 + 1) * W / 9, top + (i / 9 + 1) * step});
    save(dir, name("search"), g, rs);
  }
  {  // pause menu
    Shot g(W, H);
    g.fillRect(0, 0, W, H, 0x2104);
    ovl::pauseMenu(g, "Moving and Pushing", 1, 0, -1);
    Rects rs;
    for (int i = 0; i <= ovl::PA_COUNT; i++) { int x0, y0, x1, y1; ovl::pauseCell(i, x0, y0, x1, y1); rs.push_back({x0, y0, x1, y1}); }
    save(dir, name("pause"), g, rs);
  }
  {  // load failure
    Shot g(W, H);
    pk::drawFailCard(g, W, H, "Moving and Pushing", pk::FAIL_FILE);
    struct U { int W, H; } u{W, H};
    auto look = [](void* p, int x, int y, prs::Rect& r) {
      U* q = (U*)p; if (!pk::inFailButton(q->W, q->H, x, y)) return false;
      int x0 = x, x1 = x + 1, y0 = y, y1 = y + 1;
      while (pk::inFailButton(q->W, q->H, x0 - 1, y)) x0--;
      while (pk::inFailButton(q->W, q->H, x1, y)) x1++;
      while (pk::inFailButton(q->W, q->H, x, y0 - 1)) y0--;
      while (pk::inFailButton(q->W, q->H, x, y1)) y1++;
      r = {x0, y0, x1, y1}; return true; };
    save(dir, name("failure"), g, scan(W, H, look, &u));
  }
  printf("wrote %s*-%s.png\n", w.c_str(), tag.c_str());
  return 0;
}
