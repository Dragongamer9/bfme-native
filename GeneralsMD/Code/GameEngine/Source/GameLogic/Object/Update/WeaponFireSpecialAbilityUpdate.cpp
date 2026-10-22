// bfme-native: see WeaponFireSpecialAbilityUpdate.h
#include "PreRTS.h"

#include "Common/Xfer.h"
#include "GameLogic/AttributeModifier.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/WeaponFireSpecialAbilityUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/Weapon.h"

void WeaponFireSpecialAbilityUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	SpecialAbilityUpdateModuleData::buildFieldParse(p);
	static const FieldParse fp[] =
	{
		{ "SpecialWeapon",							INI::parseAsciiString,					nullptr, offsetof( WeaponFireSpecialAbilityUpdateModuleData, m_specialWeapon ) },
		{ "WhichSpecialWeapon",					INI::parseInt,									nullptr, offsetof( WeaponFireSpecialAbilityUpdateModuleData, m_whichSpecialWeapon ) },
		{ "SkipContinue",								INI::parseBool,									nullptr, offsetof( WeaponFireSpecialAbilityUpdateModuleData, m_skipContinue ) },
		{ "MustFinishAbility",					INI::parseBool,									nullptr, offsetof( WeaponFireSpecialAbilityUpdateModuleData, m_mustFinishAbility ) },
		{ "FreezeAfterTriggerDuration",	INI::parseDurationUnsignedInt,	nullptr, offsetof( WeaponFireSpecialAbilityUpdateModuleData, m_freezeAfterTriggerDuration ) },
		{ "BusyForDuration",						INI::parseAsciiString,					nullptr, offsetof( WeaponFireSpecialAbilityUpdateModuleData, m_unusedString ) },
		{ "ContinueIfNoTarget",					INI::parseBool,									nullptr, offsetof( WeaponFireSpecialAbilityUpdateModuleData, m_unusedBool ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fp);
}

WeaponFireSpecialAbilityUpdate::WeaponFireSpecialAbilityUpdate( Thing *thing, const ModuleData* moduleData ) : SpecialAbilityUpdate( thing, moduleData )
{
}

WeaponFireSpecialAbilityUpdate::~WeaponFireSpecialAbilityUpdate()
{
}

Bool WeaponFireSpecialAbilityUpdate::bfmeTriggerOverride()
{
	const WeaponFireSpecialAbilityUpdateModuleData* d = getWeaponFireSpecialAbilityUpdateModuleData();
	const WeaponTemplate* wt = TheWeaponStore->findWeaponTemplate(d->m_specialWeapon);
	Object* me = getObject();
	if (wt == nullptr)
		return TRUE;

	// the "special weapon N" animation
	static const ModelConditionFlagType anims[] = { MODELCONDITION_SPECIAL_WEAPON_ONE, MODELCONDITION_SPECIAL_WEAPON_TWO,
		MODELCONDITION_SPECIAL_WEAPON_THREE, MODELCONDITION_SPECIAL_WEAPON_FOUR, MODELCONDITION_SPECIAL_WEAPON_FIVE, MODELCONDITION_SPECIAL_WEAPON_SIX };
	const Int which = d->m_whichSpecialWeapon >= 1 && d->m_whichSpecialWeapon <= 6 ? d->m_whichSpecialWeapon - 1 : 0;
	ModelConditionFlags flags;
	flags.set(anims[which]);
	TheAttributeModifierStore->applyTimedModelConditions(me, flags, LOGICFRAMES_PER_SECOND);

	Object* target = TheGameLogic->findObjectByID(bfmeGetTargetID());
	if (target && !target->isEffectivelyDead())
		TheWeaponStore->createAndFireTempWeapon(wt, me, target);
	else if (bfmeHasTarget())
		TheWeaponStore->createAndFireTempWeapon(wt, me, bfmeGetTargetPos());
	else
		TheWeaponStore->createAndFireTempWeapon(wt, me, me->getPosition());
	return TRUE;
}

void WeaponFireSpecialAbilityUpdate::crc( Xfer *xfer )
{
	SpecialAbilityUpdate::crc( xfer );
}

void WeaponFireSpecialAbilityUpdate::xfer( Xfer *xfer )
{
	XferVersion cv = 1;
	XferVersion v = cv;
	xfer->xferVersion( &v, cv );
	SpecialAbilityUpdate::xfer( xfer );
}

void WeaponFireSpecialAbilityUpdate::loadPostProcess()
{
	SpecialAbilityUpdate::loadPostProcess();
}

//=================================================================================================
// HeroModeSpecialAbilityUpdate
//=================================================================================================
void HeroModeSpecialAbilityUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	SpecialAbilityUpdateModuleData::buildFieldParse(p);
	static const FieldParse fp[] =
	{
		{ "HeroAttributeModifier",					INI::parseAsciiString,					nullptr, offsetof( HeroModeSpecialAbilityUpdateModuleData, m_heroAttributeModifier ) },
		{ "HeroEffectDuration",							INI::parseDurationUnsignedInt,	nullptr, offsetof( HeroModeSpecialAbilityUpdateModuleData, m_heroEffectDuration ) },
		{ "TriggerModelCondition",					ModelConditionFlags::parseFromINI,	nullptr, offsetof( HeroModeSpecialAbilityUpdateModuleData, m_triggerModelCondition ) },
		{ "TriggerModelConditionDuration",	INI::parseDurationUnsignedInt,	nullptr, offsetof( HeroModeSpecialAbilityUpdateModuleData, m_triggerModelConditionDuration ) },
		{ "StopUnitBeforeActivating",				INI::parseBool,									nullptr, offsetof( HeroModeSpecialAbilityUpdateModuleData, m_stopUnitBeforeActivating ) },
		{ "UnpackingVariation",							INI::parseAsciiString,					nullptr, offsetof( HeroModeSpecialAbilityUpdateModuleData, m_unusedString ) },
		{ "RequiredConditions",							INI::parseAsciiString,					nullptr, offsetof( HeroModeSpecialAbilityUpdateModuleData, m_unusedString ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fp);
}

HeroModeSpecialAbilityUpdate::HeroModeSpecialAbilityUpdate( Thing *thing, const ModuleData* moduleData ) : SpecialAbilityUpdate( thing, moduleData )
{
}

HeroModeSpecialAbilityUpdate::~HeroModeSpecialAbilityUpdate()
{
}

Bool HeroModeSpecialAbilityUpdate::bfmeTriggerOverride()
{
	const HeroModeSpecialAbilityUpdateModuleData* d = getHeroModeSpecialAbilityUpdateModuleData();
	Object* me = getObject();
	const AttributeModifierList* list = d->m_heroAttributeModifier.isNotEmpty() ? TheAttributeModifierStore->findList(d->m_heroAttributeModifier) : nullptr;
	if (list)
		TheAttributeModifierStore->apply(me, list, me->getID(), d->m_heroEffectDuration);
	if (d->m_triggerModelCondition.any())
		TheAttributeModifierStore->applyTimedModelConditions(me, d->m_triggerModelCondition,
			d->m_triggerModelConditionDuration ? d->m_triggerModelConditionDuration : d->m_heroEffectDuration);
	if (getenv("BFME_DEBUG_ABILITY"))
		printf("[ability] #%u hero mode %s for %u frames\n", me->getID(), d->m_heroAttributeModifier.str(), d->m_heroEffectDuration);
	return TRUE;
}

void HeroModeSpecialAbilityUpdate::crc( Xfer *xfer )
{
	SpecialAbilityUpdate::crc( xfer );
}

void HeroModeSpecialAbilityUpdate::xfer( Xfer *xfer )
{
	XferVersion cv = 1;
	XferVersion v = cv;
	xfer->xferVersion( &v, cv );
	SpecialAbilityUpdate::xfer( xfer );
}

void HeroModeSpecialAbilityUpdate::loadPostProcess()
{
	SpecialAbilityUpdate::loadPostProcess();
}

//-------------------------------------------------------------------------------------------------
// bfme-native: ModelConditionSpecialAbilityUpdate
//-------------------------------------------------------------------------------------------------
void ModelConditionSpecialAbilityUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	SpecialAbilityUpdateModuleData::buildFieldParse(p);
	static const FieldParse fp[] =
	{
		{ "WhichSpecialPower",					INI::parseInt,				nullptr, offsetof( ModelConditionSpecialAbilityUpdateModuleData, m_whichSpecialPower ) },
		// TODO: terror pulses, ring bearer rules
		{ "MustFinishAbility",					INI::parseBool,				nullptr, offsetof( ModelConditionSpecialAbilityUpdateModuleData, m_unusedBool ) },
		{ "GenerateTerror",							INI::parseBool,				nullptr, offsetof( ModelConditionSpecialAbilityUpdateModuleData, m_unusedBool ) },
		{ "EmotionPulseRadius",					INI::parseReal,				nullptr, offsetof( ModelConditionSpecialAbilityUpdateModuleData, m_unusedReal ) },
		{ "ObjectFilter",								ObjectFilter::parseFromINI,	nullptr, offsetof( ModelConditionSpecialAbilityUpdateModuleData, m_objectFilter ) },
		{ "UnpackingVariation",					INI::parseInt,				nullptr, offsetof( ModelConditionSpecialAbilityUpdateModuleData, m_unusedInt ) },
		{ "DisableWhenWearingTheRing",	INI::parseBool,				nullptr, offsetof( ModelConditionSpecialAbilityUpdateModuleData, m_unusedBool ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fp);
}

ModelConditionSpecialAbilityUpdate::ModelConditionSpecialAbilityUpdate( Thing *thing, const ModuleData* moduleData ) : SpecialAbilityUpdate( thing, moduleData )
{
}

ModelConditionSpecialAbilityUpdate::~ModelConditionSpecialAbilityUpdate()
{
}

ModelConditionFlags ModelConditionSpecialAbilityUpdate::bfmeAbilityConditions() const
{
	static const ModelConditionFlagType s_power[] = { MODELCONDITION_SPECIAL_POWER_1, MODELCONDITION_SPECIAL_POWER_2, MODELCONDITION_SPECIAL_POWER_3 };
	Int n = getModelConditionSpecialAbilityUpdateModuleData()->m_whichSpecialPower;
	n = n < 1 ? 1 : (n > 3 ? 3 : n);
	ModelConditionFlags f;
	f.set( s_power[n - 1] );
	return f;
}

void ModelConditionSpecialAbilityUpdate::crc( Xfer *xfer )
{
	SpecialAbilityUpdate::crc( xfer );
}

void ModelConditionSpecialAbilityUpdate::xfer( Xfer *xfer )
{
	XferVersion cv = 1;
	XferVersion v = cv;
	xfer->xferVersion( &v, cv );
	SpecialAbilityUpdate::xfer( xfer );
}

void ModelConditionSpecialAbilityUpdate::loadPostProcess()
{
	SpecialAbilityUpdate::loadPostProcess();
}
