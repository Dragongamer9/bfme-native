# What the engine still skips

Generated from a full headless boot on RotWK 2.01 + Edain + Völker 4.0 (`tools/boot.sh`, `BFME_INI_REPORT`). Each line is something the original game understands that this engine does not yet; counts are occurrences in the data.

| Kind | Distinct | Occurrences |
|---|---:|---:|
| module | 221 | 18441 |
| field | 1633 | 255883 |
| block | 26 | 1929 |
| value | 487 | 13668 |
| badfield | 0 | 0 |
| strayEnd | 3 | 23 |
| missing | 2 | 2 |

## Module classes not implemented (unit behaviours, draws, bodies)

| Module | Uses |
|---|---|
| AISpecialPowerUpdate | 3627 |
| WeaponFireSpecialAbilityUpdate | 1101 |
| StancesBehavior | 995 |
| LargeGroupAudioUpdate | 991 |
| EmotionTrackerUpdate | 955 |
| CastleMemberBehavior | 908 |
| GettingBuiltBehavior | 836 |
| AutoAbilityBehavior | 815 |
| W3DFloorDraw | 471 |
| GeometryUpgrade | 462 |
| HitReactionBehavior | 442 |
| PickupStuffUpdate | 387 |
| ToggleMountedSpecialAbilityUpdate | 366 |
| WeaponModeSpecialPowerUpdate | 334 |
| BuildingBehavior | 333 |
| DoCommandUpgrade | 299 |
| RespawnUpdate | 268 |
| TerrainResourceBehavior | 253 |
| InvisibilityUpdate | 209 |
| NotifyTargetsOfImminentProbableCrushingUpdate | 201 |
| MonitorConditionUpdate | 183 |
| TemporarilyDefectUpdate | 182 |
| BannerCarrierUpdate | 179 |
| HeroModeSpecialAbilityUpdate | 158 |
| ActivateModuleSpecialPower | 144 |
| ExperienceLevelCreate | 134 |
| CastleBehavior | 129 |
| SiegeDockingBehavior | 126 |
| HordeNotifyTargetsOfImminentProbableCrushingUpdate | 121 |
| AttachUpdate | 115 |
| SlaveWatcherBehavior | 114 |
| DualWeaponBehavior | 112 |
| ModelConditionSpecialAbilityUpdate | 92 |
| W3DStreakDraw | 88 |
| AudioLoopUpgrade | 84 |
| W3DBuffDraw | 73 |
| RadiateFearUpdate | 72 |
| PlayerHealSpecialPower | 67 |
| CastleUpgrade | 67 |
| ToggleHiddenSpecialAbilityUpdate | 65 |
| InheritUpgradeCreate | 63 |
| PorcupineFormationBodyModule | 59 |
| ArrowStormUpdate | 59 |
| TooltipUpgrade | 55 |
| BloodthirstyUpdate | 55 |
| RefundDie | 54 |
| GateOpenAndCloseBehavior | 50 |
| GiveUpgradeUpdate | 47 |
| ClearanceTestingSlowDeathBehavior | 47 |
| W3DLightDraw | 46 |
| WallHubBehavior | 44 |
| DominateEnemySpecialPower | 43 |
| FloodUpdate | 42 |
| DynamicPortalBehaviour | 42 |
| FakePathfindPortalBehaviour | 41 |
| AIGateUpdate | 41 |
| DelayedDeathBody | 40 |
| SpecialEnemySenseUpdate | 39 |
| PlayerUpgradeSpecialPower | 39 |
| LevelGrantSpecialPower | 39 |

