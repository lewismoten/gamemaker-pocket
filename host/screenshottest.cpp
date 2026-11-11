// Emit the real firmware framer's bytes for the Python integration test.
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <initializer_list>
#include "screenshot.h"
struct Serial {
  template<class... A> void printf(const char* fmt, A... a) { std::printf(fmt, a...); }
  void println(const char* s = "") { std::printf("%s\r\n", s); }
  void write(const uint8_t* p, unsigned n) { std::fwrite(p, 1, n, stdout); }
};
// Synthetic physical RAM filled by an independent forward mapping of logical pixels.
struct Panel {
  int mode;
  void readRect(int x, int y, int w, int h, uint16_t* out) {
    for (int v = y; v < y + h; ++v) for (int u = x; u < x + w; ++u) {
      int lx = mode == 1 ? v : mode == 2 ? 319 - v : u;
      int ly = mode == 1 ? 239 - u : mode == 2 ? u : v;
      uint16_t value = (uint16_t)(lx + ly * 320);
      uint8_t bytes[2] = {(uint8_t)(value >> 8), (uint8_t)value};
      memcpy(out++, bytes, 2);
    }
  }
};
int main(int argc, char** argv) {
  for (int mode = 0; mode < 3; ++mode) {
    Panel panel{mode};
    for (unsigned y : {0u, 239u}) for (unsigned x : {0u, 288u}) {
      uint8_t out[64];
      shot::readPixels(panel, mode, x, y, 32, out);
      for (unsigned i = 0; i < 32; ++i)
        assert(((out[i * 2] << 8) | out[i * 2 + 1]) == ((x + i + y * 320) & 65535));
    }
  }
  if (argc != 3) return 1;
  Serial serial;
  shot::Transfer transfer;
  unsigned w = std::atoi(argv[1]), h = std::atoi(argv[2]);
  transfer.start(serial, w, h);
  while (transfer.active) {
    transfer.poll(serial, [](unsigned x, unsigned y, unsigned n, uint8_t* out) {
      for (unsigned i = 0; i < n; ++i) {
        uint16_t v = (uint16_t)((x + i) * 97 + y * 31);
        out[i * 2] = v >> 8; out[i * 2 + 1] = v;
      }
    });
    serial.println("K unrelated trace");
  }
}
