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

## Session 4 — the game renders (2026-10-22)
Rendering under SDL offscreen + Mesa lavapipe, screenshots in `docs/screens/`.
- Base BFME2 art fetched from the user's PC (terrain, textures0-4, w3d) as `zz_bfme2_*`.
- RotWK weather (rain via the snow system: IsSnowing, SnowBoxHeight/Spacing/Speed).
- Terrain draw window centred with 16-bit heights; 128px tiles (225 texture classes).
- W3D: FX shader materials (0x50-0x53: DiffuseTexture/Texture_0, emissive, alpha),
  generated blob decal shadows (shadow/shadowI).
- Animations: BFME motion channels (0x284, time-coded and 4/8-bit adaptive delta) rebuilt
  as raw anims, file-only anim names, bone-name binding across skeletons.
- Strings: data/lotr.str. Water: StandingWaterAreas / RiverAreas chunks -> water triggers.
- Fixes: quit crash (cameo movie window), screenshot after final composite, reflection
  overlay off by default.
- Harness: `reveal`, `spawn ... @dx @dy` (relative to the view), `camera X Y zoom pitch`.
Known: sky above the horizon is black (reflection-only dome), pale squares under Uruk-hai,
TriggerAreas chunk format still unknown (empty on Fords of Isen).

## Session 5 — the APT interface (2026-10-22)
RotWK's UI is EA "APT" (binary Flash). A runtime for it now lives in the engine
(`GameEngineDevice/.../Apt/`): movie loader (.apt/.const/.dat/_geometry), ActionScript VM
(AVM1 + EA opcodes; validated on all 2396 action blocks by `tools/aptdis.py --check`),
timeline player, builtins (MovieClip, Array, String, Math, Color, Mouse, setInterval,
`extern`), Render2DClass/DisplayString renderer, and the game bridge (`BfmeAptUI.cpp`).
- Palantir.apt runs in every game (BFME_NO_APT to disable) with the Edain/Völker movies,
  sub-movies (spell book, hero bar, help box, planning mode, side bar) load through
  loadMovie.
- Bridge: _global.InGame, extern values, game strings ($PalantirResources ...), palantir
  good/evil frame, minimap (W3DRadar in the globe, click to move the view), resources and
  command points, command ring (CommandSet slots 1-6 -> CommandButton content with icons,
  states, click runs the command via ControlBar::bfmeExecuteCommand), tooltips in the help
  box, hero bar (portraits, health, selection, click selects), spell book (in progress).
- APT semantics learnt the hard way: SetStringVar/Member's inline string is the value;
  a goto to the current frame re-runs its script; attachMovie runs frame 1 at once; plain
  shapes don't block buttons below; geometry image ids are characters (imported images
  come from the exporting movie).
- Tools: aptview.py (PNG preview), aptdis.py (disassembler/validator), bigls.py,
  bigextract.py (game priority). Debug: BFME_DEBUG_APT(_CALLS/_TEXT/_IMG/_MOUSE),
  BFME_APT_TREE=<frame>, BFME_APT_DUMP, BFME_APT_CALLS="path(args);...".
- Harness: click/rclick/mousemove X Y, select PLAYER TEMPLATE, spellbook, science,
  sciencepoints; 'players' marks the local player; the mouse is parked mid-screen.
- Tests: tests/logic/render_apt*.txt (map arg: Maps/FordsIsen.map).

