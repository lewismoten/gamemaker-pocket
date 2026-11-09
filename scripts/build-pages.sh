#!/bin/sh
# Builds both boards with PlatformIO and refreshes docs/fw/<env>/ and the web flasher's manifests (docs/manifest-*.json).
# Run from anywhere: scripts/build-pages.sh. Needs pio (or ~/.platformio/penv/bin/pio). Commit the result; GitHub Pages serves docs/.
set -eu
cd "$(dirname "$0")/.."
PIO=$(command -v pio || echo "$HOME/.platformio/penv/bin/pio")
ID=$(git rev-parse --short=7 HEAD)
[ -z "$(git status --porcelain -- . ':!docs')" ] || ID="$ID+"
DATE=$(date +%Y-%m-%d)
export POCKET_FW_ID="$ID"
build() { # env, size label, manifest file
  env=$1; label=$2; manifest=$3
  "$PIO" run -e "$env"
  "$PIO" run -e "$env" -t buildfs
  out=.pio/build/$env; dst=docs/fw/$env
  mkdir -p "$dst"
  cp "$out/bootloader.bin" "$out/partitions.bin" "$out/firmware.bin" "$out/littlefs.bin" "$dst/"
  cat > "docs/$manifest" <<JSON
{
  "name": "Game-Maker Pocket Player, $label board",
  "version": "$ID ($DATE)",
  "new_install_prompt_erase": false,
  "builds": [
    {
      "chipFamily": "ESP32",
      "parts": [
        { "path": "fw/$env/bootloader.bin", "offset": 4096 },
        { "path": "fw/$env/partitions.bin", "offset": 32768 },
        { "path": "fw/$env/firmware.bin", "offset": 65536 },
        { "path": "fw/$env/littlefs.bin", "offset": 1114112 }
      ]
    }
  ]
}
JSON
}
build cyd-2432s028 "2.8 inch" manifest-28.json
build esp32-3248s035 "3.5 inch" manifest-35.json
echo "Done: $ID. Commit docs/."
