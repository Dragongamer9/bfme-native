# What the engine still skips

Generated from a full headless boot on RotWK 2.01 + Edain + Völker 4.0 (`tools/boot.sh`, `BFME_INI_REPORT`). Each line is something the original game understands that this engine does not yet; counts are occurrences in the data.

| Kind | Distinct | Occurrences |
|---|---:|---:|
| module | 266 | 44309 |
| field | 1406 | 176795 |
| block | 26 | 1929 |
| value | 665 | 29827 |
| badfield | 16 | 2406 |
| strayEnd | 3 | 23 |
| missing | 2 | 2 |

## Module classes not implemented (unit behaviours, draws, bodies)

| Module | Uses |
|---|---|
| W3DScriptedModelDraw | 11600 |
| SpecialPowerModule | 4136 |
| AISpecialPowerUpdate | 3627 |
| AttributeModifierUpgrade | 1900 |
| AttributeModifierAuraUpdate | 1791 |
| WeaponFireSpecialAbilityUpdate | 1101 |
| StancesBehavior | 995 |
| LargeGroupAudioUpdate | 991 |
| LevelUpUpgrade | 978 |
| EmotionTrackerUpdate | 955 |
| CastleMemberBehavior | 908 |
| BezierProjectileBehavior | 889 |
| GettingBuiltBehavior | 836 |
| AutoAbilityBehavior | 815 |
| RemoveUpgradeUpgrade | 746 |
| HordeContain | 706 |
| PassiveAreaEffectBehavior | 568 |
| W3DHordeModelDraw | 517 |
| W3DFloorDraw | 471 |
| GeometryUpgrade | 462 |
| HitReactionBehavior | 442 |
| HordeAIUpdate | 441 |
| PickupStuffUpdate | 387 |
| ToggleMountedSpecialAbilityUpdate | 366 |
| HordeMemberCollide | 348 |
| WeaponModeSpecialPowerUpdate | 334 |
| BuildingBehavior | 333 |
| LargeGroupBonusUpdate | 332 |
| RespawnBody | 317 |
| DoCommandUpgrade | 299 |
| RespawnUpdate | 268 |
| TerrainResourceBehavior | 253 |
| InvisibilityUpdate | 209 |
| NotifyTargetsOfImminentProbableCrushingUpdate | 201 |
| MonitorConditionUpdate | 183 |
| TemporarilyDefectUpdate | 182 |
| BannerCarrierUpdate | 179 |
| ReplaceSelfUpgrade | 173 |
| HeroModeSpecialAbilityUpdate | 158 |
| HorseHordeContain | 156 |
| ActivateModuleSpecialPower | 144 |
| ExperienceLevelCreate | 134 |
| CastleBehavior | 129 |
| SiegeDockingBehavior | 126 |
| HordeNotifyTargetsOfImminentProbableCrushingUpdate | 121 |
| AttachUpdate | 115 |
| SlaveWatcherBehavior | 114 |
| FoundationAIUpdate | 112 |
| DualWeaponBehavior | 112 |
| ModelConditionSpecialAbilityUpdate | 92 |
| W3DStreakDraw | 88 |
| AudioLoopUpgrade | 84 |
| W3DBuffDraw | 73 |
| RadiateFearUpdate | 72 |
| PlayerHealSpecialPower | 67 |
| CastleUpgrade | 67 |
| CitadelSlaughterHordeContain | 66 |
| ToggleHiddenSpecialAbilityUpdate | 65 |
| InheritUpgradeCreate | 63 |
| PorcupineFormationBodyModule | 59 |

…and 115 more.

## Top-level INI block types not implemented

| Block | Count |
|---|---|
| Multisound | 685 |
| FrameDelay | 332 |
| Window | 330 |
| NewEvaEvent | 313 |
| DebugCommandMap | 148 |
| FireOnce | 54 |
| AmbientStream | 23 |
| PredefinedEvaEvent | 18 |
| WeatherData | 5 |
| StreamedSound | 2 |
| ModelLOD | 2 |
| EvaEventForwardReference | 2 |
| AudioLOD | 2 |
| Update | 1 |
| Physics | 1 |
| MiscEvaData | 1 |
| MinimumFPS | 1 |
| FXParticleSystem | 1 |
| EmissionVolume | 1 |
| EmissionVelocity | 1 |
| Draw | 1 |
| Behavior | 1 |
| AudioLowMHz | 1 |
| AnimationSoundClientBehaviorGlobalSetting | 1 |
| Angle | 1 |
| Alpha | 1 |

## Fields not implemented (by block)

