// Mac test and renders for the Settings screen (src/settingsdraw.h, through src/pressrect.h as the board taps it): every tab
// and page at both hands, the Controls preview lit by a real touch through src/controls.h, and Near pad + Raised.
// Checks: every button (Back, tabs, chips, More) is at least 36 x 36, on screen, apart from the others and found by the hit
// table; every label fits its button and every row name its column; every row of a tab is on one of its pages.
//   settingstest [out dir]      build with -lz; add -DPOCKET_CYD28 for the 320 x 240 board. PNGs: settings-<w>-<tab>-p<page>-<hand>.png
#include <stdio.h>
#include <string>
#include <vector>
namespace textdatum_t { constexpr int middle_center = 4, top_left = 0; }
#include "canvas.h"
#include "pressrect.h"
#include "settingsdraw.h"
#include "picturetune.h"

static int failures = 0;
static void fail(const std::string& m) { printf("FAIL  %s\n", m.c_str()); failures++; }
static bool overlap(const st::Box& a, const st::Box& b) { return a.x0 < b.x1 && b.x0 < a.x1 && a.y0 < b.y1 && b.y0 < a.y1; }

static ctl::Need demoNeed() {
  ctl::Need n;
  n.left = n.right = n.up = n.down = n.space = n.enter = true;
  n.jump = true;
  return n;
}
// The preview as main.cpp drives it: the controls model gets the touch scaled up to the game view.
struct Lit { uint8_t dirs = 0, btn = 0, held = 0; ctl::Disc pad = {0, 0, 0}; };
static Lit touchPreview(const ctl::Settings& s, int gx, int gy) {
  ctl::Controls c;
  c.configure(s, demoNeed());
  int id = 1, sz = 30;
  for (uint32_t t = 1000; t < 1200; t += 8) c.update(&id, &gx, &gy, &sz, 1, t);
  ctl::Change ch[16];
  int n = c.flush(ch, 16);
  Lit l;
  l.dirs = c.padDirs;
  l.pad = c.L.pad;  // the floating pad centers where the finger landed
  l.btn = c.btnDown;
  for (int i = 0; i < n; i++) {
    if (!ch[i].down) continue;
    switch (ch[i].scan) {
      case ctl::S_LEFT: l.held |= 1; break;
      case ctl::S_RIGHT: l.held |= 2; break;
      case ctl::S_UP: l.held |= 4; break;
      case ctl::S_DOWN: l.held |= 8; break;
      case ctl::S_SPACE: l.held |= 16; break;
      case ctl::S_ENTER: l.held |= 32; break;
    }
  }
  return l;
}

static void shot(const std::string& dir, const std::string& name, const st::View& v, const st::Values& vals, const Lit& lit = Lit()) {
  Canvas c(scr::W, scr::H);
  st::CardInfo card;
  card.builtIn = 30;
  snprintf(card.state, sizeof card.state, "Card: 12 games");
  snprintf(card.space, sizeof card.space, v.fromPause ? "Free space: see it from the game list" : "Card: 14.2 GB free of 14.8 GB");  // main.cpp's words
  snprintf(card.flash, sizeof card.flash, "Built-in: 380 KB free");
  snprintf(card.board, sizeof card.board, "Board C fw 1a2b3c4 2026-11-07");
  card.fromPause = v.fromPause;
  ctl::Layout L = ctl::makeLayout(vals.s, demoNeed());
  if (lit.pad.r) L.pad = lit.pad;
  st::screen(c, v, vals, card, L, demoNeed(), lit.dirs, lit.btn, lit.held);
  if (!dir.empty()) writePng(dir + "/" + name + ".png", c.W, c.H, rgbOf(c));
}

