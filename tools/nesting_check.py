#!/usr/bin/env python3
"""Check the engine's block-skip rules (INI.cpp bfmeSkipBlock) against indentation.

For every "Key = Module Tag" module line and every bare sub-block keyword inside an expanded
INI file, run the same depth counting the engine uses and compare where the skip stops with the
End line at the opener's own indentation. Mismatches are places where the engine would lose
track of nesting.  Usage: nesting_check.py <expanded.ini>...
"""
import re
import sys

PLAIN = set("""AIEconomyAssigment AddModule Animation AnimationState ArmorSet ArmyEntry ArmyMemberDefinition
ArmyToSpawn Attribute AttributeModifierNugget AudioEvent AutoResolveArmor AutoResolveWeapon Behavior
BonusForLevel BuffNugget BuildingNugget CameraShakerVolume ColorIntensityControlPoint CombatChainDefinition
CreateAHeroBling CreateAHeroBlingBinder CreateDebris CreateObject DOTNugget DamageNugget DecalTemplate
DefaultModelConditionState DynamicDecal EmotionWeaponNugget EnableRegion EvaEvent EyeTowerPoints FXList
FXListAtBonePos FireLogicNugget FireWeaponNugget Flammability FloodMember FormationPreviewDecal
FormationPreviewItemDecal FormationSelection FormationTemplate GrabNugget IdleAnimationState ImagePart
InheritableModule InvisibilityNugget LocomotorSet LuaEventNugget MetaImpactNugget Mission ModelConditionState
MoveArmy MoveCamera Object ObjectAward OpenGateNugget OwnershipSet ParalyzeNugget ParticleSystem Phase
PlayerDefeatCondition ProductionModifier ProjectileNugget Region ReplaceModule ReplaceObject RestrictBuildings
Rows Scenario SelectionDecal Session SetPlayerControlOfArmy SideInfo SideSound Sound SpawnAndFadeNugget
SpawnArmies SpawnArmy SpawnBuildings SpecialModelConditionNugget Stance StartingRestriction Structure SubClass
System TaskAfterAudio TeamDefeatCondition TeamVictoryCondition TerrainScorch ThingStat ThreatBreakdown
Threshold TransitionState Trigger Turret UnitCategory UnitDefinition UnitSpecificSounds ViewInfo ViewShake
WeaponOCLNugget WeaponSet Window WorldText TintDrawable LightPulse AttachedModel FireWeapon""".split())
EQ = set(sys.argv[1].split(",")) if len(sys.argv) > 1 and sys.argv[1].startswith("EQ=") else None
EQ = set("""Animation Behavior Body Draw ClientBehavior ClientUpdate Update Physics AnimationState
ModelConditionState TransitionState SoundState SoundUpgrade ThreatBreakdown LodOptions Wind HistoricalScenario
RadiusCursorTemplate""".split())


def parse(path):
    out = []
    for raw in open(path, encoding="latin-1").read().split("\n"):
        s = raw.rstrip()
        out.append((len(s) - len(s.lstrip()), s.strip()))
    return out


def opener(s):
    if not s:
        return False
    has_eq = "=" in s
    tok = re.split(r"[ \t=]+", s)[0]
    if tok.lower() in ("end", "end_end"):
        return False
    if has_eq:
        return tok in EQ
    return tok in PLAIN or tok.endswith("Nugget")


def skip_end(lines, i):
    """engine rule: from opener at i, return the index of the End that closes it"""
    depth, j, script = 1, i + 1, False
    while j < len(lines) and depth:
        s = lines[j][1]
        if script:
            script = s.split()[:1] != ["EndScript"]
        elif s.startswith("BeginScript"):
            script = True
        elif s:
            tok = re.split(r"[ \t=]+", s)[0]
            if tok.lower() == "end":
                depth -= 1
            elif opener(s):
                depth += 1
        j += 1
    return j - 1


def indent_end(lines, i):
    """End at the opener's indentation; None when indentation is not trustworthy here"""
    ind = lines[i][0]
    for j in range(i + 1, len(lines)):
        s = lines[j][1]
        if not s:
            continue
        if s.split()[0].lower() == "end" and lines[j][0] <= ind:
            return j if lines[j][0] == ind else None
        if lines[j][0] < ind:
            return None
    return None


def main():
    bad = 0
    for path in [a for a in sys.argv[1:] if not a.startswith("EQ=")]:
        lines = parse(path)
        script = False
        for i, (ind, s) in enumerate(lines):
            if script:
                script = s.split()[:1] != ["EndScript"]
                continue
            if s.startswith("BeginScript"):
                script = True
                continue
            if ind == 0 or not opener(s):
                continue
            a, b = skip_end(lines, i), indent_end(lines, i)
            if b is not None and a != b:
                bad += 1
                if bad <= 40:
                    print(f"{path}:{i+1}: '{s[:50]}' engine closes at {a+1}, indentation says {b+1}")
    print(f"{bad} disagreements", file=sys.stderr)


if __name__ == "__main__":
    main()
