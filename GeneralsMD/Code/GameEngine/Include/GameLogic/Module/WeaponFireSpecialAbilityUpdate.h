// bfme-native: RotWK WeaponFireSpecialAbilityUpdate -- a hero ability that walks into range, plays
// its preparation and then fires a special weapon (SpecialWeapon) at the target or location.
#pragma once

#include "GameLogic/Module/SpecialAbilityUpdate.h"
#include "GameLogic/ObjectFilter.h"

class WeaponTemplate;

class WeaponFireSpecialAbilityUpdateModuleData : public SpecialAbilityUpdateModuleData
{
public:
	WeaponFireSpecialAbilityUpdateModuleData() : m_whichSpecialWeapon(1), m_skipContinue(FALSE), m_mustFinishAbility(FALSE),
		m_freezeAfterTriggerDuration(0) {}
	static void buildFieldParse(MultiIniFieldParse& p);

	AsciiString m_specialWeapon;
	Int m_whichSpecialWeapon;
	Bool m_skipContinue;
	Bool m_mustFinishAbility;
	UnsignedInt m_freezeAfterTriggerDuration;
	AsciiString m_unusedString;
	Bool m_unusedBool;
};

class WeaponFireSpecialAbilityUpdate : public SpecialAbilityUpdate
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( WeaponFireSpecialAbilityUpdate, "WeaponFireSpecialAbilityUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( WeaponFireSpecialAbilityUpdate, WeaponFireSpecialAbilityUpdateModuleData )

public:
	WeaponFireSpecialAbilityUpdate( Thing *thing, const ModuleData* moduleData );

protected:
	virtual Bool bfmeTriggerOverride() override;
};

//-------------------------------------------------------------------------------------------------
/** HeroModeSpecialAbilityUpdate: a timed hero buff (Aragorn's Blade Master): HeroAttributeModifier for
	HeroEffectDuration, plus an optional timed TriggerModelCondition. */
class HeroModeSpecialAbilityUpdateModuleData : public SpecialAbilityUpdateModuleData
{
public:
	HeroModeSpecialAbilityUpdateModuleData() : m_heroEffectDuration(0), m_triggerModelConditionDuration(0), m_stopUnitBeforeActivating(FALSE) {}
	static void buildFieldParse(MultiIniFieldParse& p);

	AsciiString m_heroAttributeModifier;
	UnsignedInt m_heroEffectDuration;
	ModelConditionFlags m_triggerModelCondition;
	UnsignedInt m_triggerModelConditionDuration;
	Bool m_stopUnitBeforeActivating;
	AsciiString m_unusedString;
};

class HeroModeSpecialAbilityUpdate : public SpecialAbilityUpdate
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( HeroModeSpecialAbilityUpdate, "HeroModeSpecialAbilityUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( HeroModeSpecialAbilityUpdate, HeroModeSpecialAbilityUpdateModuleData )

public:
	HeroModeSpecialAbilityUpdate( Thing *thing, const ModuleData* moduleData );

protected:
	virtual Bool bfmeTriggerOverride() override;
};//-------------------------------------------------------------------------------------------------
/** ModelConditionSpecialAbilityUpdate: a RotWK ability that only plays an animation (SPECIAL_POWER_n with
	UNPACKING/PREPARING/PACKING); its effect comes from the SpecialPowerModule (AttributeModifier, TriggerFX). */
class ModelConditionSpecialAbilityUpdateModuleData : public SpecialAbilityUpdateModuleData
{
public:
	ModelConditionSpecialAbilityUpdateModuleData() : m_whichSpecialPower(1), m_unusedInt(0), m_unusedReal(0), m_unusedBool(FALSE) {}
	static void buildFieldParse(MultiIniFieldParse& p);

	Int m_whichSpecialPower;
	Int m_unusedInt;
	Real m_unusedReal;
	Bool m_unusedBool;
	AsciiString m_unusedString;
	ObjectFilter m_objectFilter;	// TODO: who the terror pulse affects
};

