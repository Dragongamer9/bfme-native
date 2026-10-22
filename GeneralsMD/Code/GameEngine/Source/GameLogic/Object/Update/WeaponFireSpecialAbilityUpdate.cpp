// bfme-native: see WeaponFireSpecialAbilityUpdate.h
#include "PreRTS.h"

#include "Common/Xfer.h"
#include "GameLogic/AttributeModifier.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/WeaponFireSpecialAbilityUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "Common/ThingTemplate.h"
#include "Common/Team.h"
#include "Common/Player.h"
#include "GameClient/FXList.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/BfmeModules.h"

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

//=================================================================================================
// ActivateModuleSpecialPower
//=================================================================================================
void ActivateModuleSpecialPowerModuleData::parseTrigger(INI* ini, void*, void* store, const void*)
{
	std::vector<Trigger>* v = (std::vector<Trigger>*)store;
	Trigger t;
	t.tag = ini->getNextToken();
	t.atTarget = FALSE;
	const char* mode = ini->getNextTokenOrNull();
	if (mode && (stricmp(mode, "TARGETPOS") == 0 || stricmp(mode, "TRAGETPOS") == 0))	// the data has the typo too
		t.atTarget = TRUE;
	v->push_back(t);
}

void ActivateModuleSpecialPowerModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	SpecialAbilityUpdateModuleData::buildFieldParse(p);
	static const FieldParse fp[] =
	{
		{ "TriggerSpecialPower",	parseTrigger,				nullptr, offsetof( ActivateModuleSpecialPowerModuleData, m_triggers ) },
		{ "EffectRange",					INI::parseReal,			nullptr, offsetof( ActivateModuleSpecialPowerModuleData, m_unusedReal ) },
		{ "UnpackingVariation",		INI::parseInt,			nullptr, offsetof( ActivateModuleSpecialPowerModuleData, m_unusedInt ) },
		{ "MustFinishAbility",		INI::parseBool,			nullptr, offsetof( ActivateModuleSpecialPowerModuleData, m_unusedBool ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fp);
}

ActivateModuleSpecialPower::ActivateModuleSpecialPower( Thing *thing, const ModuleData* moduleData ) : SpecialAbilityUpdate( thing, moduleData )
{
}

ActivateModuleSpecialPower::~ActivateModuleSpecialPower()
{
}

Bool ActivateModuleSpecialPower::bfmeTriggerOverride()
{
	const ActivateModuleSpecialPowerModuleData* d = getActivateModuleSpecialPowerModuleData();
	Object* me = getObject();
	static Int s_depth = 0;	// a triggered module may lead back here; never loop
	if (s_depth > 4)
		return TRUE;
	++s_depth;
	Coord3D target = *me->getPosition();
	if (bfmeHasTarget())
	{
		const Object* t = TheGameLogic->findObjectByID(bfmeGetTargetID());
		target = t ? *t->getPosition() : *bfmeGetTargetPos();
	}
	for (size_t i = 0; i < d->m_triggers.size(); ++i)
	{
		const NameKeyType key = NAMEKEY(d->m_triggers[i].tag);
		Coord3D at = d->m_triggers[i].atTarget ? target : *me->getPosition();
		Bool found = FALSE;
		for (BehaviorModule** b = me->getBehaviorModules(); *b; ++b)
		{
			if ((*b)->getModuleTagNameKey() != key)
				continue;
			found = TRUE;
			SpecialPowerModuleInterface* sp = (*b)->getSpecialPower();
			if (sp)
				sp->doSpecialPowerAtLocation(&at, INVALID_ANGLE, 0);
			break;
		}
		if (getenv("BFME_DEBUG_ABILITY"))
			printf("[ability] #%u %s triggers %s at %.0f,%.0f%s\n", me->getID(), me->getTemplate()->getName().str(),
				d->m_triggers[i].tag.str(), at.x, at.y, found ? "" : " (no such module)");
	}
	--s_depth;
	return TRUE;
}

void ActivateModuleSpecialPower::crc( Xfer *xfer )
{
	SpecialAbilityUpdate::crc( xfer );
}

void ActivateModuleSpecialPower::xfer( Xfer *xfer )
{
	XferVersion cv = 1;
	XferVersion v = cv;
	xfer->xferVersion( &v, cv );
	SpecialAbilityUpdate::xfer( xfer );
}

void ActivateModuleSpecialPower::loadPostProcess()
{
	SpecialAbilityUpdate::loadPostProcess();
}

//=================================================================================================
// DominateEnemySpecialPower
//=================================================================================================
static const UnsignedInt DEFECT_SECONDS = 20;	// Edain TemporarilyDefectUpdate DefectDuration

void DominateEnemySpecialPowerModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	SpecialAbilityUpdateModuleData::buildFieldParse(p);
	static const FieldParse fp[] =
	{
		{ "AttributeModifierAffects",				ObjectFilter::parseFromINI,			nullptr, offsetof( DominateEnemySpecialPowerModuleData, m_affects ) },
		{ "DominateRadius",									INI::parseReal,									nullptr, offsetof( DominateEnemySpecialPowerModuleData, m_radius ) },
		{ "PermanentlyConvert",							INI::parseBool,									nullptr, offsetof( DominateEnemySpecialPowerModuleData, m_permanent ) },
		{ "DominatedFX",										INI::parseFXList,								nullptr, offsetof( DominateEnemySpecialPowerModuleData, m_dominatedFX ) },
		{ "TriggerFX",											INI::parseFXList,								nullptr, offsetof( DominateEnemySpecialPowerModuleData, m_triggerFX ) },
		{ "UnpackingVariation",							INI::parseInt,									nullptr, offsetof( DominateEnemySpecialPowerModuleData, m_unusedInt ) },
		{ "Instant",												INI::parseBool,									nullptr, offsetof( DominateEnemySpecialPowerModuleData, m_unusedBool ) },
		{ "FreezeAfterTriggerDuration",			INI::parseDurationUnsignedInt,	nullptr, offsetof( DominateEnemySpecialPowerModuleData, m_unusedDuration ) },
		{ "TriggerModelConditionDuration",	INI::parseDurationUnsignedInt,	nullptr, offsetof( DominateEnemySpecialPowerModuleData, m_unusedDuration ) },
		{ "TriggerModelCondition",					ModelConditionFlags::parseFromINI,	nullptr, offsetof( DominateEnemySpecialPowerModuleData, m_unusedFlags ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fp);
}

DominateEnemySpecialPower::DominateEnemySpecialPower( Thing *thing, const ModuleData* moduleData ) : SpecialAbilityUpdate( thing, moduleData )
{
}

DominateEnemySpecialPower::~DominateEnemySpecialPower()
{
}

static void bfmeConvert(Object* o, Team* team, Bool permanent, const FXList* fx)
{
	if (o->getTeam() == team)
		return;
	if (permanent)
	{
		o->setTeam(team);
	}
	else
	{
		const TeamID back = o->getTeam() ? o->getTeam()->getID() : TEAM_ID_INVALID;
		o->setTemporaryTeam(team);
		if (TheAttributeModifierStore)
			TheAttributeModifierStore->schedule(o, DEFECT_SECONDS * LOGICFRAMES_PER_SECOND, [back](Object* obj)
			{
				Team* t = TheTeamFactory ? TheTeamFactory->findTeamByID(back) : nullptr;
				if (t && !obj->isEffectivelyDead())
					obj->setTemporaryTeam(t);
			});
	}
	if (fx)
		FXList::doFXObj(fx, o);
}

Bool DominateEnemySpecialPower::bfmeTriggerOverride()
{
	const DominateEnemySpecialPowerModuleData* d = getDominateEnemySpecialPowerModuleData();
	Object* me = getObject();
	Team* team = me->getControllingPlayer() ? me->getControllingPlayer()->getDefaultTeam() : nullptr;
	if (team == nullptr)
		return TRUE;
	Object* targetObj = bfmeHasTarget() ? TheGameLogic->findObjectByID(bfmeGetTargetID()) : nullptr;
	Coord3D at = targetObj ? *targetObj->getPosition() : (bfmeHasTarget() ? *bfmeGetTargetPos() : *me->getPosition());
	if (d->m_triggerFX)
		FXList::doFXPos(d->m_triggerFX, &at);

	std::vector<Object*> victims;
	if (d->m_radius > 0.0f)
	{
		SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(&at, d->m_radius, FROM_CENTER_2D);
		MemoryPoolObjectHolder hold(iter);
		for (Object* o = iter->first(); o; o = iter->next())
			victims.push_back(o);
	}
	else if (targetObj)
		victims.push_back(targetObj);

	Int converted = 0;
	std::vector<ObjectID> done;
	for (size_t i = 0; i < victims.size(); ++i)
	{
		Object* o = victims[i];
		// a horde member stands for its horde
		Object* horde = o->getProducerID() != INVALID_ID ? TheGameLogic->findObjectByID(o->getProducerID()) : nullptr;
		if (horde && HordeContain::findOn(horde))
			o = horde;
		if (o == me || o->isEffectivelyDead() || o->isKindOf(KINDOF_STRUCTURE) || me->getRelationship(o) != ENEMIES)
			continue;
		if (std::find(done.begin(), done.end(), o->getID()) != done.end())
			continue;
		done.push_back(o->getID());
		if (!d->m_affects.isEmpty() && !d->m_affects.matches(o, me))
			continue;
		bfmeConvert(o, team, d->m_permanent, d->m_dominatedFX);
		if (HordeContain* hc = HordeContain::findOn(o))
		{
			const std::vector<ObjectID>& members = hc->getMembers();
			for (size_t m = 0; m < members.size(); ++m)
				if (Object* mo = TheGameLogic->findObjectByID(members[m]))
					bfmeConvert(mo, team, d->m_permanent, nullptr);
		}
		++converted;
	}
	if (getenv("BFME_DEBUG_ABILITY"))
		printf("[ability] #%u %s dominates %d at %.0f,%.0f (%s)\n", me->getID(), me->getTemplate()->getName().str(), converted,
			at.x, at.y, d->m_permanent ? "permanent" : "temporary");
	return TRUE;
}

void DominateEnemySpecialPower::crc( Xfer *xfer )
{
	SpecialAbilityUpdate::crc( xfer );
}

void DominateEnemySpecialPower::xfer( Xfer *xfer )
{
	XferVersion cv = 1;
	XferVersion v = cv;
	xfer->xferVersion( &v, cv );
	SpecialAbilityUpdate::xfer( xfer );
}

void DominateEnemySpecialPower::loadPostProcess()
{
	SpecialAbilityUpdate::loadPostProcess();
}

//=================================================================================================
// ArrowStormUpdate
//=================================================================================================
void ArrowStormUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	SpecialAbilityUpdateModuleData::buildFieldParse(p);
	static const FieldParse fp[] =
	{
		{ "WeaponTemplate",									INI::parseAsciiString,					nullptr, offsetof( ArrowStormUpdateModuleData, m_weapon ) },
		{ "TargetRadius",										INI::parseReal,									nullptr, offsetof( ArrowStormUpdateModuleData, m_targetRadius ) },
		{ "ShotsPerTarget",									INI::parseInt,									nullptr, offsetof( ArrowStormUpdateModuleData, m_shotsPerTarget ) },
		{ "MaxShots",												INI::parseInt,									nullptr, offsetof( ArrowStormUpdateModuleData, m_maxShots ) },
		{ "CanShootEmptyGround",						INI::parseBool,									nullptr, offsetof( ArrowStormUpdateModuleData, m_canShootEmptyGround ) },
		{ "ShotsPerBurst",									INI::parseInt,									nullptr, offsetof( ArrowStormUpdateModuleData, m_unusedInt ) },
		{ "UnpackingVariation",							INI::parseInt,									nullptr, offsetof( ArrowStormUpdateModuleData, m_unusedInt ) },
		{ "ParalyzeDurationWhenCompleted",	INI::parseDurationUnsignedInt,	nullptr, offsetof( ArrowStormUpdateModuleData, m_unusedDuration ) },
		{ "ParalyzeDurationWhenAborted",		INI::parseDurationUnsignedInt,	nullptr, offsetof( ArrowStormUpdateModuleData, m_unusedDuration ) },
		{ "ActiveLoopSound",								INI::parseAsciiString,					nullptr, offsetof( ArrowStormUpdateModuleData, m_unusedString ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fp);
}

ArrowStormUpdate::ArrowStormUpdate( Thing *thing, const ModuleData* moduleData ) : SpecialAbilityUpdate( thing, moduleData ),
	m_cycles(0), m_lastTrigger(0)
{
}

ArrowStormUpdate::~ArrowStormUpdate()
{
}

Bool ArrowStormUpdate::initiateIntentToDoSpecialPower(const SpecialPowerTemplate *specialPowerTemplate, const Object *targetObj, const Coord3D *targetPos, const Waypoint *way, UnsignedInt commandOptions )
{
	const Bool ok = SpecialAbilityUpdate::initiateIntentToDoSpecialPower(specialPowerTemplate, targetObj, targetPos, way, commandOptions);
	if (ok)
		m_cycles = 0;	// a new volley
	return ok;
}

Bool ArrowStormUpdate::bfmeContinue()
{
	const Int maxCycles = getArrowStormUpdateModuleData()->m_maxShots > 0 ? getArrowStormUpdateModuleData()->m_maxShots : 10;
	return m_cycles < maxCycles;
}

Bool ArrowStormUpdate::bfmeTriggerOverride()
{
	const ArrowStormUpdateModuleData* d = getArrowStormUpdateModuleData();
	Object* me = getObject();
	m_lastTrigger = TheGameLogic->getFrame();
	++m_cycles;
	const WeaponTemplate* wt = TheWeaponStore->findWeaponTemplate(d->m_weapon);
	if (wt == nullptr)
		return TRUE;
	const Object* targetObj = bfmeHasTarget() ? TheGameLogic->findObjectByID(bfmeGetTargetID()) : nullptr;
	Coord3D at = targetObj ? *targetObj->getPosition() : (bfmeHasTarget() ? *bfmeGetTargetPos() : *me->getPosition());
	Int shots = 0;
	SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(&at, d->m_targetRadius, FROM_CENTER_2D);
	MemoryPoolObjectHolder hold(iter);
	for (Object* o = iter->first(); o; o = iter->next())
	{
		if (o->isEffectivelyDead() || me->getRelationship(o) != ENEMIES || o->isKindOf(KINDOF_HORDE) || o->isKindOf(KINDOF_UNATTACKABLE))
			continue;
		for (Int k = 0; k < max(1, d->m_shotsPerTarget); ++k, ++shots)
			TheWeaponStore->createAndFireTempWeapon(wt, me, o);
	}
	if (shots == 0 && (d->m_canShootEmptyGround || !targetObj))
	{
		Coord3D p = at;
		p.x += GameLogicRandomValueReal(-d->m_targetRadius, d->m_targetRadius) * 0.7f;
		p.y += GameLogicRandomValueReal(-d->m_targetRadius, d->m_targetRadius) * 0.7f;
		TheWeaponStore->createAndFireTempWeapon(wt, me, &p);
	}
	if (getenv("BFME_DEBUG_ABILITY"))
		printf("[ability] #%u %s arrow storm cycle %d: %d shots at %.0f,%.0f\n", me->getID(), me->getTemplate()->getName().str(), m_cycles, shots, at.x, at.y);
	return TRUE;
}

void ArrowStormUpdate::crc( Xfer *xfer )
{
	SpecialAbilityUpdate::crc( xfer );
}

void ArrowStormUpdate::xfer( Xfer *xfer )
{
	XferVersion cv = 1;
	XferVersion v = cv;
	xfer->xferVersion( &v, cv );
	SpecialAbilityUpdate::xfer( xfer );
	xfer->xferInt( &m_cycles );
	xfer->xferUnsignedInt( &m_lastTrigger );
}

void ArrowStormUpdate::loadPostProcess()
{
	SpecialAbilityUpdate::loadPostProcess();
}

//=================================================================================================
// ToggleHiddenSpecialAbilityUpdate
//=================================================================================================
void ToggleHiddenSpecialAbilityUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	SpecialAbilityUpdateModuleData::buildFieldParse(p);
	static const FieldParse fp[] =
	{
		{ "UnpackingVariation",	INI::parseInt,	nullptr, offsetof( ToggleHiddenSpecialAbilityUpdateModuleData, m_unusedInt ) },
		{ "ShowPalantirTimer",	INI::parseBool,	nullptr, offsetof( ToggleHiddenSpecialAbilityUpdateModuleData, m_unusedBool ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fp);
}

ToggleHiddenSpecialAbilityUpdate::ToggleHiddenSpecialAbilityUpdate( Thing *thing, const ModuleData* moduleData ) : SpecialAbilityUpdate( thing, moduleData )
{
}

ToggleHiddenSpecialAbilityUpdate::~ToggleHiddenSpecialAbilityUpdate()
{
}

Bool ToggleHiddenSpecialAbilityUpdate::bfmeTriggerOverride()
{
	static const NameKeyType key = NAMEKEY("InvisibilityUpdate");
	Object* me = getObject();
	for (BehaviorModule** b = me->getBehaviorModules(); *b; ++b)
	{
		if ((*b)->getModuleNameKey() != key)
			continue;
		InvisibilityUpdate* inv = static_cast<InvisibilityUpdate*>(*b);
		inv->toggleHidden();
		const UnsignedInt dur = getToggleHiddenSpecialAbilityUpdateModuleData()->m_effectDuration;
		if (inv->isToggledHidden() && dur > 0 && TheAttributeModifierStore)
			TheAttributeModifierStore->schedule(me, dur, [](Object* obj)	// a timed cloak
			{
				static const NameKeyType k = NAMEKEY("InvisibilityUpdate");
				for (BehaviorModule** m = obj->getBehaviorModules(); *m; ++m)
					if ((*m)->getModuleNameKey() == k)
					{
						InvisibilityUpdate* iu = static_cast<InvisibilityUpdate*>(*m);
						if (iu->isToggledHidden())
							iu->toggleHidden();
						break;
					}
			});
		if (getenv("BFME_DEBUG_ABILITY"))
			printf("[ability] #%u %s hidden %s\n", me->getID(), me->getTemplate()->getName().str(), inv->isToggledHidden() ? "on" : "off");
		break;
	}
	return TRUE;
}

void ToggleHiddenSpecialAbilityUpdate::crc( Xfer *xfer )
{
	SpecialAbilityUpdate::crc( xfer );
}

void ToggleHiddenSpecialAbilityUpdate::xfer( Xfer *xfer )
{
	XferVersion cv = 1;
	XferVersion v = cv;
	xfer->xferVersion( &v, cv );
	SpecialAbilityUpdate::xfer( xfer );
}

void ToggleHiddenSpecialAbilityUpdate::loadPostProcess()
{
	SpecialAbilityUpdate::loadPostProcess();
}
