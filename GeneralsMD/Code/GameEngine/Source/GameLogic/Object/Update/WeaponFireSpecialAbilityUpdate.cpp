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
