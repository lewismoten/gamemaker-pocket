#!/usr/bin/env python3
"""Board test of the USB file commands (the same lines docs/pocketdisk.js sends from the flasher page):
  ~/.platformio/penv/bin/python scripts/pocketDiskTest.py /dev/cu.usbserial-XXXX path/to/NAME.GMP
Lists /games, sends the file in 1024-byte CRC'd chunks (one bad chunk on purpose, to see the retry), rescans, checks the game
list holds it, starts it (`G n`), resets the board, then deletes it and checks the list is back as it was. Exits 1 on a failure."""
import sys, os, time, zlib
sys.path.insert(0, os.path.dirname(__file__))
from pocketStatus import open_port

def line(s, ms=8000):
    end = time.time() + ms / 1000
    while time.time() < end:
        l = s.readline().decode("utf-8", "replace").strip()
        if l: return l
    raise SystemExit("timeout")

def disk(s, text, until="DISK DONE", ms=8000):
    s.reset_input_buffer(); s.write((text + "\n").encode()); s.flush()
    out = []
    while True:
        l = line(s, ms)
        if not l.startswith("DISK "): continue
        out.append(l)
        if l.startswith("DISK ERR") or l.startswith(until): return out

def ls(s):
    r = disk(s, "DISK LS", "DISK END")
    return [l.split()[2:] for l in r if l.startswith("DISK F ")], [l.split()[2] for l in r if l.startswith("DISK O ")], [l for l in r if l.startswith("DISK FREE")][0]

def put(s, name, data, corrupt_first=False):
    r = disk(s, f"DISK PUT {name} {len(data)} {zlib.crc32(data):08x}", "DISK GO")
    assert r[-1] == "DISK GO", r
    for off in range(0, len(data), 1024):
        part = data[off:off + 1024]
        for attempt in range(3):
            s.write(f"DISK C {len(part)} {zlib.crc32(part):08x}\n".encode())
            s.write(bytes([part[0] ^ 255]) + part[1:] if corrupt_first and off == 0 and attempt == 0 else part); s.flush()
            while True:
                l = line(s)
                if l.startswith("DISK "): break
            if l.startswith("DISK OK"): break
            print("  chunk answered:", l, "-> sending it again")
        else: raise SystemExit("chunk failed")
    while True:
        l = line(s, 15000)
        if l.startswith("DISK "): return l

def main():
    port, path = sys.argv[1], sys.argv[2]
    name = os.path.basename(path).upper(); data = open(path, "rb").read()
    s = open_port(port)
    for _ in range(5):
        r = disk(s, "DISK HELLO", "DISK HELLO", 2000) if True else None
        if r and r[-1].startswith("DISK HELLO"): break
    print(r[-1])
    before = ls(s); print("before:", before)
    assert not any(f[0] == name for f in before[0]), "already there"
    # refusals
    for bad in ["DISK PUT ../X.GMP 10 00000000", "DISK PUT x.gmp 10 00000000", "DISK DEL ORDER.TXT", "DISK PUT LONGNAMEXX.GMP 10 0", "DISK PUT X.TXT 10 0", "DISK PUT BIG.GMP 99999999 0"]:
        r = disk(s, bad, "DISK GO"); assert r[-1].startswith("DISK ERR"), (bad, r)
    print("refusals ok")
    print("put:", put(s, name, data, corrupt_first=True))
    r = disk(s, "DISK ORDER %d" % (len(before[1]) + 1), "DISK GO")
    for n in before[1] + [name]: s.write((n + "\n").encode())
    s.flush(); l = line(s)
    while not l.startswith("DISK"): l = line(s)
    print("order:", l)
    print("rescan:", disk(s, "DISK RESCAN", "DISK DONE", 20000)[-1])
    after = ls(s); print("after:", after)
    assert any(f[0] == name and int(f[1]) == len(data) for f in after[0]) and name in after[1]
    s.reset_input_buffer(); s.write(b"L\n"); s.flush(); games = []
    while True:
        l = line(s)
        if l.startswith("L done"): break
        if l.startswith("L "): games.append(l)
    print("game list:", games)
    print("library has", len(games), "games")
    s.write(("G %d\n" % [g.split()[2] for g in games].index(name[:-4].lower())).encode()); s.flush()
    while True:
        l = line(s, 15000)
        if l.startswith("G "): print("start:", l); break
    s.close(); s = open_port(port, reset=True)  # back to the list (a game has no exit by serial)
    print("delete:", disk(s, f"DISK DEL {name}")[-1])
    disk(s, "DISK ORDER %d" % len(before[1]), "DISK GO")
    for n in before[1]: s.write((n + "\n").encode())
    s.flush(); line(s); print("rescan:", disk(s, "DISK RESCAN", "DISK DONE", 20000)[-1])
    end = ls(s); print("end:", end)
    assert end[0] == before[0] and end[1] == before[1], "list differs from the start"
    print("PASS")
main()
