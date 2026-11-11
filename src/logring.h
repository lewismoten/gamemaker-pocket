// Main-task-only RAM diagnostics. No heap, flash writes, or background-task hooks.
#pragma once
#include "serialfs.h"
#include <stdarg.h>
#include <stdio.h>
namespace logs {
constexpr unsigned CAPACITY = 3072, LINE_BYTES = 192, CHUNK = 64;
template<unsigned N = CAPACITY> struct Ring {
  static_assert(N >= 2, "ring needs space for a line and newline");
  uint8_t data[N] = {};
  unsigned head = 0, size = 0;
  uint8_t at(unsigned i) const { return data[(head + i) % N]; }
  void append(const char* text) {
    unsigned n = 0;
    constexpr unsigned limit = N < LINE_BYTES ? N - 1 : LINE_BYTES - 1;
    while (text[n] && n < limit) ++n;
    while (size + n + 1 > N) {
      uint8_t c;
      do { c = data[head]; head = (head + 1) % N; --size; } while (size && c != '\n');
    }
    for (unsigned i = 0; i < n; ++i) {
      char c = text[i];
      if (c == '\r' || c == '\n') c = ' ';
      if (i + 1 == n && text[n]) c = '~';
      data[(head + size++) % N] = (uint8_t)c;
    }
    data[(head + size++) % N] = '\n';
  }
  void event(unsigned long ms, const char* fmt, ...) {
    char line[LINE_BYTES];
    int prefix = snprintf(line, sizeof line, "%lu ", ms);
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(line + prefix, sizeof line - prefix, fmt, args);
    va_end(args);
    if (n >= (int)sizeof line - prefix) line[sizeof line - 2] = '~';
    append(line);
  }
};
// Bounded synchronous dump: callers must not append or service other commands until it returns.
// At most 3 KiB plus framing (~0.5 s at 115200). No host ACK or snapshot allocation.
template<class S, unsigned N> void dump(S& serial, const Ring<N>& ring) {
  serial.printf("LOGS BEGIN 1 %u\n", ring.size);
  uint32_t whole = pk::sfs::CRC_START;
  for (unsigned offset = 0; offset < ring.size;) {
    uint8_t bytes[CHUNK];
    unsigned n = ring.size - offset < CHUNK ? ring.size - offset : CHUNK;
    for (unsigned i = 0; i < n; ++i) bytes[i] = ring.at(offset + i);
    uint32_t part = pk::sfs::crcDone(pk::sfs::crcUpdate(pk::sfs::CRC_START, bytes, n));
    serial.printf("LOGS DATA %u %u %08lx\n", offset, n, (unsigned long)part);
    serial.write(bytes, n);
    serial.println();
    whole = pk::sfs::crcUpdate(whole, bytes, n);
    offset += n;
  }
  serial.printf("LOGS END %u %08lx\n", ring.size, (unsigned long)pk::sfs::crcDone(whole));
}
}  // namespace logs
