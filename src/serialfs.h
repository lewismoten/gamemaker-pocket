// The board's USB file commands for /games (the flasher page's "Send games" tab): name rules, the command parser, the
// space check and an incremental CRC-32. Pure code with no board calls, so a Mac test runs it (host/serialfstest.cpp).
// Lines (115200 baud, all start with DISK): LS | PUT NAME.GMP size crc | C len crc (then len bytes) | END | DEL NAME.GMP |
// ORDER count (then count name lines) | RESCAN | ABORT. Only /games, only NAME.GMP (1-8 of A-Z 0-9 _ -, upper case) and ORDER.TXT.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace pk {
namespace sfs {
constexpr uint32_t BLOCK = 4096, CHUNK_MAX = 1024, FILE_MAX = 2u * 1024 * 1024, ORDER_MAX = 256;
inline bool nameChar(char c) { return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'; }
// NAME.GMP: 1 to 8 name characters, then .GMP, nothing else (no path, no lower case, no spaces).
inline bool gmpName(const char* s) {
  size_t n = strlen(s);
  if (n < 5 || n > 12 || strcmp(s + n - 4, ".GMP")) return false;
  for (size_t i = 0; i < n - 4; i++) if (!nameChar(s[i])) return false;
  return true;
}
inline bool isOrder(const char* s) { return !strcmp(s, "ORDER.TXT"); }
inline uint32_t crcUpdate(uint32_t c, const void* data, size_t n) {
  const uint8_t* p = (const uint8_t*)data;
  while (n--) { c ^= *p++; for (int i = 0; i < 8; i++) c = (c >> 1) ^ ((c & 1) ? 0xedb88320u : 0); }
  return c;
}
constexpr uint32_t CRC_START = 0xffffffffu;
inline uint32_t crcDone(uint32_t c) { return c ^ 0xffffffffu; }
inline uint32_t roundUp(uint32_t n) { return (n + BLOCK - 1) / BLOCK * BLOCK; }
// Does a file of `size` bytes fit? Keeps two blocks free for the file system. A file it replaces is not credited: the old copy
// stays until the new one is whole and renamed over it, so both are on the board for a while.
inline bool fits(uint32_t size, uint32_t total, uint32_t used) {
  if (size == 0 || size > FILE_MAX) return false;
  uint64_t free_ = (uint64_t)(total > used ? total - used : 0);
  return (uint64_t)roundUp(size) + 2 * BLOCK <= free_;
}
enum Kind { BAD, LS, PUT, CHUNK, END, DEL, ORDER, RESCAN, ABORT, HELLO };
struct Cmd { Kind kind = BAD; char name[16] = ""; uint32_t size = 0, crc = 0, count = 0; const char* why = ""; };
inline bool num(const char*& p, uint32_t& v, int base, size_t maxDigits) {
  v = 0; size_t d = 0;
  while (*p && *p != ' ') {
    char c = *p; int x = (c >= '0' && c <= '9') ? c - '0' : (base == 16 && c >= 'a' && c <= 'f') ? c - 'a' + 10 : (base == 16 && c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
    if (x < 0 || ++d > maxDigits) return false;
    v = v * base + x; p++;
  }
  return d > 0;
}
inline bool word(const char*& p, char* out, size_t cap) {
  size_t n = 0; while (*p && *p != ' ') { if (n + 1 >= cap) return false; out[n++] = *p++; } out[n] = 0; return n > 0;
}
inline void skip(const char*& p) { if (*p == ' ') p++; }
// Reads one "DISK ..." line. Anything malformed or outside the rules is BAD with a short reason.
inline Cmd parse(const char* line) {
  Cmd c;
  if (strncmp(line, "DISK", 4) || (line[4] && line[4] != ' ')) { c.why = "not a disk command"; return c; }
  const char* p = line + 4; skip(p);
  char verb[10];
  if (!word(p, verb, sizeof verb)) { c.why = "no verb"; return c; }
  skip(p);
  if (!strcmp(verb, "LS")) c.kind = LS;
  else if (!strcmp(verb, "HELLO")) c.kind = HELLO;
  else if (!strcmp(verb, "RESCAN")) c.kind = RESCAN;
  else if (!strcmp(verb, "ABORT")) c.kind = ABORT;
  else if (!strcmp(verb, "END")) c.kind = END;
  else if (!strcmp(verb, "DEL")) {
    if (!word(p, c.name, sizeof c.name) || !gmpName(c.name) || *p) { c.why = "bad name"; return c; }
    c.kind = DEL;
  } else if (!strcmp(verb, "PUT")) {
    if (!word(p, c.name, sizeof c.name) || !gmpName(c.name)) { c.why = "bad name"; return c; }
    skip(p);
    if (!num(p, c.size, 10, 8) || c.size == 0 || c.size > FILE_MAX) { c.why = "bad size"; return c; }
    skip(p);
    if (!num(p, c.crc, 16, 8) || *p) { c.why = "bad crc"; return c; }
    c.kind = PUT;
  } else if (!strcmp(verb, "C")) {
    if (!num(p, c.size, 10, 5) || c.size == 0 || c.size > CHUNK_MAX) { c.why = "bad length"; return c; }
    skip(p);
    if (!num(p, c.crc, 16, 8) || *p) { c.why = "bad crc"; return c; }
    c.kind = CHUNK;
  } else if (!strcmp(verb, "ORDER")) {
    if (!num(p, c.count, 10, 3) || c.count > ORDER_MAX || *p) { c.why = "bad count"; return c; }
    c.kind = ORDER;
  } else { c.why = "unknown verb"; return c; }
  if ((c.kind == LS || c.kind == HELLO || c.kind == RESCAN || c.kind == ABORT || c.kind == END) && *p) { c.kind = BAD; c.why = "extra text"; }
  return c;
}
}  // namespace sfs
}  // namespace pk