| Block | Field | Count |
|---|---|---|
| CommandButton | InPalantir | 5240 |
| AudioEvent | VolumeSliderMultiplier | 3936 |
| Object | GeometryContactPoint | 3614 |
| Weapon | DamageNugget | 3543 |
| CommandButton | Radial | 3406 |
| Object | LocomotorSet | 3097 |
| Object | ThreatLevel | 3017 |
| Object | GeometryOffset | 2554 |
| Weapon | FiringDuration | 2514 |
| Object | UpgradeTexture | 2445 |
| Object | EvaEventDamagedOwner | 2096 |
| Object | ClientBehavior | 1892 |
| Object | MaxHealthDamaged | 1809 |
| Object | EvaEnemyObjectSightedEvent | 1796 |
| Object | AdditionalGeometry | 1724 |
| Object | Sound | 1631 |
| Object | GravityMult | 1615 |
| Object | CommandPoints | 1484 |
| Object | DeathFlags | 1441 |
| Weapon | MeleeWeapon | 1419 |
| CommandButton | AutoAbility | 1379 |
| Object | EvaEventAmbushed | 1371 |
| Object | EvaEventDamagedByFireOwner | 1371 |
| Object | EvaEventDamagedFromShroudedSourceOwner | 1371 |
| Object | EvaEventSecondDamageFarFromFirstOwner | 1371 |
| Object | EvaEventSecondDamageFarFromFirstScanRange | 1371 |
| Object | EvaEventSecondDamageFarFromFirstTimeoutMS | 1371 |
| Object | GeometryName | 1362 |
| Object | BountyValue | 1289 |
| Object | Delay | 1287 |
| ObjectCreationList | IgnoreCommandPointLimit | 1197 |
| Weapon | ProjectileNugget | 1179 |
| Object | VisionBonusPercentPerFoot | 1172 |
| Object | MaxHealthReallyDamaged | 1138 |
| Object | VisionBonusTestRadius | 1115 |
| Object | MaxVisionBonusPercent | 1114 |
| Object | AutoResolveWeapon | 1059 |
| Object | AILuaEventsList | 1032 |
| ObjectCreationList | DispositionAngle | 1019 |
| Object | VoiceFullyCreated | 985 |
| Object | VoicePriority | 983 |
| Object | AutoResolveArmor | 977 |
| Object | SoundImpact | 958 |
| Weapon | HitPercentage | 930 |
| Object | ThingClass | 926 |
| Object | VisionRear | 911 |
| Object | VisionSide | 911 |
| Object | Description | 907 |
| CommandButton | NeededUpgrade | 898 |
| Object | GiveNoXP | 896 |
| Object | DamageCreationList | 882 |
| CommandButton | ShowProductionCount | 870 |
| Object | VoiceSelectBattle | 869 |
| Weapon | AttributeModifierNugget | 858 |
| Object | VoiceAttackCharge | 837 |
| Object | VoiceAttackStructure | 828 |
| Object | VoiceMoveToCamp | 815 |
| Weapon | AntiAirborneMonster | 809 |
| Object | VoiceAttackMachine | 783 |
| Object | DescriptionStrategic | 780 |

…and 1346 more.

## Fields whose value format differs (parse errors)

| Block | Field | Count |
|---|---|---|
| CommandButton | Command | 1535 |
| Object | FXList | 543 |
| Object | TransitionState | 103 |
| Object | Weapon | 82 |
| Object | AutoChooseSources | 50 |
| DamageFX | MajorFX | 31 |
| DamageFX | AmountForMajorFX | 31 |
| Object | CrushableLevel | 8 |
| DamageFX | MinorFX | 5 |
| Object | FX | 4 |
| ChildObject | Weapon | 4 |
| ChildObject | TransitionState | 3 |
| ChildObject | FXList | 3 |
| ChildObject | AutoChooseSources | 2 |
| Locomotor | CanMoveBackwards | 1 |
| ChildObject | FX | 1 |

## Enum/flag values not known (in block)

| Block | Value | Count |
|---|---|---|
| CommandButton | CANCELABLE | 1866 |
| Object | UNIT | 1761 |
| Object | NOT_AUTOACQUIRABLE | 1281 |
| Object | PATH_THROUGH_EACH_OTHER | 984 |
| CommandButton | NONPRESSABLE | 936 |
| Object | IGNORE_FOR_VICTORY | 783 |
| Object | ARMY_SUMMARY | 775 |
| Object | THROWN_OBJECT | 617 |
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
| Object | HEAVY_MELEE_HITTER | 370 |
| Object | MELEE_HORDE | 368 |
| Object | DEATH_4 | 345 |
| CommandButton | TOGGLE_IMAGE_ON_WEAPONSET | 324 |
| Object | CASTLE_KEEP | 286 |
| CommandButton | HIDE_WHILE_DISABLED | 264 |
| Weapon | PER_POSITION | 258 |
| CommandButton | OK_FOR_MULTI_EXECUTE | 251 |
| Object | CAVALRY | 248 |
| ChildObject | SUMMONED | 246 |
| Object | WEAPONSET_TOGGLE_1 | 245 |
| Object | NOTIFY_OF_PREATTACK | 238 |
| ObjectCreationList | RELATIVE_ANGLE | 232 |
| Object | IGNORE_FOR_EVA_SPEECH_POSITION | 231 |
| Object | CAN_USE_SIEGE_TOWER | 230 |
| Object | PLAYER_UPGRADE_2 | 222 |
| Object | ARCHER | 217 |
| Object | DONT_HIDE_IF_FOGGED | 214 |
| ObjectCreationList | SPAWN_AROUND | 211 |
| CommandButton | ON_GROUND_ONLY | 209 |

…and 625 more.
