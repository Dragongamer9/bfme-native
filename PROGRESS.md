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

## Session 1 (cont.)
Analysis of the real install (C:\RotWK: RotWK 2.01 + Edain + Völker 4.0):
- game.dat: 330 module classes recovered (202 BFME-only), 478 INI tables, 69 INI block keywords; Ghidra project with 68,865 functions, 3,576 auto-labels applied
- docs/SCOPE.md: Edain+Völker use 258 module classes, 81 already in Generals code, 177 to write; ranked list
- WorldBuilder.exe carries ~2,200 Class::method strings + ~1,500 source paths -> being analysed to carry names into game.dat (tools/name_from_strings.py, tools/match_functions.py)

Engine (engine/GeneralsX, branch bfme-rotwk):
- INIPreprocessor: #include / #define / macro substitution / #ADD #SUBTRACT #MULTIPLY #DIVIDE, reimplemented from game.dat 0x00a159aa, 0x0042cfc9, 0x0042ddd1 (validated on all 1,692 mod INI: every include resolves, 12,106 macros, no conflicts)
- BIG4 archives accepted
- SubsystemLegend: RotWK's data-driven subsystem INI lists replace the compiled-in paths
- Native Linux build of the base engine in progress (deps built from source: SDL3, DXVK, glm 1.0.1, gli, curl, miniaudio)

## Next
1. Finish native build; boot with RotWK data to the first INI error and work down the list
2. Generate tolerant field tables for all 478 RotWK INI tables (unknown fields parsed and stored, not fatal)
3. W3DModelDraw -> W3DScriptedModelDraw: Model/Animation state schemas recovered (tables at 0xBE0A78, 0xD99F60)
4. Port modules in docs/SCOPE.md order
