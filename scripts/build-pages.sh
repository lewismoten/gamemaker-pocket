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
PLAYER_DATA=$(mktemp -d "${TMPDIR:-/tmp}/pocket-player-data.XXXXXX")
trap 'rm -rf "$PLAYER_DATA"' EXIT
cp data/font.bin "$PLAYER_DATA/"  # the only file the player itself needs; no Starter courses
manifest() { # env, size label, kind (player|courses), description, output file
  env=$1; label=$2; kind=$3; file=$4
  if [ "$kind" = player ]; then name="Game-Maker Pocket Player only, $label board"; img=littlefs-player.bin
  else name="Game-Maker Pocket Player with Starter courses, $label board"; img=littlefs.bin; fi
  cat > "docs/$file" <<JSON
{
  "name": "$name",
  "version": "$ID ($DATE)",
  "new_install_prompt_erase": false,
  "builds": [
    {
      "chipFamily": "ESP32",
      "parts": [
        { "path": "fw/$env/bootloader.bin", "offset": 4096 },
        { "path": "fw/$env/partitions.bin", "offset": 32768 },
        { "path": "fw/$env/firmware.bin", "offset": 65536 },
        { "path": "fw/$env/$img", "offset": 1114112 }
      ]
    }
  ]
}
JSON
}
build() { # env, size label, board number (28 or 35)
  env=$1; label=$2; n=$3
  "$PIO" run -e "$env"
  "$PIO" run -e "$env" -t buildfs
  out=.pio/build/$env; dst=docs/fw/$env
  mkdir -p "$dst"
  cp "$out/bootloader.bin" "$out/partitions.bin" "$out/firmware.bin" "$out/littlefs.bin" "$dst/"
  PLATFORMIO_DATA_DIR="$PLAYER_DATA" "$PIO" run -e "$env" -t buildfs
  cp "$out/littlefs.bin" "$dst/littlefs-player.bin"
  # rebuild the full image last so .pio/build/ ends as a normal build
  "$PIO" run -e "$env" -t buildfs
  manifest "$env" "$label" courses "manifest-$n-courses.json"
  manifest "$env" "$label" player "manifest-$n-player.json"
  cp "docs/manifest-$n-courses.json" "docs/manifest-$n.json"  # the first manifest URL keeps working: it is the courses install
}
build cyd-2432s028 "2.8 inch" 28
build esp32-3248s035 "3.5 inch" 35
echo "Done: $ID. Commit docs/."
