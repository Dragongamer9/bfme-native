# bfme-native

A native 64-bit Linux build of **The Lord of the Rings: The Battle for Middle-earth II — Rise of the Witch-king**, running **Edain + the Völker submod** as its built-in content.

Personal project. It needs your own installed copy of the game; no EA files are stored here.

## Playing

```bash
git clone -b main https://github.com/Dragongamer9/bfme-native && cd bfme-native
./play.sh build                                   # compile the engine (once; needs cmake, ninja, a C++20 compiler)
./play.sh setup "/path/to/RotWK" "/path/to/BFME2" # link your game files (Edain + Völker .big files inside RotWK)
./play.sh --map "map mp umbar" --you Men --ai Mordor
./play.sh maps        # skirmish maps
./play.sh --size 1920x1080 --fullscreen           # window size (default 1280x720) / fullscreen
```

A 1v1 skirmish against a computer opponent. Any of the 27 Edain/Völker sides works for either
player (`./play.sh factions`). Maps with Edain camp flags (Umbar, Harlindon, The Heubris, Fall Back
4p horde...) start you in your faction's camp: select the keep for heroes (its hero page), the
build plots for buildings, and take the economy plots around the map with your units. Maps
without flags start in free build with porters. Esc opens the menu.

What works and what doesn't yet is in `PROGRESS.md`.

## Approach

RotWK runs on EA's SAGE engine, a descendant of the Command & Conquer: Generals engine whose source EA released under GPL-3.0. [GeneralsX](https://github.com/fbraz3/GeneralsX) has already ported that code to native 64-bit Linux (SDL3, DXVK/Vulkan, OpenAL/miniaudio). This project:

1. **Maps the RotWK executable** against the Generals source, automatically, using structures that survive into retail builds (INI parse tables, the module registry, vtables).
2. **Measures what Edain + submod actually use**: every module class and INI field. That list is the port's scope, so engine features the mod never touches can wait.
3. **Evolves GeneralsX into the RotWK engine**, one system at a time, using the decompiled RotWK code as the reference for behaviour.

## Layout

| Path | What |
|---|---|
| `setup.sh` | One-time: builds capstone, fetches pefile and GeneralsX, extracts Generals reference symbols |
| `analyze.sh <game> [mods…]` | Full pass: scans the exe, merges labels, extracts all INI in load order, writes the scope report |
| `tools/generals_symbols.py` | Generals source → FieldParse/BlockParse tables + module list |
| `tools/scan_tables.py` | Finds every INI parse table in the exe by shape, matches classes, names parser functions, records field offsets, lists BFME-only fields |
| `tools/scan_modules.py` | Recovers `ModuleFactory::init`: every module's name, factories, constructor and vtable, including BFME-only modules |
| `tools/merge_labels.py` | Combines all labels → `data/labels.csv` |
| `tools/bigtool.py` | List, extract, pack and overlay `.big` archives (BIGF/BIG4) |
| `tools/ini_usage.py` | Module/field usage across the mod's INI, cross-referenced with Generals and the RotWK exe |
| `ghidra_scripts/ApplyBfmeLabels.java` | Imports `labels.csv` into a Ghidra project |
| `PROGRESS.md` | Session log and next steps |

## Running

```bash
./setup.sh
./analyze.sh "/path/to/RotWK" "/path/to/BFME2" "/path/to/Edain" "/path/to/Voelker"
```

Then in Ghidra, import the game exe (x86:LE:32, Visual Studio), run auto-analysis, and run `ApplyBfmeLabels.java` on `data/labels.csv`.
