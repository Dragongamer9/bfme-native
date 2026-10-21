# Progress

## Target
- Game: BFME2 Rise of the Witch-king (Windows install, being installed via the All-in-One launcher)
- Content: Edain + Völker submod, built in as the default data
- Output: native 64-bit Linux executable, fewer engine limits; Windows 64-bit later
- Engine base: GeneralsX (GPL-3.0); the whole project is GPL-3.0

## Session 1 — 2026-10-21
Done:
- Toolchain that needs only git (no pip/apt): capstone 5.0.6 built from source, pefile from source
- `generals_symbols.py`: from Core + GeneralsMD it extracted 227 FieldParse tables (2,013 fields), the 62-entry INI block table, and 204 modules
- `scan_tables.py`, `scan_modules.py`, `merge_labels.py`: verified on a synthetic 32-bit MSVC-ABI PE (`tests/fake_sage.cpp`). They found the tables, matched classes, named parser functions with conflict rejection, found the module registry including a non-Generals module, and recovered constructors, vtables and unique vtable slots
- `bigtool.py`: BIGF/BIG4 list, extract, pack and overlay; round-trip verified
- `ini_usage.py`: scope report of modules and fields used by the mod
- `analyze.sh`: end-to-end pipeline, tested on fake game and mod directories
- `ApplyBfmeLabels.java`: Ghidra label importer (not yet run in Ghidra)

Blocked on: the game install finishing; then folder access on the Windows PC.

## Next
1. Get RotWK, BFME2, Edain and Völker directories; stage the exe (`game.dat`) and the .big files
2. Run `analyze.sh`; review the match rates and tune the scanners against the real binary
3. Scope report: which module classes Edain uses that GeneralsX lacks, ranked by use count
4. Fork GeneralsX into `engine/` as the RotWK target and get it booting to the point of loading RotWK `.big`, INI and W3D, with gameplay stubbed
5. Port modules in scope order: horde (HordeContain etc.), build plots / castles, heroes, powers, then AI
