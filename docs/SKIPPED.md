# What the engine still skips

Generated from a full headless boot on RotWK 2.01 + Edain + Völker 4.0 (`tools/boot.sh`, `BFME_INI_REPORT`). Each line is something the original game understands that this engine does not yet; counts are occurrences in the data.

| Kind | Distinct | Occurrences |
|---|---:|---:|
| module | 262 | 39334 |
| field | 1483 | 179878 |
| block | 44 | 9984 |
| value | 664 | 29594 |
| badfield | 16 | 2339 |
| strayEnd | 24 | 349 |
| missing | 2 | 2 |

## Module classes not implemented (unit behaviours, draws, bodies)

| Module | Uses |
|---|---|
| W3DScriptedModelDraw | 11330 |
| SpecialPowerModule | 3461 |
| AISpecialPowerUpdate | 2107 |
| AttributeModifierUpgrade | 1781 |
| AttributeModifierAuraUpdate | 1643 |
| StancesBehavior | 968 |
| LargeGroupAudioUpdate | 966 |
| WeaponFireSpecialAbilityUpdate | 939 |
| CastleMemberBehavior | 908 |
| BezierProjectileBehavior | 862 |
| GettingBuiltBehavior | 836 |
| RemoveUpgradeUpgrade | 746 |
| AutoAbilityBehavior | 709 |
| HordeContain | 706 |
| LevelUpUpgrade | 627 |
| EmotionTrackerUpdate | 583 |
| PassiveAreaEffectBehavior | 566 |
| W3DHordeModelDraw | 517 |
| W3DFloorDraw | 471 |
| GeometryUpgrade | 462 |
| HordeAIUpdate | 441 |
| HitReactionBehavior | 411 |
| HordeMemberCollide | 348 |
| BuildingBehavior | 333 |
| RespawnBody | 312 |
| ToggleMountedSpecialAbilityUpdate | 294 |
| RespawnUpdate | 263 |
| TerrainResourceBehavior | 253 |
| DoCommandUpgrade | 242 |
| LargeGroupBonusUpdate | 230 |
| WeaponModeSpecialPowerUpdate | 219 |
| NotifyTargetsOfImminentProbableCrushingUpdate | 190 |
| BannerCarrierUpdate | 178 |
| TemporarilyDefectUpdate | 173 |
| ReplaceSelfUpgrade | 172 |
| InvisibilityUpdate | 156 |
| HorseHordeContain | 156 |
| HeroModeSpecialAbilityUpdate | 152 |
| MonitorConditionUpdate | 143 |
| ExperienceLevelCreate | 134 |
| CastleBehavior | 129 |
| SiegeDockingBehavior | 126 |
| ActivateModuleSpecialPower | 124 |
| AttachUpdate | 115 |
| SlaveWatcherBehavior | 113 |
| FoundationAIUpdate | 112 |
| DualWeaponBehavior | 111 |
| W3DStreakDraw | 88 |
| AudioLoopUpgrade | 84 |
| ModelConditionSpecialAbilityUpdate | 83 |
| W3DBuffDraw | 73 |
| CastleUpgrade | 67 |
| CitadelSlaughterHordeContain | 65 |
| InheritUpgradeCreate | 63 |
| RadiateFearUpdate | 60 |
| PorcupineFormationBodyModule | 59 |
| PlayerHealSpecialPower | 59 |
| ArrowStormUpdate | 58 |
| ToggleHiddenSpecialAbilityUpdate | 55 |
| TooltipUpgrade | 54 |

…and 114 more.

## Top-level INI block types not implemented

| Block | Count |
|---|---|
| Behavior | 6677 |
| Multisound | 685 |
| LocomotorSet | 512 |
| FrameDelay | 332 |
| Geometry | 331 |
| Window | 330 |
| NewEvaEvent | 313 |
| Draw | 269 |
| AddEmotion | 156 |
| DebugCommandMap | 148 |
| FireOnce | 54 |
| Side | 38 |
| AmbientStream | 23 |
| PredefinedEvaEvent | 18 |
| Scale | 12 |
| Body | 12 |
| ReplaceModule | 9 |
| FormationPreviewItemDecal | 9 |
| AlwaysAfraidOf | 9 |
| AttackPriority | 8 |
| WeatherData | 5 |
| KindOf | 3 |
| FormationPreviewDecal | 3 |
| CamouflageDetectionMultiplier | 3 |
| StreamedSound | 2 |
| RadarPriority | 2 |
| ModelLOD | 2 |
| EvaEventForwardReference | 2 |
| AudioLOD | 2 |
| VoiceAttack | 1 |
| Update | 1 |
| ShroudClearingRange | 1 |
| Physics | 1 |
| MiscEvaData | 1 |
| MinimumFPS | 1 |
| FXParticleSystem | 1 |
| EvaEnemyObjectSightedEvent | 1 |
| EmissionVolume | 1 |
| EmissionVelocity | 1 |
| AutoResolveBody | 1 |

…and 4 more.

## Fields not implemented (by block)

