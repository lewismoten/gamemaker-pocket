# Pocket host tests and frame probes

These run on a desktop, without a board, SD controller, display, or serial connection.
They supplement `web/scripts/pocketHostCheck.ts` (walkthrough parity), not replace it.
Use a C++17 compiler and zlib development headers, Python 3, and Node.js for the serial-page test. Run from the repository root.

## Shared models and renders

Run these from the repository root. Plain builds work on macOS without `timeout` or sanitizer runtime setup. The output stays in a temporary directory; assertions must stay enabled.

```sh
POCKET_TEST_OUT=$(mktemp -d "${TMPDIR:-/tmp}/pocket-host.XXXXXX")
for size in 35 28; do
  flags=""
  if [ "$size" = 28 ]; then flags="-DPOCKET_CYD28"; fi
  out="$POCKET_TEST_OUT/$size"
  mkdir -p "$out"
  for test in coretest serialfstest controlstest carddrawtest touchcaltest setuptest idletest paddrawtest firstruntest settingstest librarytest favoritestest setupshots playshots storytest; do
    c++ -std=c++17 -O1 $flags -Isrc "host/$test.cpp" -lz -o "$out/$test" || exit 1
    case "$test" in
      librarytest|favoritestest|playshots) "$out/$test" data "$out" ;;
      storytest) "$out/$test" data "$out" first-steps ;;
      setupshots|firstruntest|settingstest) "$out/$test" "$out" ;;
      paddrawtest) "$out/$test" "$out/pad.ppm" ;;
      *) "$out/$test" ;;
    esac || exit 1
  done
done
node host/pocketdisktest.mjs
python3 host/transitiontest.py
python3 host/boardcardtest.py
python3 host/profiletest.py
```

The shell block is for a POSIX shell (`sh` or `bash`); if using zsh, run it inside `sh` so the optional compiler flag expands as intended. The two sizes exercise the shared models and render assertions; saved PPM/PNG files let a person inspect the pictures. This command does not compare image hashes against the companion's stored snapshots. `hosttest.cpp` needs recorded key input; `cardstest.cpp` emits decoded-card hashes. The companion scripts compare those outputs with its engine (see below). `heaptest.cpp`, `frameprofile.cpp` and `iconsdump.cpp` are probes, not standalone pass/fail suites.

All host results are **hardware-untested**. Real touch, display, audio, SD removal, NVS writes and power loss need board checks.

## Engine parity in the companion checkout

The engine and walkthroughs are in Game-Maker: Sandbox Edition, not this repo. From that checkout's `web/` folder, point the scripts at this worktree:

```sh
export GAMEMAKER_POCKET_DIR=/absolute/path/to/gamemaker-pocket
npx vite-node scripts/pocketHostCheck.ts --resume
npx vite-node scripts/pocketCardsCheck.ts
npx vite-node scripts/pocketControlsCheck.ts
```

Use the absolute path to your own Pocket checkout. These scripts skip when they cannot find it; a skip is not a passed parity check. They need the companion's dependencies and packed game data. See [the firmware notes](../docs/FIRMWARE.md#check-the-play-loop-on-the-mac) for the comparisons.

## Reader failures

```sh
POCKET_FAULT_OUT=$(mktemp -d "${TMPDIR:-/tmp}/pocket-faults.XXXXXX")
c++ -std=c++17 -O1 -g -Wall -Wextra -Isrc \
  host/sdfaulttest.cpp -lz -o "$POCKET_FAULT_OUT/sdfaulttest"
"$POCKET_FAULT_OUT/sdfaulttest" data first-steps
"$POCKET_FAULT_OUT/sdfaulttest" data take-the-wheel
python3 host/transitiontest.py
python3 host/boardcardtest.py
c++ -std=c++17 -O1 -g -Wall -Wextra -Wno-unused-function \
  -Isrc host/hostpaktest.cpp \
  -o "$POCKET_FAULT_OUT/hostpaktest"
"$POCKET_FAULT_OUT/hostpaktest" "$POCKET_FAULT_OUT"
```

The same probe accepts a loose microSD cfg's short name and its card pack filename:

```sh
"$POCKET_FAULT_OUT/sdfaulttest" CARD_ROOT/pocket SHORTNAME SHORTNAME.pak
# One-file bundle (use the id stored inside its cfg, not the DOS .GMP filename):
"$POCKET_FAULT_OUT/sdfaulttest" CARD_ROOT/pocket/NAME.GMP game-id
```

It chooses the first picture-bearing intro/exit/won/lost card list. A game with no
configured picture cards reports that portion as N/A; it is never counted as a
successful picture-fault test.

