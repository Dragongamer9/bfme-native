// bfme-native: RotWK WeaponFireSpecialAbilityUpdate -- a hero ability that walks into range, plays
// its preparation and then fires a special weapon (SpecialWeapon) at the target or location.
#pragma once

#include "GameLogic/Module/SpecialAbilityUpdate.h"

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
};
