// The USB file commands' parser, name rules, space check and CRC (src/serialfs.h).
//   c++ -std=c++17 -Wall -Wextra -I src host/serialfstest.cpp -o /tmp/serialfstest && /tmp/serialfstest
#include "serialfs.h"
#include "library.h"
#include <assert.h>
#include <stdio.h>
using namespace pk::sfs;
int main() {
  assert(gmpName("SNOWBOUN.GMP") && gmpName("A.GMP") && gmpName("A-B_9.GMP"));
  for (const char* bad : {"", ".GMP", "snowbound.GMP", "SNOWBOUND.GMP", "A.gmp", "A.GMPX", "A B.GMP", "../A.GMP", "/games/A.GMP", "A/B.GMP", "A.TXT", "ORDER.TXT", "A.B.GMP", "ÄB.GMP"}) assert(!gmpName(bad));
  assert(isOrder("ORDER.TXT") && !isOrder("order.txt") && !isOrder("ORDER.TXT2") && !isOrder("../ORDER.TXT"));
  assert(crcDone(crcUpdate(CRC_START, "123456789", 9)) == 0xCBF43926u);
  uint32_t c = CRC_START; c = crcUpdate(c, "1234", 4); c = crcUpdate(c, "56789", 5); assert(crcDone(c) == 0xCBF43926u);
  const char data[] = "hello pocket"; assert(crcDone(crcUpdate(CRC_START, data, sizeof data - 1)) == pk::crc32(data, sizeof data - 1));
  assert(parse("DISK LS").kind == LS && parse("DISK HELLO").kind == HELLO && parse("DISK RESCAN").kind == RESCAN && parse("DISK ABORT").kind == ABORT && parse("DISK END").kind == END);
  Cmd p = parse("DISK PUT BLASTER.GMP 1017234 deadBEEF");
  assert(p.kind == PUT && !strcmp(p.name, "BLASTER.GMP") && p.size == 1017234 && p.crc == 0xdeadbeefu);
  Cmd k = parse("DISK C 1024 0000ffff"); assert(k.kind == CHUNK && k.size == 1024 && k.crc == 0xffff);
  Cmd d = parse("DISK DEL A.GMP"); assert(d.kind == DEL && !strcmp(d.name, "A.GMP"));
  Cmd o = parse("DISK ORDER 5"); assert(o.kind == ORDER && o.count == 5);
  for (const char* bad : {"", "DISK", "DISKS LS", "DISK ", "DISK NOPE", "DISK LS now", "DISK PUT", "DISK PUT A.GMP", "DISK PUT A.GMP 0 00000000", "DISK PUT A.GMP 2097153 00000000", "DISK PUT A.GMP 99999999999 0",
                          "DISK PUT A.GMP 10 xyz", "DISK PUT A.GMP 10 123456789", "DISK PUT A.GMP 10 00 extra", "DISK PUT ORDER.TXT 10 00", "DISK PUT ../A.GMP 10 00", "DISK PUT a.gmp 10 00", "DISK C 0 00", "DISK C 1025 00", "DISK C 10",
                          "DISK C -5 00", "DISK DEL ORDER.TXT", "DISK DEL /games/A.GMP", "DISK DEL A.GMP B.GMP", "DISK ORDER 257", "DISK ORDER x", "DISK ORDER", "DISK PUT VERYLONGNAMEXX.GMP 10 00"})
    assert(parse(bad).kind == BAD);
  assert(roundUp(1) == 4096 && roundUp(4096) == 4096 && roundUp(4097) == 8192 && roundUp(0) == 0);
  assert(fits(100000, 3000000, 1000000) && !fits(100000, 3000000, 2950000));
  assert(!fits(0, 100, 0) && !fits(FILE_MAX + 1, 100000000, 0));
  assert(fits(4096, 12288, 0) && !fits(4096, 12287 - 4096, 0) && !fits(4096, 8192, 0));
  assert(!fits(10, 100, 500));
  // a replacement is checked as if the old copy stays: 100 KB free, a 120 KB file does not fit however big the one it replaces
  assert(!fits(120000, 3000000, 3000000 - 100000));
  assert(parse("DISK ORDER 256").kind == ORDER && parse("DISK ORDER 100").kind == ORDER);
  puts("serialfstest ok");
}
