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

--publish refuses unless the working tree is clean and the images were built from HEAD: build-pages.sh stamps the
commit id into docs/manifest-*.json ("version": "<id> (<date>)"; "<id>+" means a dirty build). The stamped commit may
differ from HEAD only by files under docs/ (committing the built images). The tag is created on HEAD (--target).
The dry run prints the same problems as warnings. Each image is read once; the hash is of the bytes that are staged.
"""
import argparse, datetime, hashlib, json, pathlib, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
BOARDS = ["cyd-2432s028", "esp32-3248s035"]
FILES = ["bootloader.bin", "partitions.bin", "firmware.bin", "littlefs.bin"]


def sh(*a):
    return subprocess.check_output(a, cwd=ROOT, text=True).strip()


def problems(git_head):
    """Reasons the images in docs/fw can't be trusted to be HEAD's build (empty list = fine)."""
    out = []
    if sh("git", "status", "--porcelain"):
        out.append("working tree is not clean; commit or remove changes first")
    stamps = sorted((ROOT / "docs").glob("manifest-*.json"))
    if not stamps:
        out.append("no docs/manifest-*.json build stamp; run scripts/build-pages.sh (or --build)")
    for m in stamps:
        ver = json.loads(m.read_text()).get("version", "")
        sid = ver.split(" ")[0]
        if sid.endswith("+"):
            out.append(f"{m.name} was built from a dirty tree ({sid}); rebuild from a clean HEAD")
            continue
        try:
            sh("git", "diff", "--quiet", sid, "HEAD", "--", ".", ":!docs")
        except subprocess.CalledProcessError:
            out.append(f"{m.name} was built from {sid or '?'}, not HEAD {git_head}; run scripts/build-pages.sh (or --build)")
    return out


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
    bad = problems(git)
    for b in bad:
        print(("refusing: " if a.publish else "warning: ") + b, file=sys.stderr)
    if bad and a.publish:
        sys.exit(1)
    head = sh("git", "rev-parse", "HEAD")
    tag = a.tag or f"fw-{datetime.date.today().isoformat()}-{git}"
    files = []
    for board in BOARDS:
        for name in FILES:
            f = ROOT / "docs" / "fw" / board / name
            if not f.is_file():
                sys.exit(f"missing {f.relative_to(ROOT)}; run scripts/build-pages.sh (or --build)")
            data = f.read_bytes()  # read once: the hash and the staged copy come from this buffer
            files.append({"board": board, "file": name, "asset": f"{board}-{name}",
                          "sha256": hashlib.sha256(data).hexdigest(), "size": len(data), "data": data})
    manifest = {"version": tag, "files": [{k: v for k, v in e.items() if k != "data"} for e in files]}
    out = pathlib.Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    mf = out / "pocket-firmware.json"
    mf.write_text(json.dumps(manifest, indent=2) + "\n")
    for e in files:
        # gh uses the file's own name as the asset name ("path#text" only sets a label), so both boards'
        # bootloader.bin would collide: copy each image to its board-prefixed asset name first.
        staged = out / e["asset"]
        staged.write_bytes(e.pop("data"))
        e["path"] = staged
        print(f"{e['asset']:36} {e['size']:>9} {e['sha256']}")
    print(f"manifest: {mf}\ntag: {tag}")
    cmd = ["gh", "release", "create", tag, str(mf), *[str(e["path"]) for e in files],
           "--target", head, "--title", f"Pocket Player firmware {tag}",
           "--notes", f"Firmware images for the web flasher, built from {git}. Hashes are in pocket-firmware.json."]
    if not a.publish:
        print("dry run; to publish:\n  " + " ".join(f"'{c}'" if " " in c else c for c in cmd))
        return
    subprocess.check_call(cmd, cwd=ROOT)


if __name__ == "__main__":
    main()
