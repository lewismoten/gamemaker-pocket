// Included by uploadtest.py around exact main.cpp routines. No duplicate upload/read implementation.
#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include "gmsmall.h"
#include "serialfs.h"
static std::vector<std::string> events;
struct Node { std::vector<uint8_t> bytes; bool shortRead = false, shortWrite = false; int opened = 0, payloadReads = 0; };
struct File {
  std::shared_ptr<Node> node;
  size_t pos = 0;
  std::string path;
  explicit operator bool() const { return bool(node); }
  size_t size() const { return node->bytes.size(); }
  bool seek(size_t off) { pos = off; return off <= size(); }
  size_t read(uint8_t* out, size_t n) {
    events.push_back("read:" + path);
    n = std::min(n, size() - pos);
    bool payload = path != "/small.pak" || pos >= 48;
    if (payload) ++node->payloadReads;
    if (node->shortRead && payload && n) --n;
    if (n) memcpy(out, node->bytes.data() + pos, n);
    pos += n; return n;
  }
  size_t write(const uint8_t* in, size_t n) {
    events.push_back("write:" + path);
    if (node->shortWrite && n) --n;
    node->bytes.insert(node->bytes.end(), in, in + n); return n;
  }
  void close() {
    if (node) { assert(node->opened > 0); --node->opened; events.push_back("close:" + path); node.reset(); }
  }
};
namespace fs {
struct FS {
  std::map<std::string, std::shared_ptr<Node>> nodes;
  std::string failRemove;
  bool mounted = true, failMount = false, failOpen = false, failRename = false;
  uint32_t total = 65536;
  bool begin(bool format) { assert(!format); events.push_back("mount"); mounted = !failMount; return mounted; }
  bool exists(const char* p) { assert(mounted); events.push_back(std::string("exists:") + p); return nodes.count(p); }
  bool remove(const char* p) {
    assert(mounted); events.push_back(std::string("remove:") + p);
    if (failRemove == p) return false;
    auto i = nodes.find(p);
    if (i == nodes.end()) return false;
    assert(!i->second->opened); nodes.erase(i); return true;
  }
  bool mkdir(const char*) { assert(mounted); return true; }
  uint32_t totalBytes() { assert(mounted); return total; }
  uint32_t usedBytes() {
    assert(mounted); events.push_back("used"); size_t n = 0;
    for (const auto& e : nodes) n += e.second->bytes.size();
    return uint32_t(n);
  }
  File open(const char* p, const char* mode) {
    assert(mounted); events.push_back(std::string("open:") + p);
    if (failOpen) return {};
    if (!strcmp(mode, "w")) { nodes[p] = std::make_shared<Node>(); }
    auto i = nodes.find(p);
    if (i == nodes.end()) return {};
    ++i->second->opened; return {i->second, 0, p};
  }
  bool rename(const char* a, const char* b) {
    assert(mounted); events.push_back(std::string("rename:") + b);
    if (failRename) return false;
    assert(nodes.count(a) && !nodes[a]->opened);
    nodes[b] = nodes[a]; nodes.erase(a); return true;
  }
  void put(const char* p, size_t n, uint8_t value = 0x42) { auto v = std::make_shared<Node>(); v->bytes.assign(n, value); nodes[p] = v; }
  void closed() const { for (const auto& e : nodes) assert(!e.second->opened); }
};
}
static fs::FS LittleFS, SD;
static bool sdUp = false, mountFail = false;
static bool mountSD() { events.push_back("sd-mount"); if (mountFail) return false; sdUp = SD.mounted = true; return true; }
static void unmountSD() { events.push_back("sd-unmount"); sdUp = SD.mounted = false; }
static uint32_t clockMs = 0;
static uint32_t millis() { return clockMs; }
static void delay(int n) { if (n == 1000) throw std::runtime_error("boot stopped"); clockMs += uint32_t(n); }
static void message(const char*) { events.push_back("mount-error"); }
// Only unrelated board/settings/resume/UI boundaries are stubs. The mount-to-list
// call sequence itself is copied verbatim from setup(), not reconstructed here.
static void loadSettings() { events.push_back("settings"); }
static void applyBacklight() {}
static bool ledOn = false;
static struct { bool enabled = false; } statusLed;
static struct { int sound = 0; } settings;
static void resumeLoad() {}
namespace spk { static void setVolume(int) {} }
namespace scr { constexpr int W = 320, H = 240; }
static int tft;
namespace pk { static void splashLine(int, int, int, const char*) {} }
static void loadGameList() { assert(LittleFS.mounted); events.push_back("list"); LittleFS.usedBytes(); }
struct MockSerial {
  std::vector<uint8_t> input;
  size_t pos = 0;
  std::string output;
  int read() { if (pos == input.size()) return -1; return input[pos++]; }
  int read(uint8_t* out, size_t n) { n = std::min(n, input.size() - pos); if (n) memcpy(out, input.data() + pos, n); pos += n; return int(n); }
  void println(const char* s) { output += std::string(s) + "\n"; }
  void printf(const char* fmt, ...) { char b[256]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap); output += b; }
  void append(const std::string& s) { input.insert(input.end(), s.begin(), s.end()); }
} Serial;
struct GameEntry {};
static bool validGmp = true;
static bool gmpEntry(File&, const char*, bool, GameEntry&) {
  assert(LittleFS.nodes.at("/games/A.GMP")->bytes == std::vector<uint8_t>(4096, 0x19));
  events.push_back("gmp-validation"); return validGmp;
}
static size_t allocations = 0, largest = 0;
static bool failAlloc = false;
static void* testMalloc(size_t n) { ++allocations; largest = std::max(largest, n); return failAlloc ? nullptr : malloc(n); }
#define malloc testMalloc
// PRODUCTION_ROUTINES
#undef malloc
static void reset() {
  LittleFS = {}; SD = {}; SD.mounted = false; sdUp = mountFail = false;
  Serial = {}; events.clear(); clockMs = 0; allocations = largest = 0; failAlloc = false; validGmp = true;
}
static size_t at(const std::string& event) { auto i = std::find(events.begin(), events.end(), event); assert(i != events.end()); return size_t(i - events.begin()); }
static bool has(const std::string& event) { return std::find(events.begin(), events.end(), event) != events.end(); }
static void protectedFiles() {
  LittleFS.put("/games/A.GMP", 4096, 0x19); LittleFS.put("/games/ORDER.TXT", 16, 0x29);
  LittleFS.put("/games/KEEP.TMP", 10, 0x39);
}
static void preserved() {
  assert(LittleFS.nodes.at("/games/A.GMP")->bytes == std::vector<uint8_t>(4096, 0x19));
  assert(LittleFS.nodes.at("/games/ORDER.TXT")->bytes == std::vector<uint8_t>(16, 0x29));
  assert(LittleFS.nodes.at("/games/KEEP.TMP")->bytes == std::vector<uint8_t>(10, 0x39));
  LittleFS.closed();
}
static pk::sfs::Cmd putCommand(uint32_t n = 4096) {
  pk::sfs::Cmd c; c.kind = pk::sfs::PUT; strcpy(c.name, "A.GMP"); c.size = n; return c;
}
static void cleanupTests() {
  reset(); protectedFiles(); LittleFS.put("/games/UPLOAD.TMP", 50000); LittleFS.put("/games/ORDER.NEW", 4000);
  // Admission is possible ONLY after orphan removal; timeout cleans the newly opened temp.
  diskPut(putCommand());
  assert(Serial.output.find("DISK GO\n") != std::string::npos);
  assert(at("remove:/games/UPLOAD.TMP") < at("used"));
  assert(at("remove:/games/ORDER.NEW") < at("used"));
  assert(!LittleFS.nodes.count("/games/UPLOAD.TMP") && !LittleFS.nodes.count("/games/ORDER.NEW")); preserved();
  reset(); protectedFiles(); LittleFS.put("/games/UPLOAD.TMP", 100); LittleFS.put("/games/ORDER.NEW", 100);
  LittleFS.mounted = false; bootMount();
  assert(at("mount") < at("remove:/games/UPLOAD.TMP"));
  assert(at("remove:/games/UPLOAD.TMP") < at("list") && at("remove:/games/ORDER.NEW") < at("list")); preserved();
  assert(at("remove:/games/ORDER.NEW") < at("settings") && at("settings") < at("list"));
  reset(); LittleFS.failMount = true;
  try { bootMount(); assert(false); } catch (const std::runtime_error&) {}
  assert(!has("remove:/games/UPLOAD.TMP") && !has("list"));
  for (const char* blocked : {"/games/UPLOAD.TMP", "/games/ORDER.NEW"}) {
    reset(); protectedFiles(); LittleFS.put("/games/UPLOAD.TMP", 100); LittleFS.put("/games/ORDER.NEW", 100); LittleFS.failRemove = blocked;
    auto oldTemp = LittleFS.nodes.at(blocked);
    diskPut(putCommand());
    assert(Serial.output.find("DISK ERR cleanup\n") != std::string::npos);
    assert(!has("used") && !has("open:/games/UPLOAD.TMP"));
    assert(LittleFS.nodes.at(blocked) == oldTemp); preserved();
    assert(has("remove:/games/UPLOAD.TMP") && has("remove:/games/ORDER.NEW"));
  }
  reset(); protectedFiles(); LittleFS.put("/games/UPLOAD.TMP", 100); LittleFS.failRemove = "/games/UPLOAD.TMP";
  bootMount(); assert(Serial.output.find("DISK ERR cleanup\n") != std::string::npos); assert(has("list")); preserved();
  reset(); protectedFiles(); LittleFS.total = 8192; diskPut(putCommand());
  assert(Serial.output.find("DISK ERR space") != std::string::npos); assert(!has("open:/games/UPLOAD.TMP")); preserved();
  puts("PASS PKU-01 actual diskPut admission/usedBytes after both orphan removals, boot mount/cleanup/list order, cleanup/mount failure, committed preservation");
}
static void readTests();
static void crcTests();
int main(int argc, char** argv) {
  (void)readFileIn;
  assert(argc == 2); std::string c = argv[1];
  if (c == "cleanup" || c == "all") cleanupTests();
  if (c == "read" || c == "all") readTests();
  if (c == "crc" || c == "all") crcTests();
}
static void packed(size_t len) {
  LittleFS.put("/small.pak", len + 48, 0x5a);
  auto& bytes = LittleFS.nodes.at("/small.pak")->bytes;
  std::fill(bytes.begin(), bytes.begin() + 48, 0);
  memcpy(bytes.data(), "GMS1", 4); bytes[4] = 1;
  memcpy(bytes.data() + 8, "asset", 6); bytes[40] = 48;
  for (int i = 0; i < 4; ++i) bytes[44 + i] = uint8_t(len >> (8 * i));
}
static void readTests() {
  constexpr size_t cap = 1024 * 1024;
  // Real SmallPak metadata parser plus the real readFileIn. Oversize metadata reads
  // are allowed; payload reads and allocations must both be zero.
  for (int path = 0; path < 4; ++path) for (size_t len : {size_t(0), cap, cap + 1}) for (int fault = 0; fault < 3; ++fault) {
    reset(); bool pak = path == 0, card = path >= 2, wasUp = path == 3;
    fs::FS& fsys = card ? SD : LittleFS;
    if (wasUp) sdUp = SD.mounted = true;
    if (pak) packed(len); else fsys.put("/folder/asset", len, 0x5a);
    auto node = fsys.nodes.at(pak ? "/small.pak" : "/folder/asset");
    failAlloc = fault == 1; node->shortRead = fault == 2;
    size_t n = 123;
    uint8_t* p = readFileIn(fsys, pak ? "" : "/folder", "asset", &n);
    bool success = len <= cap && !failAlloc && !(fault == 2 && len);
    assert(bool(p) == success);
    if (success) { assert(n == len); for (size_t i = 0; i < n; ++i) assert(p[i] == 0x5a); free(p); }
    else assert(n == 123);
    assert(allocations == (len <= cap ? 1u : 0u));
    assert(largest == (len <= cap ? len + 1 : 0));
    assert(node->payloadReads == (len <= cap && !failAlloc ? 1 : 0));
    fsys.closed();
    assert(sdUp == wasUp && SD.mounted == wasUp);
    assert(has("sd-unmount") == (card && !wasUp));
  }
  reset(); packed(cap + 1); LittleFS.put("/asset", 2);
  size_t n = 123; assert(!readFileIn(LittleFS, "", "asset", &n));
  assert(!has("open:/asset") && allocations == 0); LittleFS.closed();
  reset(); packed(3); LittleFS.put("/missing", 2);
  uint8_t* p = readFileIn(LittleFS, "", "missing", &n); assert(p && n == 2); free(p); LittleFS.closed();
  reset(); mountFail = true; n = 123;
  assert(!readFileIn(SD, "/folder", "asset", &n)); assert(!sdUp && allocations == 0 && n == 123);
  reset(); SD.failOpen = true;
  assert(!readFileIn(SD, "/folder", "asset", &n)); assert(!sdUp && has("sd-unmount") && allocations == 0);
  puts("PASS PKU-03 actual readFileIn: small.pak/plain/card/pre-mounted, zero/exact 1 MiB/cap+1, allocation/short-read/open/mount failure, close/unmount, zero oversize allocation/payload read");
}
static uint32_t checksum(const std::string& s) {
  return pk::sfs::crcDone(pk::sfs::crcUpdate(pk::sfs::CRC_START, s.data(), s.size()));
}
static void chunk(const std::string& payload, bool valid) {
  char line[64]; snprintf(line, sizeof line, "DISK C %zu %08x\n", payload.size(), checksum(payload) ^ (valid ? 0u : 1u));
  Serial.append(line); Serial.append(payload);
}
static size_t occurrences(const std::string& word) {
  size_t n = 0, pos = 0;
  while ((pos = Serial.output.find(word, pos)) != std::string::npos) { ++n; pos += word.size(); }
  return n;
}
static void crcTests() {
  // Seven failures still allow a valid replacement; old destination stays live until rename.
  reset(); protectedFiles(); auto old = LittleFS.nodes.at("/games/A.GMP");
  for (int i = 0; i < 7; ++i) chunk("a", false);
  chunk("a", true); auto c = putCommand(1); c.crc = checksum("a"); diskPut(c);
  assert(occurrences("DISK ERR crc\n") == 7 && occurrences("DISK DONE A.GMP 1\n") == 1);
  assert(LittleFS.nodes.at("/games/A.GMP")->bytes == std::vector<uint8_t>{'a'});
  assert(old->bytes == std::vector<uint8_t>(4096, 0x19));
  assert(at("close:/games/UPLOAD.TMP") < at("rename:/games/A.GMP")); LittleFS.closed();
  // Eight failures abort immediately, without needing the existing inactivity timeout.
  reset(); protectedFiles();
  for (int i = 0; i < 8; ++i) chunk("a", false);
  c = putCommand(1); c.crc = checksum("a"); diskPut(c);
  assert(occurrences("DISK ERR crc\n") == 8);
  assert(Serial.output.find("timeout") == std::string::npos && clockMs < 8000);
  assert(!LittleFS.nodes.count("/games/UPLOAD.TMP") && !has("rename:/games/A.GMP")); preserved();
  // Valid chunks reset the consecutive budget: more than eight total bad chunks is OK.
  reset(); protectedFiles();
  for (int part = 0; part < 3; ++part) {
    for (int i = 0; i < 7; ++i) chunk("a", false);
    chunk("a", true);
  }
  c = putCommand(3); c.crc = checksum("aaa"); diskPut(c);
  assert(occurrences("DISK ERR crc\n") == 21 && occurrences("DISK DONE A.GMP 3\n") == 1);
  assert(LittleFS.nodes.at("/games/A.GMP")->bytes == std::vector<uint8_t>(3, 'a')); LittleFS.closed();
  // A partial valid upload followed by eight failures must discard the partial bytes.
  reset(); protectedFiles(); chunk("a", true);
  for (int i = 0; i < 8; ++i) chunk("b", false);
  chunk("b", true); c = putCommand(2); c.crc = checksum("ab"); diskPut(c);
  assert(!LittleFS.nodes.count("/games/UPLOAD.TMP") && !has("rename:/games/A.GMP"));
  assert(occurrences("DISK OK 1\n") == 1 && occurrences("DISK OK 2\n") == 0);
  assert(clockMs == 200 && Serial.pos == Serial.input.size()); preserved();
  // Existing inactivity deadline, explicit abort, validation and atomic rename failures.
  for (int failure = 0; failure < 5; ++failure) {
    reset(); protectedFiles(); c = putCommand(1); c.crc = checksum("a");
    if (failure == 0) { Serial.append("DISK C 1 00000000\n"); } // payload timeout
    if (failure == 1) Serial.append("DISK ABORT\n");
    if (failure >= 2) chunk("a", true);
    if (failure == 2) c.crc ^= 1;
    if (failure == 3) validGmp = false;
    if (failure == 4) LittleFS.failRename = true;
    diskPut(c); assert(!LittleFS.nodes.count("/games/UPLOAD.TMP")); preserved();
    if (!failure) assert(clockMs == 8000 && Serial.output.find("DISK ERR timeout") != std::string::npos);
  }
  puts("PASS PKU-04 actual diskPut: 7 bad then good, abort on eighth consecutive, 21 interspersed failures, partial upload cleanup/drain, inactivity 8000 ms, old destination retained on all abort/commit failures");
}