The C++ probe runs the real shared `Assets`/card decoders with callback-level faults:
unavailable/open failure, a three-byte packed read, a truncated stream, invalid deflate,
and excessive unpacked length. It fails each asset-reader call in turn, releases partial
loads twice, and retries successfully. Missing sound is intentionally nonfatal; other
assets must fail the load. A missing configuration open, removed configuration at reload,
and malformed header are rejected. It also checks picture-range failures and corruption.
For bundles, malformed magic, a short header, and failed header/index/first streamed-cfg
range reads are checked. `hostpaktest.cpp` uses tiny generated fixtures to ensure the stdio
bundle adapter rejects impossible payload ranges **before allocation**, including an
addition-overflow case, while still reading valid and empty entries. This does not impose
an arbitrary file-size budget on valid bundles.

The two Python probes compile the **actual selected routines from `main.cpp`** with
panel/time stubs: a failed transition must return to the picker before drawing the partial
world; a failed picture card must show a bounded notice without waiting for a key. Those
are source-slice checks, not a firmware build or a real screen/timer test. They deliberately
fail if those guards disappear. They cannot verify the Arduino file implementation.

A transport must reject a failed or short _exact_ range with `nullptr`; `ReadRange` carries
no actual-length field. The board does this in `readPak`. Mount failure, open failure,
physical removal, and a failed low-level read can produce the same callback result here;
the probe does not distinguish their physical causes. `Lines::next` cannot distinguish
EOF from an I/O failure, so a configuration truncated after a valid prefix is **not**
proven rejected. These probes are not a general malformed-file fuzzer, allocation-failure
sweep, FAT test, or proof of controller timeout behavior. Test those separately on hardware.

## Every-level software frame profiling

```sh
python3 host/profiletest.py
# In the companion web/ folder, generate files with pocketData.ts --out CARD_ROOT.
# Then return to this repository root; see the root README for the card layout.
python3 host/profile.py \
  --data data --card CARD_ROOT/pocket \
  --out "$TMPDIR/pocket-profile" --expected-games 121
```

The inventory assertion is intentional: if the current card pack has a different number
of games, reconcile the inventory before setting `--expected-games` to the observed count.
For onboard-only runs omit `--card` and use `--expected-games 106`. Optional `--cpu N` pins
the process and its child to an **allowed** Linux CPU; it does not reserve that CPU or
exclude other work. Without it affinity is unchanged. Save the entire output directory.

The driver compiles `frameprofile.cpp` at `-O1` (the parity harness uses the same level),
reads every index row, rejects duplicate ids, and checks all indexed level/repetition/sample
coordinates. Each level is entered directly at its configured start, in cfg order so an
inherited hero is available. Each repetition uses seed 12345; each level gets 30 warmup
frames, then 180 recorded frames, repeated three times by default. The repeating 180-frame
input cycle holds Right for 60, Left for 60, then neither; Space is held for the first six
of every 30 frames. Warmup participates in that cycle. A death, link, or win resets the
**same** selected level outside the timed interval. Resets are counted in `levels.csv`.
This is a synthetic workload, not a walkthrough, playability test, all-room exploration,
or proof that every reachable state was sampled.

Each sample measures `World::tick()` (including its draw-list update), followed by all
196 rows × 312 visible palette indices from `renderRow`. A pixel checksum keeps rendering
observable. Wall time uses `steady_clock`; `cpu_us` uses process CPU time (one thread),
including the clock calls surrounding the wall interval, so small rounding differences
are expected. CSV printing, loading, resets, fades/cards, input injection, and sleeps are
outside the timed interval. Full software rendering is forced, unlike the board's dirty
bands. CPU time helps identify scheduler noise but is still host-specific.

Outputs: one raw CSV per game; `inventory.json`; per-level `levels.csv`; and `summary.json`
with nearest-rank p50/p95/p99, maxima, sample counts, compiler/build flags, source hash,
revision, UTC timestamps, CPU model, affinity, and system load. Keep noisy runs rather than
silently dropping outliers. `profiletest.py` checks sample coordinates, checksum/event
reproducibility, finite timings, and rejected invalid inputs on the real two-level fixture.

**Not CYD timing:** there is no ESP32 instruction cost, SPI/display push, palette/RGB565
conversion, board controls, audio task contention, SD latency, NVS, or board allocator in
these frame measurements. No board performance budget is defined by this tool. The loop's
18.2 Hz scheduling cadence in `main.cpp` is not a host-frame or board-render budget; any
comparison to that cadence must be labeled a host-only diagnostic, not a CYD pass/fail.

The Send games page's code (`docs/pocketdisk.js`) has a Node test with a mock Web Serial port: `node host/pocketdisktest.mjs`. The serial file commands' parser is `host/serialfstest.cpp`, and the play core's count clamps are in `host/coretest.cpp`.