class ModelConditionSpecialAbilityUpdate : public SpecialAbilityUpdate
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( ModelConditionSpecialAbilityUpdate, "ModelConditionSpecialAbilityUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( ModelConditionSpecialAbilityUpdate, ModelConditionSpecialAbilityUpdateModuleData )

public:
	ModelConditionSpecialAbilityUpdate( Thing *thing, const ModuleData* moduleData );

protected:
	virtual Bool bfmeTriggerOverride() override { return TRUE; }
	virtual ModelConditionFlags bfmeAbilityConditions() const override;
};

//-------------------------------------------------------------------------------------------------
/** ActivateModuleSpecialPower: a RotWK ability update whose effect is other special power modules of
	the same object, named by tag: "TriggerSpecialPower = <ModuleTag> [TARGETPOS|OBJECTPOS]". TARGETPOS
	fires the module at the ability's target, otherwise at the object itself. Many Edain hero
	abilities work this way (the effects sit on SpecialAbilityActivateeDummy modules). */
class ActivateModuleSpecialPowerModuleData : public SpecialAbilityUpdateModuleData
{
public:
	struct Trigger { AsciiString tag; Bool atTarget; };
	ActivateModuleSpecialPowerModuleData() : m_unusedInt(0), m_unusedReal(0.0f), m_unusedBool(FALSE) {}
	static void buildFieldParse(MultiIniFieldParse& p);
	static void parseTrigger(INI* ini, void* instance, void* store, const void* userData);
	std::vector<Trigger> m_triggers;
	Int m_unusedInt;
	Real m_unusedReal;
	Bool m_unusedBool;
};

class ActivateModuleSpecialPower : public SpecialAbilityUpdate
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( ActivateModuleSpecialPower, "ActivateModuleSpecialPower" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( ActivateModuleSpecialPower, ActivateModuleSpecialPowerModuleData )
public:
	ActivateModuleSpecialPower( Thing *thing, const ModuleData* moduleData );
protected:
	virtual Bool bfmeTriggerOverride() override;
};

//-------------------------------------------------------------------------------------------------
/** DominateEnemySpecialPower: a RotWK ability that turns enemies near its target to the caster's side
	(Saruman, Wormtongue...): everything within DominateRadius of the target that passes
	AttributeModifierAffects, whole hordes at once. PermanentlyConvert keeps them; otherwise they
	return to their owner after DEFECT_SECONDS (the targets' TemporarilyDefectUpdate). */
class DominateEnemySpecialPowerModuleData : public SpecialAbilityUpdateModuleData
{
public:
	DominateEnemySpecialPowerModuleData() : m_radius(0.0f), m_permanent(FALSE), m_dominatedFX(nullptr), m_triggerFX(nullptr),
		m_unusedInt(0), m_unusedBool(FALSE), m_unusedDuration(0) {}
	static void buildFieldParse(MultiIniFieldParse& p);
	ObjectFilter m_affects;
	Real m_radius;
	Bool m_permanent;
	const FXList* m_dominatedFX;
	const FXList* m_triggerFX;
	Int m_unusedInt;
	Bool m_unusedBool;
	UnsignedInt m_unusedDuration;
	ModelConditionFlags m_unusedFlags;
};

class DominateEnemySpecialPower : public SpecialAbilityUpdate
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( DominateEnemySpecialPower, "DominateEnemySpecialPower" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( DominateEnemySpecialPower, DominateEnemySpecialPowerModuleData )
public:
	DominateEnemySpecialPower( Thing *thing, const ModuleData* moduleData );
protected:
	virtual Bool bfmeTriggerOverride() override;
};

//-------------------------------------------------------------------------------------------------
/** ArrowStormUpdate: a persistent RotWK volley (Legolas, Celeborn, archer heroes): every preparation
	cycle fires WeaponTemplate ShotsPerTarget times at each enemy within TargetRadius of the target
	(or at the ground there with CanShootEmptyGround), for MaxShots cycles. */
class ArrowStormUpdateModuleData : public SpecialAbilityUpdateModuleData
{
public:
	ArrowStormUpdateModuleData() : m_targetRadius(50.0f), m_shotsPerTarget(1), m_maxShots(10), m_canShootEmptyGround(FALSE),
		m_unusedInt(0), m_unusedDuration(0) {}
	static void buildFieldParse(MultiIniFieldParse& p);
	AsciiString m_weapon;
	Real m_targetRadius;
	Int m_shotsPerTarget;
	Int m_maxShots;
	Bool m_canShootEmptyGround;
	Int m_unusedInt;
	UnsignedInt m_unusedDuration;
	AsciiString m_unusedString;
};