## Session 6 — playable skirmish (2026-10-22)
- Spells: spell books join AI groups (no AI module), RotWK power types may target any spot or
  object; spell column recharge clocks. Hero abilities from the ring (Boromir's horn).
- Heroes no longer die on spawn: LifetimeUpdate WaitForWakeUp/DeathType; RotWK bodies
  (DelayedDeathBody, ...) map to ActiveBody/StructureBody; AvailableAtStart defaults to yes.
- ModelConditionSpecialAbilityUpdate (SPECIAL_POWER_n abilities); ring _notReady + clocks.
- Skirmish: `-skirmish <map> <side> <side>` (tools/gdbskirmish.sh); GameData
  VictoryCondition*ObjectFilter + grace period; DefaultStartingCash; Edain free build
  (Upgrade_EvilDualEconomyChoice) when the start has no base site -> porter eggs hatch.
- ObjectCreationUpgrade (ThingToSpawn/Offset/Delay/GrantUpgrade/RemoveUpgrade), SpawnBehavior
  upgrade triggers, re-entrant upgrade grants, ObjectFilter "ANY -X".
- Locomotor TurnTime (RotWK has no TurnRate: wheeled units could not turn).
- Rendering: AnimationStates only apply when all their flags are set — finished buildings were
  stuck in their sunk construction animation; build-up animation follows construction %;
  "tex.tga.001" names.
- UI: side command bar (slots 7-18) with porter build menus; quit menu (QuitMenu.apt) on Esc /
  Options; BfmeSkirmishAI: a computer opponent driven by SkirmishAIData.ini (AIDozerAssignment,
  ArmyDefinition): builds farms and producers, trains by army percentages, attacks in waves.
- MonitorConditionUpdate (command set pages/modes).
- Tools: bseinfo.py, mapobjects.py, modfields.py, fieldparse.py (find RotWK INI fields in
  game.dat), w3dchunks.py. Debug: BFME_DEBUG_{DEATH,VICTORY,AICMD,MESH,FX,SKIRMISH_AI},
  BFME_DEBUG_SCENE=<frame>, harness conditions/quitmenu.
Known: sky black above the horizon; castle/camp sites (CastleBehavior + bases/*.bse) not unpacked;
123 RotWK module types still missing (tools/modfields.py, /tmp list in session notes).

## Session 7 — camps, heroes and the Völker maps (2026-10-22)
- Victory/defeat banner (GuiFX ShowEndGame). Building floors (W3DFloorDraw, HideIfModelConditions).
- Hordes: banner carriers at rank 2 (BannerCarriersAllowed/Position) bring fallen soldiers back out
  of melee (BannerCarrierUpdate timings); ReplenishNearbyHorde buildings refill hordes.
- Map scripts: RotWK Script v3/v4 and nested ScriptGroups parse (Völker maps loaded with no
  objects before); conditions with extra parameters duplicate safely. tools/mapscripts.py dumps them.
- Skirmish players are Player_N (start position N) and take over the map's Player_N side.
- Camps and castles: CastleBehavior claims the site on a start position, unpacks the faction base
  from bases/<name>/<name>.bse (start site free, later sites paid via CASTLE_UNPACK); build plots
  (FOUNDATION_CONSTRUCT) raise their building by themselves and return when it falls. OCL
  Required/ForbiddenUpgrades. AdditionalGeometry no longer clobbers the main geometry (keeps were
  zero-sized and stayed shrouded).
- Heroes: REVIVE slots on keeps/barracks recruit or revive the side's BuildableHeroesMP heroes;
  command pages (InitialVisible, PUSH/POP_VISIBLE_COMMAND_RANGE); command sets up to 36 buttons.
- AI: AISpecialPowerUpdate (computer units use buffs/heals/killers/area powers), builds on camp
  plots, spreads farms apart.
- Capture flags (economy plots, outposts) are taken by standing at them uncontested for 3 s;
  CASTLE_UNPACK_EXPLICIT_OBJECT builds the chosen farm/outpost on them. Captured objects re-run
  their upgrade modules (faction command sets).
- Modules: GateOpenAndCloseBehavior (gates never block paths, open for friends), CastleUpgrade,
  InvisibilityUpdate (stealth/camouflage; no tree test, so forest camouflage stays off),
  W3DSailModelDraw, ShipSlowDeathBehavior. Self-named animation files no longer recurse forever;
  maps with more terrain blends than our table load (Dagorlad).
- Völker: all 27 playable sides start (tools/factionsweep.sh); Völker computer players no longer
  turn human (no skirmish side in the map) and train every unit of their side when they have no
  ArmyDefinition. Missing camp/castle bases fall back castle↔camp, then to free build.
- Audio: the user's upload has no BFME2 audio.big/speech/music, so most sounds are missing here;
  play.sh setup links them from a real BFME2 install. BFME_DEBUG_AUDIO lists played/missing files.
- AI: AISpecialPowerUpdate (computer units use buffs/heals/killers/area powers), builds on camp
  plots and economy plots, takes free plots, recruits heroes, spreads farms apart, and no longer
  pays twice for units and buildings (its economy was half speed).
- play.sh (build / setup / launch a skirmish; default map Umbar), tools/factionsweep.sh and
  tools/mapsweep.sh (every faction and every map starts).
- Abilities: ActivateModuleSpecialPower (an ability that fires other special power modules of the
  unit by tag, at the target or itself: Théoden's "Stunde des Ruhms" and many Edain hero powers) and
  DominateEnemySpecialPower (Saruman, Wormtongue, Sauron, attack trolls... take enemies near the
  target, whole hordes at once; temporary conversions return after 20 s).
  ArrowStormUpdate (Legolas, Nar, Óin... volleys at every enemy around the target) and
  ToggleHiddenSpecialAbilityUpdate (Elven Cloak, spirit form: switch a unit's stealth on and off).
  Location-less abilities with a view object no longer crash.
- Command points: CommandPointsUpgrade (bought upgrades that raise the limit), COMMAND_POINT_BONUS
  attribute modifiers, and the AI buys Edain's CP "economy units" at its keep when its army is at
  the limit and it has money left over (before, it stopped at 500 and hoarded gold).
- play.sh launches at 1280x720 by default (--size WxH, --fullscreen).
Known: the user's data has no base files for several factions (no fb_fortress_*, rohan_*,
zwerge_*, lothlorien_* camps/castles...): those starts fall back to camp → free build, which has
no keep (heroes then come from barracks hero slots). GeometryUpgrade and wall building are not
implemented.

## Next
1. Read the skip report: rank unimplemented modules/fields by how much of Edain they affect
2. W3DScriptedModelDraw (model/animation states) so units render - needs a GPU test
3. Port gameplay modules in docs/SCOPE.md order (hordes, special powers, attribute modifiers...)
4. Spell store (purchase screen), side command bar (slots 7+), stances/formations logic
5. Shell: main menu / skirmish setup in APT
