// The ten-slot list for Save game and Load game (saveslots.h has the layout and the names), drawn in the Pause menu's look: a title
// band with the Close X, then two columns of five rows, "1 name" or "1 empty". An empty slot is dimmed while loading (nothing to
// load); a slot waiting for its second tap says "Tap to overwrite" on yellow.
#pragma once
#include <string.h>
#include "closex.h"
#include "overlaydraw.h"
#include "saveslots.h"
#include "spaced.h"

namespace slotdraw {

template <class G> void row(G& g, int W, int H, const gm::SlotList& l, int i, bool saving, bool armed, bool hot = false) {
  int x0, y0, x1, y1;
  gm::slotRect(W, H, i, x0, y0, x1, y1);
  bool dim = !saving && !l.used[i];
  uint16_t fill = armed || hot ? ovl::C_YELLOW : ovl::C_DARK, ink = armed || hot ? ovl::C_BLACK : dim ? 0x8410 : ovl::C_WHITE;
  g.fillRoundRect(x0, y0, x1 - x0, y1 - y0, 6, fill);
  g.drawRoundRect(x0, y0, x1 - x0, y1 - y0, 6, dim ? 0x8410 : ovl::C_WHITE);
  int size = W <= 320 ? 1 : 2;
  char text[40], shown[40];
  const char* label = armed ? "Tap to overwrite" : gm::slotLabel(l, i);
  if (armed) snprintf(text, sizeof text, "%s", label);  // the words alone: "Tap to overwrite" is the longest line that has to fit
  else snprintf(text, sizeof text, "%d %s", i + 1, label);
  sp::ellipsize(text, shown, sizeof shown, x1 - x0 - 12, size);
  g.setTextSize(size);
  g.setTextColor(ink, fill);
  sp::print(g, x0 + 8, (y0 + y1) / 2 - 4 * size, shown, size);
}

template <class G> void screen(G& g, int W, int H, const gm::SlotList& l, bool saving, int armed = -1) {
  g.fillScreen(ovl::C_BLACK);
  g.setTextSize(2);
  g.setTextColor(ovl::C_YELLOW, ovl::C_BLACK);
  sp::print(g, 8, W <= 320 ? 12 : 18, saving ? "Save game" : "Load game", 2);
  cx::draw(g, W);
  for (int i = 0; i < gm::SLOTS; i++) row(g, W, H, l, i, saving, i == armed);
}

}  // namespace slotdraw
