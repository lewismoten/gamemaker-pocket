# Changelog

The firmware has no release number of its own. Every build bakes in its git hash and date (`fwid.py`), and the serial command `FW` prints them (`FW board=C git=1a2b3c4 date=2026-11-09 env=cyd-2432s028`). This changelog starts at 0.1.0, the first version of this repository.

## 0.1.0 - 2026-11-09

### Added

- First release of this repository: the Pocket Player firmware split out, with its history, from [Game-Maker: Sandbox Edition](https://github.com/lewismoten/GameMaker-SandboxEdition) (`hardware/pocket-player/`, 218 commits from 2026-11-04 to 2026-11-09). It runs on the ESP32-2432S028 (2.8 inch) and the ESP32-3248S035 (3.5 inch).
- The board tools (`scripts/`), copied from Game-Maker: Sandbox Edition at commit `4e6b5e5a2`.
