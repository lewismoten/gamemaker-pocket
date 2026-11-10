#!/usr/bin/env python3
"""Build and run the repository's desktop tests, from any working directory."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]

# Explicit recipes: an unregistered new host program must fail discovery, rather
# than silently disappearing from the suite or being run without its arguments.
CPP = {
    "carddrawtest", "cardstest", "controlstest", "coretest", "favoritestest",
    "firstruntest", "frameprofile", "heaptest", "hostpaktest", "hosttest",
    "iconsdump", "idletest", "librarytest", "paddrawtest", "playshots",
    "sdfaulttest", "serialfstest", "settingstest", "setupshots", "setuptest",
    "storytest", "touchcaltest",
}
PYTHON = {"boardcardtest", "profile", "profiletest", "transitiontest"}
BOARD_VARIANTS = {
    "carddrawtest", "controlstest", "firstruntest", "idletest", "paddrawtest",
    "playshots", "settingstest", "setuptest", "storytest",
}
SANITIZED = {"coretest", "hostpaktest", "sdfaulttest"}


def main():
    work = Path(tempfile.mkdtemp(prefix="pocket-host-tests-"))
    results = {"PASS": 0, "FAIL": 0, "SKIP": 0}
    print(f"Logs, executables and renders: {work}", flush=True)

    def report(status, name, reason=""):
        results[status] += 1
        print(f"{status} {name}" + (f": {reason}" if reason else ""), flush=True)

    def run(name, commands, timeout=120, reject=None):
        log = work / (name.replace("/", "-") + ".log")
        try:
            with log.open("w") as output:
                for command in commands:
                    command = [str(arg) for arg in command]
                    output.write("$ " + shlex.join(command) + "\n")
                    output.flush()
                    proc = subprocess.run(command, cwd=ROOT, stdout=output,
                                          stderr=subprocess.STDOUT, timeout=timeout,
                                          env={**os.environ, "UBSAN_OPTIONS":
                                               os.environ.get("UBSAN_OPTIONS", "") + ":halt_on_error=1"})
                    if proc.returncode:
                        raise RuntimeError(f"exit {proc.returncode}")
            if reject and re.search(reject, log.read_text()):
                raise RuntimeError("diagnostic indicates failure despite exit 0")
        except (OSError, subprocess.TimeoutExpired, RuntimeError) as error:
            report("FAIL", name, f"{error}; see {log}")
            return False
        report("PASS", name)
        return True

    discovered = {p.stem for p in (ROOT / "host").glob("*.cpp")}
    python = {p.stem for p in (ROOT / "host").glob("*.py")}
    js = {p.name for p in (ROOT / "host").glob("*.mjs")}
    if discovered != CPP or python != PYTHON or js != {"pocketdisktest.mjs"}:
        report("FAIL", "discovery", f"update recipes: C++ {sorted(discovered ^ CPP)}, "
               f"Python {sorted(python ^ PYTHON)}, JS {sorted(js ^ {'pocketdisktest.mjs'})}")

    data = ROOT / "data"
    # Missing committed fixtures are failures, never optional-data skips.
    try:
        ids = [line.split("|")[0] for line in (data / "games.txt").read_text().splitlines()]
        if len(ids) != 106 or len(set(ids)) != 106:
            raise ValueError("expected 106 distinct onboard games; reconcile inventory before changing this")
        for filename in ("small.pak", "cards.pak", "font.bin", "thumbs.pak", "library.jsonl"):
            if not (data / filename).is_file():
                raise ValueError(f"missing committed fixture data/{filename}")
    except (OSError, ValueError) as error:
        report("FAIL", "data", str(error))
        ids = []

    # The parity walkthroughs live in the other repository. This bounded input
    # exercises the probe locally; it is explicitly not a walkthrough comparison.
    keys = work / "smoke.keys"
    keys.write_text("R 12345\nK 1 77\nK 61 205\nK 62 75\nK 122 203\nT 180\n")
    for name in sorted(CPP):
        for board in (["35", "28"] if name in BOARD_VARIANTS else [None]):
            label = name + (f"-{board}" if board else "")
            out = work / label
            out.mkdir()
            exe = out / name
            flags = ["-std=c++17", "-O1", "-Wall", "-Wextra"]
            if name in SANITIZED:
                flags += ["-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
            if name == "hostpaktest":
                flags += ["-Wno-unused-function"]
            if board == "28":
                flags += ["-DPOCKET_CYD28"]
            commands = [["c++", *flags, "-Isrc", f"host/{name}.cpp", "-lz", "-o", exe]]
            if name in {"cardstest", "heaptest", "iconsdump", "hosttest"}:
                for ident in ids:
                    args = [data, ident]
                    if name == "iconsdump":
                        args += ["30"]
                    elif name == "hosttest":
                        args += [keys]
                    commands.append([exe, *args])
            elif name in {"favoritestest", "librarytest"}:
                commands.append([exe, data, out])  # these exercise both widths internally
            elif name == "storytest":
                commands.append([exe, data, out, *ids])
            elif name == "playshots":
                commands.append([exe, data, out, "first-steps", "around-the-room", "jump-boots"])
            elif name == "sdfaulttest":
                commands += [[exe, data, ident] for ident in ("first-steps", "take-the-wheel")]
            elif name in {"hostpaktest", "firstruntest", "settingstest", "setupshots"}:
                commands.append([exe, out])
            elif name == "paddrawtest":
                commands.append([exe, out / "pad.ppm"])
            elif name == "frameprofile":
                # Full every-level coverage follows via profile.py; profiletest.py
                # additionally checks reproducibility and rejected inputs.
                commands.append([exe, data, "first-steps", "180", "3"])
            else:
                commands.append([exe])
            # Some parity producers print errors but still return zero.
            reject = r"(?m)^C .*\bERR\b|^Z \d+ [1-9]\d*\s*$|^FAIL\b"
            run(label, commands, reject=reject)

    for name in sorted(PYTHON - {"profile"}):
        run(name, [[sys.executable, ROOT / "host" / (name + ".py")]])
    run("profile-onboard", [[sys.executable, ROOT / "host/profile.py", "--data", data,
                            "--out", work / "profile", "--expected-games", "106"]], timeout=1800)
    run("pocketdisktest", [["node", ROOT / "host/pocketdisktest.mjs"]])
    run("pocket-heap-test", [[sys.executable, ROOT / "scripts/pocket-heap-test.py"]])
    run("pocket-inventory-test-unit", [[sys.executable, ROOT / "scripts/pocket-inventory-test.py",
        "InventoryTests.test_missing_assets_remain_in_inventory_and_duplicates_are_rejected"]])

    report("SKIP", "pocket-inventory-test-integration",
           "requires external Game-Maker checkout and its npm run check:cyd (not in this repo)")
    report("SKIP", "pocket-heap-audit",
           "requires external microSD games.txt/assets and Game-Maker checkout; onboard heaptest ran for all 106 games")
    report("SKIP", "engine parity and hosttest resume",
           "engine, golden frame hashes and winning walkthroughs are in Game-Maker web/scripts/pocket*Check.ts, not this repo; local producers ran")
    report("SKIP", "microSD/bundle game probes",
           "no exported card games in this repo; generated bundle range fixtures ran in hostpaktest")
    print("Summary: " + ", ".join(f"{count} {status}" for status, count in results.items()), flush=True)
    return int(results["FAIL"] != 0)


if __name__ == "__main__":
    # Python -O would disable assertions in the Python probes.
    os.environ.pop("PYTHONOPTIMIZE", None)
    raise SystemExit(main())
