// Rolling panel readback, no framebuffer. One <=32-pixel chunk per poll (~9 ms at 115200).
#pragma once
#include "serialfs.h"
#include "softgfx.h"
namespace shot {
constexpr unsigned PIXELS = 32;
// Read a logical horizontal span, including CYD software landscape modes.
// Panel words are already in big endian order (LovyanGFX swapBytes=false).
template<class Panel> void readPixels(Panel& panel, int mode, unsigned x, unsigned y, unsigned n, uint8_t* out) {
  uint16_t pixels[PIXELS];
  sg::Xf xf; xf.mode = mode;
  int px, py, pw, ph;
  xf.rect(x, y, n, 1, px, py, pw, ph);
  panel.readRect(px, py, pw, ph, pixels);
  for (unsigned i = 0; i < n; ++i) {
    unsigned j = mode == 2 ? n - 1 - i : i;
    memcpy(out + i * 2, pixels + j, 2);
  }
}
struct Transfer {
  unsigned width = 0, height = 0, offset = 0;
  uint32_t whole = pk::sfs::CRC_START;
  bool active = false;
  template<class S> void start(S& serial, unsigned w, unsigned h) {
    if (active) { serial.println("SHOT ERR busy"); return; }
    width = w; height = h; offset = 0; whole = pk::sfs::CRC_START; active = true;
    serial.printf("SHOT BEGIN 1 %u %u RGB565BE\n", w, h);
  }
  template<class S, class Read> void poll(S& serial, Read read) {
    if (!active) return;
    uint8_t data[PIXELS * 2];
    unsigned x = offset % width, y = offset / width;
    unsigned n = width - x < PIXELS ? width - x : PIXELS;
    read(x, y, n, data);
    uint32_t crc = pk::sfs::crcDone(pk::sfs::crcUpdate(pk::sfs::CRC_START, data, n * 2));
    serial.printf("SHOT DATA %u %u %08lx\n", offset * 2, n * 2, (unsigned long)crc);
    serial.write(data, n * 2);
    serial.println();
    whole = pk::sfs::crcUpdate(whole, data, n * 2);
    offset += n;
    if (offset == width * height) {
      serial.printf("SHOT END %u %08lx\n", offset * 2, (unsigned long)pk::sfs::crcDone(whole));
      active = false;
    }
  }
};
}  // namespace shot