…and 92 more.

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
| Object | Texture | 12344 |
| Object | StateName | 11864 |
| Object | AnimationBlendTime | 10732 |
| Object | AnimationSpeedFactorRange | 7926 |
| CommandButton | InPalantir | 5240 |
| Object | AnimationPriority | 5204 |
| AudioEvent | VolumeSliderMultiplier | 3936 |
| Object | GeometryContactPoint | 3614 |
| CommandButton | Radial | 3406 |
| Object | UseWeaponTiming | 3023 |
| Object | ThreatLevel | 3017 |
| Object | EnteringStateFX | 3007 |
| Object | GeometryOffset | 2554 |
| Weapon | FiringDuration | 2514 |
| Object | UpgradeTexture | 2445 |
| Object | EvaEventDamagedOwner | 2096 |
| Object | StaticModelLODMode | 2068 |
| Object | ClientBehavior | 1892 |
| Object | MaxHealthDamaged | 1827 |
| Object | EvaEnemyObjectSightedEvent | 1796 |
| ChildObject | AnimationBlendTime | 1731 |
| Object | AdditionalGeometry | 1724 |
| Object | ShareAnimation | 1637 |
| Object | Sound | 1631 |
| Object | GravityMult | 1615 |
| ChildObject | Texture | 1606 |
| ChildObject | StateName | 1561 |
| Object | RandomTexture | 1545 |
| Object | CommandPoints | 1484 |
| Object | AILuaEventsList | 1482 |
| Object | DeathFlags | 1441 |
| Weapon | MeleeWeapon | 1419 |
| Object | Skeleton | 1405 |
| CommandButton | AutoAbility | 1379 |
| Object | EvaEventAmbushed | 1371 |
| Object | EvaEventDamagedByFireOwner | 1371 |
| Object | EvaEventDamagedFromShroudedSourceOwner | 1371 |
| Object | EvaEventSecondDamageFarFromFirstOwner | 1371 |
| Object | EvaEventSecondDamageFarFromFirstScanRange | 1371 |
| Object | EvaEventSecondDamageFarFromFirstTimeoutMS | 1371 |
| Object | GeometryName | 1362 |
| Object | UseStandardModelNames | 1311 |
| Object | BountyValue | 1289 |
| Object | Delay | 1287 |
| Object | FXEvent | 1229 |
| ObjectCreationList | IgnoreCommandPointLimit | 1197 |
| Object | VisionBonusPercentPerFoot | 1172 |
| Object | MaxHealthReallyDamaged | 1152 |
| Object | VisionBonusTestRadius | 1115 |
| Object | MaxVisionBonusPercent | 1114 |
| Object | AutoResolveWeapon | 1059 |
| ChildObject | AnimationSpeedFactorRange | 1027 |
| ObjectCreationList | DispositionAngle | 1019 |
| Object | VoiceFullyCreated | 985 |
| Object | VoicePriority | 983 |
| Object | AutoResolveArmor | 977 |
| Object | SoundImpact | 958 |
| Object | AttackPriority | 954 |
| Weapon | HitPercentage | 930 |
| Object | ThingClass | 926 |

…and 1573 more.

## Fields whose value format differs (parse errors)

| Block | Field | Count |
|---|---|---|

## Enum/flag values not known (in block)

| Block | Value | Count |
|---|---|---|
| CommandButton | CANCELABLE | 1866 |
| Object | UNIT | 1761 |
| CommandButton | NONPRESSABLE | 936 |
| CommandButton | TrainingRadiusCursor | 533 |
| ObjectCreationList | ABSOLUTE_ANGLE | 503 |
| Weapon | NEUTRAL | 474 |
| ObjectCreationList | BUILDING_CHUNKS | 436 |
| Object | DEATH_4 | 345 |
| CommandButton | TOGGLE_IMAGE_ON_WEAPONSET | 324 |
| CommandButton | HIDE_WHILE_DISABLED | 264 |
| Weapon | PER_POSITION | 258 |
| CommandButton | OK_FOR_MULTI_EXECUTE | 251 |
| Object | WEAPONSET_TOGGLE_1 | 245 |
| ObjectCreationList | RELATIVE_ANGLE | 232 |
| Object | PLAYER_UPGRADE_2 | 222 |
| ObjectCreationList | SPAWN_AROUND | 211 |
| CommandButton | ON_GROUND_ONLY | 209 |
| SpecialPower | SPECIAL_GENERAL_TARGETLESS | 153 |
| Object | ALTERNATE_FORMATION | 151 |
| SpecialPower | SPECIAL_AT_VISIBLE_OBJECT | 148 |
| Object | MOUNTED | 120 |
| Object | WEAPONSET_HERO_MODE | 108 |
| Object | ModelConditionState:USER_1 | 94 |
| SpecialPower | SPECIAL_HERO_MODE | 92 |
| Object | CLOSE_RANGE | 86 |
| Object | HIDDEN | 84 |
| CommandButton | TOGGLE_IMAGE_ON_FORMATION | 83 |
| Object | CONTAINED | 82 |
| SpecialPower | SPECIAL_ARROW_STORM | 80 |
| SpecialPower | SPECIAL_SPELL_BOOK_MEN_OF_DALE_ALLIES | 71 |
| SpecialPower | SPECIAL_SPELL_BOOK_BOMBARD | 70 |
| Object | CONTESTING_BUILDING | 70 |
| ObjectCreationList | USE_WATER_SURFACE | 68 |
| Object | SHADOW_VOLUME_NON_SELF_2 | 66 |
| Object | ModelConditionState:USER_2 | 66 |
| Weapon | MONSTERS | 63 |
| CommandButton | FamiliarRadiusCursor | 59 |
| SpecialPower | SPECIAL_TOGGLE_MOUNTED | 57 |
| SpecialPower | SPECIAL_GENERAL_TARGETLESS_THREE | 55 |
| Object | OBSOLETE | 54 |

…and 447 more.
