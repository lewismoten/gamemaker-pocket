# Pocket host failure and frame probes

These run on a desktop, without a board, SD controller, display, or serial connection.
They supplement `web/scripts/pocketHostCheck.ts` (walkthrough parity), not replace it.
Use a C++17 compiler and zlib development headers, plus Python 3. Run from the repository root.

## Run everything

```sh
python3 tools/run_host_tests.py
```

Also needs Node.js with Web Streams and a compiler with AddressSanitizer and
UndefinedBehaviorSanitizer. The script resolves paths from its own location, so it
can be invoked by absolute path from another directory. It discovers every
`host/*.cpp`, `host/*.py` and `host/*.mjs` and fails if one lacks a recipe. A failed
build, assertion, command, timeout, sanitizer report, card `ERR`, or sound timing
failure makes the command exit nonzero; remaining test programs still run.
Missing tools or committed `data/` files are failures, not skips.

Each test prints PASS/FAIL; unavailable external checks print SKIP with a reason.
The final line counts all three. All build products, exact commands, stdout/stderr,
PNGs, PPMs and profiling reports stay in the printed `pocket-host-tests-*` temporary
directory for inspection (remove it when finished). Nothing is flashed or written
to game data. The every-level profiler uses its full defaults, so allow several
minutes for the suite.

Builds use the existing host recipes: `c++ -std=c++17 -O1 -Wall -Wextra -Isrc`,
linked with `-lz`. `coretest`, `sdfaulttest` and `hostpaktest` additionally use
`-g -fsanitize=address,undefined -fno-omit-frame-pointer`; `hostpaktest` retains
`-Wno-unused-function`. Undefined behavior stops the test immediately. The Python
probes retain their own compiler commands. Assertions remain enabled.

| Programs | Runner arguments / coverage |
| --- | --- |
| `carddrawtest`, `controlstest`, `idletest`, `setuptest` | No arguments; default 3.5 inch build and `-DPOCKET_CYD28` 2.8 inch build |
| `firstruntest`, `settingstest` | Separate output directory for each board build |
| `paddrawtest` | Output PPM filename for each board build |
| `coretest`, `serialfstest`, `touchcaltest` | No arguments |
| `hostpaktest` | Scratch directory for generated valid/malformed bundle fixtures |
| `favoritestest`, `librarytest` | `data OUTPUT`; both widths exercised internally |
| `storytest` | `data OUTPUT ID...`, all 106 indexed ids, both board builds |
| `playshots` | `data OUTPUT first-steps around-the-room jump-boots`, both board builds |
| `cardstest`, `heaptest` | `data ID`, separately for every indexed game |
| `iconsdump` | `data ID 30`, separately for every indexed game (icon producer smoke check) |
| `hosttest` | `data ID KEYS`, every indexed game; generated seed 12345, Right then Left, 180 ticks (smoke check, not engine parity) |
| `sdfaulttest` | `data first-steps` and `data take-the-wheel` |
| `frameprofile` | `data first-steps 180 3`; also exercised by both profiling scripts below |
| `boardcardtest.py`, `transitiontest.py`, `profiletest.py` | No arguments; existing regression assertions |
| `profile.py` | `--data data --out OUTPUT --expected-games 106`; all levels, 180 samples, three repetitions; affinity recorded as null on platforms without that API |
| `pocketdisktest.mjs` | `node host/pocketdisktest.mjs` |
| `scripts/pocket-heap-test.py` | All generated-fixture allocation regressions |
| `scripts/pocket-inventory-test.py` | `InventoryTests.test_missing_assets_remain_in_inventory_and_duplicates_are_rejected` |

These checks require files outside this repository and are explicitly skipped:

- The other inventory test, `test_runner_records_every_packed_level_and_checkout_identity`,
  invokes `scripts/pocket-heap-audit.py`, which requires a separate Game-Maker checkout,
  its git identity, Node/npm and `npm run check:cyd`.
- The full heap audit additionally needs an exported microSD `games.txt` and assets:
  `GAMEMAKER_DIR=/path/to/GameMaker python3 scripts/pocket-heap-audit.py --card CARD/pocket --out OUTPUT --expected-games 121`.
  Reconcile that count if using a different inventory. Its local heap probe and
  fixture regressions still run in the suite.
- Engine parity, golden screenshot comparisons and winning Continue/Restart
  walkthroughs are in Game-Maker's `web/scripts/pocketHostCheck.ts` (including
  `--resume`), `pocketCardsCheck.ts` and `pocketControlsCheck.ts`. Running the local
  producers does not claim those comparisons passed.
- Real exported microSD/`.GMP` game probes need external games. Generated malformed
  bundle fixtures are still tested by `hostpaktest`.

The serial scripts (`pocketDiskTest.py`, `pocketBoardCheck.py`, `pocketHeldTrace.py`,
`pocketWalk.py`) require hardware and are outside the host suite. `pocketIconsShot.py`
is a Pillow illustration utility; its underlying `iconsdump` producer runs above.
`tools/svgHand.py` and `scripts/build-pages.sh` generate assets/build firmware and
are not tests.

## Reader failures

```sh
mkdir -p "$TMPDIR/pocket-faults"
c++ -std=c++17 -O1 -g -Wall -Wextra -fsanitize=address,undefined \
  -fno-omit-frame-pointer -Isrc \
  host/sdfaulttest.cpp -lz -o "$TMPDIR/pocket-faults/sdfaulttest"
timeout 30 "$TMPDIR/pocket-faults/sdfaulttest" data first-steps
timeout 30 "$TMPDIR/pocket-faults/sdfaulttest" data take-the-wheel
python3 host/transitiontest.py
python3 host/boardcardtest.py
c++ -std=c++17 -O1 -g -Wall -Wextra -Wno-unused-function \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Isrc host/hostpaktest.cpp \
  -o "$TMPDIR/pocket-faults/hostpaktest"
"$TMPDIR/pocket-faults/hostpaktest" "$TMPDIR/pocket-faults"
```

The same probe accepts a loose microSD cfg's short name and its card pack filename:

```sh
"$TMPDIR/pocket-faults/sdfaulttest" CARD_ROOT/pocket SHORTNAME SHORTNAME.pak
# One-file bundle (use the id stored inside its cfg, not the DOS .GMP filename):
"$TMPDIR/pocket-faults/sdfaulttest" CARD_ROOT/pocket/NAME.GMP game-id
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
# First generate microSD files with pocketData.ts --out CARD_ROOT (see ../README.md).
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