| Block | Field | Count |
|---|---|---|
| CommandButton | InPalantir | 5240 |
| AudioEvent | VolumeSliderMultiplier | 3936 |
| Object | GeometryContactPoint | 3614 |
| Weapon | DamageNugget | 3543 |
| CommandButton | Radial | 3406 |
| Object | ThreatLevel | 2994 |
| Object | GeometryOffset | 2543 |
| Object | LocomotorSet | 2536 |
| Weapon | FiringDuration | 2514 |
| Object | UpgradeTexture | 2418 |
| Object | EvaEventDamagedOwner | 2046 |
| Object | ClientBehavior | 1831 |
| Object | MaxHealthDamaged | 1765 |
| Object | EvaEnemyObjectSightedEvent | 1759 |
| Object | AdditionalGeometry | 1720 |
| Object | Sound | 1540 |
| Object | CommandPoints | 1446 |
| Weapon | MeleeWeapon | 1419 |
| Object | DeathFlags | 1394 |
| CommandButton | AutoAbility | 1379 |
| Object | GeometryName | 1362 |
| Object | EvaEventAmbushed | 1335 |
| Object | EvaEventDamagedByFireOwner | 1335 |
| Object | EvaEventDamagedFromShroudedSourceOwner | 1335 |
| Object | EvaEventSecondDamageFarFromFirstOwner | 1335 |
| Object | EvaEventSecondDamageFarFromFirstScanRange | 1335 |
| Object | EvaEventSecondDamageFarFromFirstTimeoutMS | 1335 |
| Object | BountyValue | 1252 |
| Object | GravityMult | 1201 |
| ObjectCreationList | IgnoreCommandPointLimit | 1197 |
| Weapon | ProjectileNugget | 1179 |
| Object | Delay | 1176 |
| Object | VisionBonusPercentPerFoot | 1150 |
| Object | MaxHealthReallyDamaged | 1138 |
| Object | VisionBonusTestRadius | 1093 |
| Object | MaxVisionBonusPercent | 1092 |
| ObjectCreationList | DispositionAngle | 1019 |
| Object | AILuaEventsList | 995 |
| Object | VoiceFullyCreated | 961 |
| Object | VoicePriority | 946 |
| Weapon | HitPercentage | 930 |
| Object | SoundImpact | 928 |
| CommandButton | NeededUpgrade | 898 |
| Object | Description | 898 |
| Object | VisionRear | 892 |
| Object | VisionSide | 892 |
| Object | ThingClass | 888 |
| Object | RankInfo | 887 |
| Object | DamageCreationList | 882 |
| CommandButton | ShowProductionCount | 870 |
| Weapon | AttributeModifierNugget | 858 |
| Object | VoiceSelectBattle | 841 |
| Object | VoiceAttackCharge | 831 |
| Object | VoiceAttackStructure | 816 |
| Weapon | AntiAirborneMonster | 809 |
| ChildObject | RankInfo | 791 |
| Object | VoiceMoveToCamp | 780 |
| Object | VoiceAttackMachine | 773 |
| Object | DescriptionStrategic | 771 |
| Object | ThingToSpawn | 769 |

…and 1423 more.

## Fields whose value format differs (parse errors)

| Block | Field | Count |
|---|---|---|
| CommandButton | Command | 1535 |
| Object | FXList | 543 |
| Object | Weapon | 82 |
| Object | AutoChooseSources | 50 |
| Object | TransitionState | 38 |
| DamageFX | MajorFX | 31 |
| DamageFX | AmountForMajorFX | 31 |
| Object | CrushableLevel | 8 |
| DamageFX | MinorFX | 5 |
| Object | FX | 4 |
| ChildObject | Weapon | 4 |
| ChildObject | FXList | 3 |
| ChildObject | AutoChooseSources | 2 |
| Locomotor | CanMoveBackwards | 1 |
| ChildObject | TransitionState | 1 |
| ChildObject | FX | 1 |

## Enum/flag values not known (in block)

| Block | Value | Count |
|---|---|---|
| CommandButton | CANCELABLE | 1866 |
| Object | UNIT | 1723 |
| Object | NOT_AUTOACQUIRABLE | 1281 |
| Object | PATH_THROUGH_EACH_OTHER | 970 |
| CommandButton | NONPRESSABLE | 936 |
| Object | IGNORE_FOR_VICTORY | 783 |
| Object | ARMY_SUMMARY | 754 |
| Object | THROWN_OBJECT | 603 |
| CommandButton | TrainingRadiusCursor | 533 |
| Object | GRAB_AND_DROP | 505 |
| ObjectCreationList | ABSOLUTE_ANGLE | 503 |
| Object | INERT_SHROUD_REVEALER | 500 |
| Weapon | NEUTRAL | 474 |
| Object | HORDE | 452 |
| ObjectCreationList | BUILDING_CHUNKS | 436 |
| Object | OPTIMIZED_PROP | 421 |
| Object | MADE_OF_STONE | 408 |
| Object | CHUNK_VENDOR | 408 |
| Object | NEED_BASE_FOUNDATION | 382 |
| Object | LARGE_RECTANGLE_PATHFIND | 375 |
| Object | MELEE_HORDE | 368 |
| Object | HEAVY_MELEE_HITTER | 362 |
| Object | DEATH_4 | 345 |
| CommandButton | TOGGLE_IMAGE_ON_WEAPONSET | 324 |
| Object | CASTLE_KEEP | 286 |
| CommandButton | HIDE_WHILE_DISABLED | 264 |
| Weapon | PER_POSITION | 258 |
| CommandButton | OK_FOR_MULTI_EXECUTE | 251 |
| Object | WEAPONSET_TOGGLE_1 | 245 |
| ChildObject | SUMMONED | 243 |
| Object | NOTIFY_OF_PREATTACK | 238 |
| ObjectCreationList | RELATIVE_ANGLE | 232 |
| Object | IGNORE_FOR_EVA_SPEECH_POSITION | 231 |
| Object | CAVALRY | 231 |
| Object | CAN_USE_SIEGE_TOWER | 230 |
| Object | PLAYER_UPGRADE_2 | 222 |
| Object | ARCHER | 217 |
| Object | DONT_HIDE_IF_FOGGED | 214 |
| ObjectCreationList | SPAWN_AROUND | 211 |
| CommandButton | ON_GROUND_ONLY | 209 |

…and 624 more.