int main(int argc, char** argv) {
  std::string dir = argc > 1 ? argv[1] : "";
  const char* wn = scr::SMALL ? "320" : "480";
  int buttons = 0, shots = 0;
  // the bar
  std::vector<st::Box> bar;  // the tabs left to right, then the Close X at the right end
  for (int t = 0; t < st::T_COUNT; t++) {
    st::Box b = st::tabBox(t);
    if ((int)(strlen(st::tabName(t)) * 6 * st::G.tabSize) > b.x1 - b.x0 - 4) fail(std::string("tab label too wide: ") + st::tabName(t));
    bar.push_back(b);
  }
  bar.push_back(st::backBox());
  for (size_t i = 0; i < bar.size(); i++) {
    const st::Box& b = bar[i];
    if (b.x1 - b.x0 < 36 || b.y1 - b.y0 < 36 || b.x0 < 0 || b.x1 > scr::W || b.y1 > st::G.barH) fail("bar button small or off screen");
    for (size_t j = i + 1; j < bar.size(); j++) if (overlap(b, bar[j]) || (j == i + 1 && bar[j].x0 - b.x1 < 4)) fail("bar buttons need a four-pixel gap");
  }
  for (int t = 0; t < st::T_COUNT; t++) {
    st::Item all[16];
    int n = st::tabItems((st::Tab)t, all), seen = 0;
    for (int page = 0; page < st::pages((st::Tab)t); page++) {
      for (int pause = 0; pause < 2; pause++) {
        st::View v;
        v.tab = (st::Tab)t;
        v.page = page;
        v.fromPause = pause;
        prs::Ctx ctx;
        ctx.screen = prs::SC_SETTINGS;
        ctx.W = scr::W;
        ctx.H = scr::H;
        ctx.settings = v;
        std::vector<st::Box> boxes = bar;
        if (t == st::T_CONTROLS && st::PREVIEW) {
          st::Box p = st::previewBox();
          if (p.x1 > scr::W || p.y1 > st::G.moreY0) fail("preview off screen or over More");
          prs::Rect r;
          if (prs::under(ctx, (p.x0 + p.x1) / 2, (p.y0 + p.y1) / 2, r)) fail("the preview counts as a button");
          if (st::hit(v, (p.x0 + p.x1) / 2, (p.y0 + p.y1) / 2).kind != st::K_PREVIEW) fail("the preview is not found");
          boxes.push_back(p);
        }
        if (t == st::T_CONTROLS && !st::PREVIEW) {  // the Preview button opens the Preview page; it is 36 px and apart from More
          st::Box pb = st::previewBtn();
          if (pb.x1 - pb.x0 < 36 || pb.y1 - pb.y0 < 36 || pb.y1 > scr::H) fail("Preview button is small or off screen");
          if (st::hit(v, (pb.x0 + pb.x1) / 2, (pb.y0 + pb.y1) / 2).kind != st::K_PVOPEN) fail("Preview button is not found");
          if (overlap(pb, st::moreBox())) fail("Preview button overlaps More");
          boxes.push_back(pb);
        }
        if (t == st::T_CONTROLS && st::pages(v.tab) > 1) {
          st::Box m = st::moreBox();
          if (m.x1 - m.x0 < 36 || m.y1 - m.y0 < 36 || m.y1 > scr::H) fail("More is small or off screen");
          st::Box nb = st::nextBox(), pv = st::prevBox();
          if (nb.x1 - nb.x0 < 36 || pv.x1 - pv.x0 < 36 || nb.y1 - nb.y0 < (scr::SMALL ? 36 : 40)) fail("Prev or Next is small");
          st::View first = v; first.page = 0;
          if (st::hit(first, (nb.x0 + nb.x1) / 2, (nb.y0 + nb.y1) / 2).kind != st::K_MORE) fail("Next is not found");
          if (st::hit(first, (pv.x0 + pv.x1) / 2, (pv.y0 + pv.y1) / 2).kind != st::K_NONE) fail("Prev works on the first page");
          st::View last = v; last.page = st::pages(v.tab) - 1;
          if (st::hit(last, (nb.x0 + nb.x1) / 2, (nb.y0 + nb.y1) / 2).kind != st::K_NONE) fail("Next works on the last page");
          if (st::hit(last, (pv.x0 + pv.x1) / 2, (pv.y0 + pv.y1) / 2).kind != st::K_PREV) fail("Prev is not found");
          boxes.push_back(pv);
          boxes.push_back(nb);
        }
        st::Item it[16];
        int k = st::pageItems(v, it);
        if (!pause) seen += k;
        for (int i = 0; i < k; i++) {
          st::RowBox rb = st::rowBox(v.tab, i);
          int nameW = (int)(strlen(st::info(it[i]).name) * 6 * st::G.nameSize);
          int room = st::stacked(v.tab) ? st::colX1(v.tab) - rb.nameX : rb.chips.x0 - 4 - rb.nameX;
          if (nameW > room) fail(std::string("row name too wide: ") + st::info(it[i]).name);
          int c = st::chipCount(it[i]);
          for (int j = 0; j < c; j++) {
            st::Box b = st::chipBox(rb.chips, c, j);
            if (j && b.x0 - st::chipBox(rb.chips, c, j-1).x1 < 4) fail("chips need a four-pixel gap");
            char lab[24];
            st::chipLabel(it[i], j, lab, sizeof lab);
            std::string nm = std::string(st::info(it[i]).name) + " / " + lab;
            if (b.x1 - b.x0 < 36 || b.y1 - b.y0 < 36) fail("chip under 36 x 36: " + nm);
            if (b.x0 < 0 || b.x1 > scr::W || b.y1 > scr::H) fail("chip off screen: " + nm);
            if ((int)strlen(lab) * 6 > b.x1 - b.x0 - 6) fail("chip label does not fit: " + nm);
            prs::Rect r;
            bool live = !(it[i] == st::I_RESCAN && pause);
            if (prs::under(ctx, (b.x0 + b.x1) / 2, (b.y0 + b.y1) / 2, r) != live) fail("hit table misses " + nm);
            st::Hit h = st::hit(v, (b.x0 + b.x1) / 2, (b.y0 + b.y1) / 2);
            if (live && (h.kind != st::K_CHIP || h.item != it[i] || h.chip != j)) fail("hit finds the wrong chip for " + nm);
            if (c == 1) {
              st::Hit label = st::hit(v, 6, (b.y0+b.y1)/2);
              if (live ? (label.kind != st::K_CHIP || label.item != it[i] || label.box.x0 != 4) : label.kind != st::K_NONE)
                fail("single-action row label / paused Rescan hit mismatch");
            }
            for (auto& o : boxes) if (overlap(o, b)) fail("chip overlaps another button: " + nm);
            boxes.push_back(b);
            buttons++;
          }
        }
        if (pause) continue;
        for (int hand = 0; hand < 2; hand++) {
          st::Values vals;
          vals.s.lefty = hand;
          char name[64];
          snprintf(name, sizeof name, "settings-%s-%s-p%d-%s", wn, st::tabName(t), page + 1, hand ? "left" : "right");
          shot(dir, name, v, vals);
          shots++;
        }
      }
    }
    if (seen != n) fail(std::string("not every row of ") + st::tabName(t) + " is on a page");
  }
  // The Controls preview lit by a touch on the pad's upper right (Right + Jump on the 2.8 inch board's arc), the Card tab from a
  // paused game, and Near pad + Raised with the finger on Jump.
  {
    st::Values vals;
    st::View v;
    ctl::Layout L = ctl::makeLayout(vals.s, demoNeed());
    v.last = st::I_DEAD;
    v.preview = !st::PREVIEW;  // 2.8 inch: the pad is on the Preview page
    if (!st::PREVIEW) {
      st::Box p = st::previewBox(), ob = st::optionsBtn();
      st::View o = v;
      o.preview = false;
      if (st::hit(o, (st::previewBtn().x0 + st::previewBtn().x1) / 2, st::previewBtn().y0 + 5).kind != st::K_PVOPEN) fail("Controls has no reachable Preview");
      if (p.x0 < 0 || p.x1 > scr::W || p.y0 <= st::G.barH || p.y1 > st::G.moreY0 - 20) fail("Preview page: pad off screen or over the buttons");
      if (ob.x1 - ob.x0 < 36 || ob.y1 - ob.y0 < 36 || ob.y1 > scr::H) fail("Back to options is small or off screen");
      if (st::hit(v, (ob.x0 + ob.x1) / 2, (ob.y0 + ob.y1) / 2).kind != st::K_PVBACK) fail("Back to options is not found");
      if (st::hit(v, (p.x0 + p.x1) / 2, (p.y0 + p.y1) / 2).kind != st::K_PREVIEW) fail("Preview page: the pad is not found");
      if (st::pageItems(v, nullptr) != 0) fail("Preview page still lists rows");
      if (overlap(p, ob) || overlap(p, st::backBox())) fail("Preview page overlaps");
      Canvas c(scr::W, scr::H);
      st::Values pv;
      pv.s.lefty = 1;
      st::screen(c, v, pv, st::CardInfo(), ctl::makeLayout(pv.s, demoNeed()), demoNeed(), 0, 0, 0);
      st::View v0 = v;
      v0.preview = false;
      Canvas c0(scr::W, scr::H);
      st::screen(c0, v0, pv, st::CardInfo(), ctl::makeLayout(pv.s, demoNeed()), demoNeed(), 0, 0, 0);
      if (c.px == c0.px) fail("Preview page draws the same as the options");
      shot(dir, "settings-320-Controls-preview", v, vals);
    }
    shot(dir, std::string("settings-") + wn + "-Controls-hint", v, vals);
    Lit lit = touchPreview(vals.s, L.pad.cx + L.pad.r * 45 / 100, L.pad.cy - L.pad.r * 45 / 100);
    if (!lit.held) fail("a touch on the preview's pad pressed nothing");
    shot(dir, std::string("settings-") + wn + "-Controls-touch", v, vals, lit);
    vals.s.jumpNear = vals.s.raised = 1;
    v.last = st::I_JUMP;
    L = ctl::makeLayout(vals.s, demoNeed());
    Lit j = touchPreview(vals.s, L.space.cx, L.space.cy);
    if (!(j.held & 16)) fail("a touch on the preview's Jump near the pad pressed no Space");
    shot(dir, std::string("settings-") + wn + "-Controls-nearpad-raised", v, vals, j);
    {  // Pad position: Float, before a touch (the dotted place a thumb may land) and with a thumb landed low in it, sliding up
      st::Values fv;
      fv.s.floatPad = 1;
      st::View fview;
      fview.last = st::I_POS;
      if (st::value(st::I_POS, fv) != 2) fail("Float is not the third Pad position chip");
      shot(dir, std::string("settings-") + wn + "-Controls-float", fview, fv);
      ctl::Layout fl = ctl::makeLayout(fv.s, demoNeed());
      int zx = (fl.zone.x0 + fl.zone.x1) / 2, zy = scr::VIEW_H * 2 / 3;
      ctl::Controls fc;
      fc.configure(fv.s, demoNeed());
      int id = 1, sz = 30, x = zx, y = zy;
      for (uint32_t t = 1000; t < 1100; t += 8) fc.update(&id, &x, &y, &sz, 1, t);
      if (fc.L.pad.cx != zx || fc.L.pad.cy != zy) fail("the preview's floating pad did not center on the thumb");
      y -= fl.pad.r / 2;
      for (uint32_t t = 1100; t < 1300; t += 8) fc.update(&id, &x, &y, &sz, 1, t);
      Lit fl2;
      fl2.dirs = fc.padDirs; fl2.btn = fc.btnDown; fl2.held = (fc.padDirs & 4) ? 4 : 0; fl2.pad = fc.L.pad;
      if (!fl2.held) fail("a slide up from where the floating pad landed pressed no Up");
      shot(dir, std::string("settings-") + wn + "-Controls-float-touch", fview, fv, fl2);
      st::Values back = fv;
      st::choose(st::I_POS, 1, back);
      if (back.s.floatPad || !back.s.raised) fail("choosing Raised after Float did not give a fixed raised pad");
      shots += 2;
    }
    v = st::View();
    v.tab = st::T_CARD;
    v.fromPause = true;
    shot(dir, std::string("settings-") + wn + "-Card-paused", v, st::Values());
    shots += 4;
  }
#ifdef POCKET_CYD28
  {  // Settings, Display, Panel, Picture (picturetune.h): chips 36 x 36 or more, apart, found by the hit table; the commands
    std::vector<pt::Box> all = {pt::backBox()};
    for (int r = 0; r < 2; r++) for (int k = 0; k < 4; k++) all.push_back(pt::chipBox(r, k));
    for (size_t i = 0; i < all.size(); i++) {
      const pt::Box& b = all[i];
      buttons++;
      if (i && (i-1)%4 && b.x0-all[i-1].x1<4) fail("picture: chips need a four-pixel gap");
      if (b.x1 - b.x0 < 36 || b.y1 - b.y0 < 36) fail("picture: a button under 36 x 36");
      if (b.x0 < 0 || b.y0 < 0 || b.x1 > scr::W || b.y1 > scr::H) fail("picture: a button off screen");
      for (size_t j = 0; j < i; j++) if (all[j].x0 < b.x1 && b.x0 < all[j].x1 && all[j].y0 < b.y1 && b.y0 < all[j].y1) fail("picture: buttons overlap");
      pt::Hit h = pt::hit((b.x0 + b.x1) / 2, (b.y0 + b.y1) / 2);
      pt::Kind want = i == 0 ? pt::K_BACK : i <= 4 ? pt::K_GAMMA : pt::K_TUNE;
      if (h.kind != want || (i && h.k != (int)((i - 1) % 4))) fail("picture: the hit table misses a button");
    }
    if (pt::cardBox().y1 > pt::rowY(0) || pt::cardBox().y0 < pt::BAR_H) fail("picture: the test card overlaps the bar or the rows");
    if (!pt::isDefault(0, 0) || pt::isDefault(1, 0) || pt::isDefault(0, 2)) fail("picture: isDefault");
    for (int d = 0; d < 2; d++)
      for (int t = 0; t < pt::TUNES; t++)
        for (int gm = 0; gm < pt::GAMMAS; gm++) {
          uint8_t b[80];
          int n = pt::commands(d != 0, gm, t, b), at = 0, gamSeen = 0;
          while (at < n && b[at] != 0xFF) {
            if (b[at] == 0xE0 || b[at] == 0xE1) { if (b[at + 1] != (d ? 14 : 15)) fail("picture: a gamma table of the wrong length for its driver"); }
            if (b[at] == 0x26) { gamSeen = 1; if (b[at + 2] != (1 << gm)) fail("picture: GAMSET value"); }
            at += 2 + b[at + 1];
          }
          if (at != n - 1 || !gamSeen) fail("picture: a command list does not end where it should or has no GAMSET");
        }
    static const char* const drv[2] = {"ILI9341", "ST7789"};
    for (int gm : {0, 2})
      for (int t : {0, 1}) {
        Canvas c(scr::W, scr::H);
        pt::screen(c, gm, t, drv[1]);
        char name[64];
        snprintf(name, sizeof name, "picture-%s-g%d-t%d", wn, gm + 1, t);
        if (!dir.empty()) writePng(dir + "/" + name + ".png", c.W, c.H, rgbOf(c));
        shots++;
      }
  }
#endif
  printf("settingstest (%s): %s; %d buttons checked, %d renders%s%s\n", wn, failures ? "FAILURES" : "passed", buttons, shots, dir.empty() ? "" : " in ", dir.c_str());
  return failures ? 1 : 0;
}