class ArrowStormUpdate : public SpecialAbilityUpdate
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( ArrowStormUpdate, "ArrowStormUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( ArrowStormUpdate, ArrowStormUpdateModuleData )
public:
	ArrowStormUpdate( Thing *thing, const ModuleData* moduleData );
	virtual Bool initiateIntentToDoSpecialPower(const SpecialPowerTemplate *specialPowerTemplate, const Object *targetObj, const Coord3D *targetPos, const Waypoint *way, UnsignedInt commandOptions ) override;
protected:
	virtual Bool bfmeTriggerOverride() override;
	virtual Bool bfmeContinue() override;
private:
	Int m_cycles;
	UnsignedInt m_lastTrigger;
};

//-------------------------------------------------------------------------------------------------
/** ToggleHiddenSpecialAbilityUpdate: switches the object's first InvisibilityUpdate (a non-innate
	stealth such as the Elven Cloak) on or off. */
class ToggleHiddenSpecialAbilityUpdateModuleData : public SpecialAbilityUpdateModuleData
{
public:
	ToggleHiddenSpecialAbilityUpdateModuleData() : m_unusedInt(0), m_unusedBool(FALSE) {}
	static void buildFieldParse(MultiIniFieldParse& p);
	Int m_unusedInt;
	Bool m_unusedBool;
};

class ToggleHiddenSpecialAbilityUpdate : public SpecialAbilityUpdate
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( ToggleHiddenSpecialAbilityUpdate, "ToggleHiddenSpecialAbilityUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( ToggleHiddenSpecialAbilityUpdate, ToggleHiddenSpecialAbilityUpdateModuleData )
public:
	ToggleHiddenSpecialAbilityUpdate( Thing *thing, const ModuleData* moduleData );
protected:
	virtual Bool bfmeTriggerOverride() override;
};

//-------------------------------------------------------------------------------------------------
/** LevelGrantSpecialPower: gives Experience to everything within RadiusEffect of the target that
	passes AcceptanceFilter (hero "King's favour" style powers). */
class LevelGrantSpecialPowerModuleData : public SpecialAbilityUpdateModuleData
{
public:
	LevelGrantSpecialPowerModuleData() : m_experience(0), m_radius(0.0f), m_levelFX(nullptr), m_unusedInt(0), m_unusedDuration(0) {}
	static void buildFieldParse(MultiIniFieldParse& p);
	Int m_experience;
	Real m_radius;
	ObjectFilter m_filter;
	const FXList* m_levelFX;
	Int m_unusedInt;
	UnsignedInt m_unusedDuration;
};

class LevelGrantSpecialPower : public SpecialAbilityUpdate
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( LevelGrantSpecialPower, "LevelGrantSpecialPower" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( LevelGrantSpecialPower, LevelGrantSpecialPowerModuleData )
public:
	LevelGrantSpecialPower( Thing *thing, const ModuleData* moduleData );
protected:
	virtual Bool bfmeTriggerOverride() override;
};

//-------------------------------------------------------------------------------------------------
/** TeleportSpecialAbilityUpdate: the unit vanishes and reappears at the target point (at most
	MaxDistance away), firing DestinationWeaponName there. */
class TeleportSpecialAbilityUpdateModuleData : public SpecialAbilityUpdateModuleData
{
public:
	TeleportSpecialAbilityUpdateModuleData() : m_maxDistance(0.0f), m_unusedInt(0), m_unusedDuration(0) {}
	static void buildFieldParse(MultiIniFieldParse& p);
	AsciiString m_destinationWeapon;
	Real m_maxDistance;
	Int m_unusedInt;
	UnsignedInt m_unusedDuration;
};

class TeleportSpecialAbilityUpdate : public SpecialAbilityUpdate
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( TeleportSpecialAbilityUpdate, "TeleportSpecialAbilityUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( TeleportSpecialAbilityUpdate, TeleportSpecialAbilityUpdateModuleData )
public:
	TeleportSpecialAbilityUpdate( Thing *thing, const ModuleData* moduleData );
protected:
	virtual Bool bfmeTriggerOverride() override;
};
