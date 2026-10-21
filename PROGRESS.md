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

## Milestone: full engine init on RotWK + Edain + Völker (2026-10-21, overnight)
The native 64-bit Linux engine (headless) now loads all 1,339 INI files of RotWK 2.01 + Edain +
Völker 4.0, initializes every subsystem (logic, client, UI data, audio, command maps) and reaches
the main loop. Run it with `tools/boot.sh <dir with the .big files>`.

RotWK behaviour reimplemented on the way (decompiled from game.dat / WorldBuilder):
- INI preprocessor: #include, #define (case-insensitive), #ADD/#SUBTRACT/#MULTIPLY/#DIVIDE
- data-driven SubsystemLegend; BIG4 archives; lang\*.big localized archives
- science prerequisites (OR of AND groups); terrain classes; audio types/control/submix
- damage and death types in RotWK order; ClipReloadTime Min/Max
- ChildObject inheritance (tag-based module replacement); CommandSets inside object files
- limits raised: upgrades 512 -> 2048
- tolerant loading: unknown blocks, fields, values, modules are skipped and listed in a report
  (BFME_INI_REPORT), BFME_INI_STRICT=1 restores aborting

Test environment notes: the container has no GPU; SwiftShader lacks VK_EXT_robustness2 needed by
DXVK, so automated boots use the engine's -headless mode. On a real GPU run without -headless.

## Session 2 — gameplay data (2026-10-21, overnight, cont.)
Zero bad INI fields left except 14 edge cases (was 2,353). Added, each from game.dat tables/decompiles:
- W3DScriptedModelDraw: AnimationState / IdleAnimationState / TransitionState, permissive
  ModelConditionState (RotWK rules: any order, later duplicates ignored, model inherited)
- RotWK vocabularies: 591 model conditions, 222 KindOfs, 5 weapon slots (QUATERNARY, QUINARY),
  RotWK GUI commands, DamageFXType (DamageFX is keyed by it), ALMOST_FINAL collapse phase
- ObjectFilter (`ANY +INFANTRY -HERO ENEMIES`, ALL/ANY/NONE, ALLIES/ENEMIES/NEUTRAL/SAME_PLAYER,
  EVIL/GOOD) -- used by weapons, auras, special powers
- Weapon nuggets: DamageNugget (radius, min radius, arc, height limits, taper, DamageScalar filters,
  force-kill filter, drain life), DOTNugget, ProjectileNugget + warhead weapons, MetaImpactNugget
  (knockback), WeaponOCLNugget, ParalyzeNugget, AttributeModifierNugget
- Attribute modifiers: ModifierList store, per-object timed bonuses (armor with the 75% cap,
  damage add/mult, spell damage, speed, health, invulnerability, model conditions, FX), category
  replacement; modules AttributeModifierUpgrade and AttributeModifierAuraUpdate

Can't test in-game yet: the container only has the INI archives. Art, maps and audio archives are
on the PC (C:\RotWK) and are needed for map loading and rendering tests.

## Session 3 — battles, economy and heroes (2026-10-21)
Headless battle tests run on a generated flat map (`tools/mkmap.py`, `tools/logictest.sh`,
scripts in `tests/logic/`). Working now:
- hordes (formation, attack hand-off, horde dies with its last member), archers, mixed armies,
  self-building structures (GettingBuiltBehavior)
- economy: TerrainResourceBehavior (farm income, shared land, upgrade bonus)
- heroes: AutoAbilityBehavior (units use abilities by themselves), WeaponFireSpecialAbilityUpdate,
  WeaponModeSpecialPowerUpdate, HeroModeSpecialAbilityUpdate, ToggleMountedSpecialAbilityUpdate
  (mount/dismount and ring-hero forms), DoCommandUpgrade, RespawnUpdate (fall, revive, auto-revive)
- RotWK experience: ExperienceLevels.ini ranks 1-10 with their upgrades and attribute modifiers,
  ExperienceLevelCreate, LevelUpUpgrade on ranks
- parser fix: `AddEmotion = OVERRIDE ... End` sub-blocks inside not-yet-ported modules were cutting
  the rest of ~1,900 module lines out of horde and hero objects

## Next
1. Read the skip report: rank unimplemented modules/fields by how much of Edain they affect
2. W3DScriptedModelDraw (model/animation states) so units render - needs a GPU test
3. Port gameplay modules in docs/SCOPE.md order (hordes, special powers, attribute modifiers...)
4. Map loading for RotWK maps, then the APT (Flash) UI
