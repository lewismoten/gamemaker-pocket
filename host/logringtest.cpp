#include <cassert>
#include <cstdio>
#include <string>
#include "logring.h"
struct Serial {
  template<class... A> void printf(const char* fmt, A... a) { std::printf(fmt, a...); }
  void println() { std::printf("\r\n"); }
  void write(const uint8_t* p, unsigned n) { std::fwrite(p, 1, n, stdout); }
};
template<unsigned N> std::string contents(const logs::Ring<N>& r) {
  std::string out;
  for (unsigned i = 0; i < r.size; ++i) out += (char)r.at(i);
  return out;
}
int main(int argc, char**) {
  logs::Ring<12> small;
  small.append("one"); small.append("two"); small.append("tri");
  assert(contents(small) == "one\ntwo\ntri\n");
  small.append("four");
  assert(contents(small) == "tri\nfour\n");
  small.append("0123456789abcdef");
  assert(contents(small) == "0123456789~\n");
  small.append("\r\n"); assert(contents(small) == "  \n");
  small.append(""); assert(contents(small) == "  \n\n");
  logs::Ring<2> tiny;
  tiny.append("long"); assert(contents(tiny) == "~\n");
  logs::Ring<> ring;
  ring.event(123, "boot %s", "ok");
  assert(contents(ring) == "123 boot ok\n");
  ring.event(124, "%s", std::string(500, 'x').c_str());
  assert(ring.size == 12 + logs::LINE_BYTES);
  assert(ring.at(ring.size - 2) == '~' && ring.at(ring.size - 1) == '\n');
  // Compare thousands of evictions/wraps against a separate complete-line model.
  std::string model = contents(ring);
  for (unsigned i = 0; i < 4000; ++i) {
    std::string text = std::to_string(i) + std::string(i % 150, 'a') + '\n';
    while (model.size() + text.size() > logs::CAPACITY) model.erase(0, model.find('\n') + 1);
    model += text;
    ring.append(text.substr(0, text.size() - 1).c_str());
    assert(contents(ring) == model);
  }
  Serial serial;
  if (argc > 1) { logs::Ring<> empty; logs::dump(serial, empty); }
  else logs::dump(serial, ring);
}
