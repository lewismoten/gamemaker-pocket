# Changelog

The firmware has no release number of its own. Every build bakes in its git hash and date (`fwid.py`), and the serial command `FW` prints them (`FW board=C git=1a2b3c4 date=2026-11-09 env=cyd-2432s028`). This changelog starts at 0.1.0, the first version of this repository.

## Unreleased

### Added

- A game's own menu, as PLAYGAME draws it (Play, Read Instructions, Read Storyline, See Credits, See Highest Scores, Quit), opens from Details > Play: each word is a touch row and a strip at the foot has Up, Down, Enter and Esc (`src/gmmenu.h`, `src/dosmenudraw.h`). A game with no packed menu picture shows PLAYGAME's default screen; the credits and Top Ten backgrounds are not packed yet.
- Pause has Save game and Load game (PLAYGAME's F5 and F6): each opens a list of ten slots (number and saved name, or "empty"; empty slots are dimmed for Load). Save asks for a name of up to 20 letters on the Search keyboard (the Close X cancels) and asks with a second tap before writing over a used slot. A game's slot n is `/<hash>.s<n>` and its names are a 210-byte table, `/<hash>.nam`, laid out like DOS's SavNames.dat; the contents are a run-length packed copy of the play core's state (`src/gmsave.h`, `src/saveslots.h`, `src/slotdraw.h`; `host/slotstest.cpp`). A save from the one-file build (`.sav`) is not read. Pause also has the Close X (it resumes).
- The Close X's touch area is larger than its drawing (to the glass edge and the top, and 8 px down on the 3.5 inch board) and counts as a press in Pause, so a tap near the corner registers the first time.
- The Top Ten: when a game ends with a score that makes it, a name entry on the Search keyboard, then the Top Ten, saved per game in PLAYGAME's `.his` record layout (`src/gmhiscore.h`); See Highest Scores shows it from the menu.
- Settings pages read [Prev] 1/4 [Next] in place of "More 1/4".
- Serial: `D n` opens Details, `P` pauses, `HS n` ends a game with score n (walks only); `scripts/pocket-walks/dos-menu-*.txt`, `host/gamesavetest.cpp`, `host/dosmenushots.cpp`.
- Library redesign from the UI pack: 40 px title-bar buttons, page buttons 38 px (2.8 inch) or 44 px (3.5 inch) tall, a larger title, a castle badge on the home page, the card chip in the footer, and a thick yellow frame on the chosen game.
- The boot splash is the UI pack's castle art with a live status line below it ("Starting up", "Looking for games"), drawn from run-length coded app flash at 320 x 240 and 480 x 320 (`tools/splashPack.py`, `art/ui-pack/`); it replaces the 160 x 100 sting frame and shrinks the firmware by about 25 KB. The first-run display setup is unchanged and still comes first.
- Games can be kept in the board's flash (LittleFS `/games`) and play like card games, marked On board; a card copy wins over a board copy.
- The web flasher's Send games tab and the serial `DISK` commands: list, send, remove and reorder the games in the board's flash over USB (`src/serialfs.h`, `docs/send.js`, `docs/pocketdisk.js`).
- The web flasher's "Player + starter games" install (Virus Blaster, Snowbound, KEEP IT BRIEF!, Sandcastle Knight, Where's the Remote?!), now its default.

### Fixed

- Send games: the port closes cleanly (a retry after Disconnect, a busy board or a timeout needs no page reload); the space check no longer counts the file a send replaces; bytes that are not a game are refused (`DISK ERR notgmp`); input after an aborted transfer is discarded; the order list takes up to 256 games.
- A level's block counts from a `.cfg` are clamped to what the engine can index, and the block file's size is checked in 64 bits (a hostile count could wrap on the 32-bit board and read past a buffer).
- `host/README.md` commands use this repository's paths.

## 0.1.0 - 2026-11-09

### Added

- First release of this repository: the Pocket Player firmware split out, with its history, from [Game-Maker: Sandbox Edition](https://github.com/lewismoten/GameMaker-SandboxEdition) (`hardware/pocket-player/`, 218 commits from 2026-11-04 to 2026-11-09). It runs on the ESP32-2432S028 (2.8 inch) and the ESP32-3248S035 (3.5 inch).
- The board tools (`scripts/`), copied from Game-Maker: Sandbox Edition at commit `4e6b5e5a2`.
