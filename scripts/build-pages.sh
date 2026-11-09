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
GAMES_DATA=$(mktemp -d "${TMPDIR:-/tmp}/pocket-games-data.XXXXXX")
trap 'rm -rf "$PLAYER_DATA" "$GAMES_DATA"' EXIT
cp data/font.bin "$PLAYER_DATA/"  # the only file the player itself needs; no Starter courses
# The starter games are exported from a Game-Maker: Sandbox Edition checkout (GAMEMAKER_DIR, default ../GameMaker) with its
# pocketData.ts exporter, Pocket-only (no DOS files). A file name's number keeps the list in this order (the board lists flash
# files by name); the first is the one that must fit, so drop from the end if the image is ever too big.
STARTER_GAMES="blaster:1-VIRUS snowbound:2-SNOWBOUND brief:3-BRIEF sandcast:4-SANDCAST remote:5-REMOTE"
GAMEMAKER_DIR=${GAMEMAKER_DIR:-"$(cd .. && pwd)/GameMaker"}
mkdir -p "$GAMES_DATA/games"
cp data/font.bin "$GAMES_DATA/"
EXPORT=$(mktemp -d "${TMPDIR:-/tmp}/pocket-gmp.XXXXXX")
ids=$(for g in $STARTER_GAMES; do printf '%s,' "${g%%:*}"; done); ids=${ids%,}
(cd "$GAMEMAKER_DIR/web" && npx vite-node scripts/pocketData.ts --gmp "$EXPORT" --no-dos --only "$ids" >/dev/null)
for g in $STARTER_GAMES; do
  f=$(ls "$EXPORT/pocket" | grep -i -m1 "^$(echo "${g%%:*}" | cut -c1-8)" || true)
  [ -n "$f" ] || { echo "no .GMP exported for ${g%%:*}" >&2; exit 1; }
  cp "$EXPORT/pocket/$f" "$GAMES_DATA/games/${g#*:}.GMP"
done
rm -rf "$EXPORT"
manifest() { # env, size label, kind (player|courses|games), output file
  env=$1; label=$2; kind=$3; file=$4
  if [ "$kind" = player ]; then name="Game-Maker Pocket Player only, $label board"; img=littlefs-player.bin
  elif [ "$kind" = games ]; then name="Game-Maker Pocket Player with starter games, $label board"; img=littlefs-games.bin
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
  PLATFORMIO_DATA_DIR="$GAMES_DATA" "$PIO" run -e "$env" -t buildfs
  cp "$out/littlefs.bin" "$dst/littlefs-games.bin"
  # rebuild the full image last so .pio/build/ ends as a normal build
  "$PIO" run -e "$env" -t buildfs
  manifest "$env" "$label" courses "manifest-$n-courses.json"
  manifest "$env" "$label" player "manifest-$n-player.json"
  manifest "$env" "$label" games "manifest-$n-games.json"
  cp "docs/manifest-$n-courses.json" "docs/manifest-$n.json"  # the first manifest URL keeps working: it is the courses install
}
build cyd-2432s028 "2.8 inch" 28
build esp32-3248s035 "3.5 inch" 35
echo "Done: $ID. Commit docs/."
