#!/usr/bin/env python3
"""Publishes the firmware images as GitHub Release assets, for Game-Maker: Sandbox Edition's site build to fetch.

Collects bootloader, partitions, firmware and littlefs (the full-library image) for each board from docs/fw/<env>/
(run scripts/build-pages.sh first, or pass --build), writes pocket-firmware.json (board, file, asset name, sha256, size,
version) and uploads everything with the GitHub CLI. Nothing is uploaded without --publish; the default is a dry run that
prints what it would do and the pin to copy into GameMaker's web/scripts/pocketFirmware.json.

  scripts/release_fw.py                 dry run: list assets, sizes and hashes
  scripts/release_fw.py --build         run scripts/build-pages.sh first (needs PlatformIO), then dry run
  scripts/release_fw.py --publish       gh release create <tag> with the assets (needs gh logged in)
Options: --tag fw-YYYY-MM-DD-<git>  --out DIR (where the manifest is written; default .pio/release-fw)
"""
import argparse, datetime, hashlib, json, pathlib, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
BOARDS = ["cyd-2432s028", "esp32-3248s035"]
FILES = ["bootloader.bin", "partitions.bin", "firmware.bin", "littlefs.bin"]


def sh(*a):
    return subprocess.check_output(a, cwd=ROOT, text=True).strip()


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--build", action="store_true")
    p.add_argument("--publish", action="store_true")
    p.add_argument("--tag")
    p.add_argument("--out", default=str(ROOT / ".pio" / "release-fw"))
    a = p.parse_args()
    if a.build:
        subprocess.check_call(["sh", str(ROOT / "scripts" / "build-pages.sh")])
    git = sh("git", "rev-parse", "--short=7", "HEAD")
    tag = a.tag or f"fw-{datetime.date.today().isoformat()}-{git}"
    files = []
    for board in BOARDS:
        for name in FILES:
            f = ROOT / "docs" / "fw" / board / name
            if not f.is_file():
                sys.exit(f"missing {f.relative_to(ROOT)}; run scripts/build-pages.sh (or --build)")
            data = f.read_bytes()
            files.append({"board": board, "file": name, "asset": f"{board}-{name}",
                          "sha256": hashlib.sha256(data).hexdigest(), "size": len(data), "path": f})
    manifest = {"version": tag, "files": [{k: v for k, v in e.items() if k != "path"} for e in files]}
    out = pathlib.Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    mf = out / "pocket-firmware.json"
    mf.write_text(json.dumps(manifest, indent=2) + "\n")
    for e in files:
        print(f"{e['asset']:36} {e['size']:>9} {e['sha256']}")
    print(f"manifest: {mf}\ntag: {tag}")
    cmd = ["gh", "release", "create", tag, str(mf), *[str(e["path"]) + "#" + e["asset"] for e in files],
           "--title", f"Pocket Player firmware {tag}",
           "--notes", f"Firmware images for the web flasher, built from {git}. Hashes are in pocket-firmware.json."]
    if not a.publish:
        print("dry run; to publish:\n  " + " ".join(f"'{c}'" if " " in c else c for c in cmd))
        return
    subprocess.check_call(cmd, cwd=ROOT)


main()
