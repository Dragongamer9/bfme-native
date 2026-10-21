// bfme-native: see ToggleMountedSpecialAbilityUpdate.h
#include "PreRTS.h"

#include "Common/SpecialPower.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "GameLogic/ArmorSet.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Module/ToggleMountedSpecialAbilityUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/WeaponSet.h"

void ToggleMountedSpecialAbilityUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	SpecialAbilityUpdateModuleData::buildFieldParse(p);
	static const FieldParse fp[] =
	{
		{ "MountedTemplate",								INI::parseAsciiString,							nullptr, offsetof( ToggleMountedSpecialAbilityUpdateModuleData, m_mountedTemplate ) },
		{ "OpacityTarget",									INI::parseReal,											nullptr, offsetof( ToggleMountedSpecialAbilityUpdateModuleData, m_opacityTarget ) },
		{ "IgnoreFacingCheck",							INI::parseBool,											nullptr, offsetof( ToggleMountedSpecialAbilityUpdateModuleData, m_ignoreFacingCheck ) },
		{ "SynchronizeTimerOnSpecialPower",	INI::parseAsciiStringVectorAppend,	nullptr, offsetof( ToggleMountedSpecialAbilityUpdateModuleData, m_synchronizeTimers ) },
		{ "TriggerInstantlyOnCreate",				INI::parseBool,											nullptr, offsetof( ToggleMountedSpecialAbilityUpdateModuleData, m_triggerInstantlyOnCreate ) },
		{ "CancelDisguiseWhenDismounting",	INI::parseBool,											nullptr, offsetof( ToggleMountedSpecialAbilityUpdateModuleData, m_cancelDisguiseWhenDismounting ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fp);
}

ToggleMountedSpecialAbilityUpdate::ToggleMountedSpecialAbilityUpdate( Thing *thing, const ModuleData* moduleData ) : SpecialAbilityUpdate( thing, moduleData )
{
}

ToggleMountedSpecialAbilityUpdate::~ToggleMountedSpecialAbilityUpdate()
{
}

Bool ToggleMountedSpecialAbilityUpdate::isMounted(const Object* obj)
{
	return obj->testWeaponSetFlag(WEAPONSET_BFME_MOUNTED);
}

void ToggleMountedSpecialAbilityUpdate::setMounted(Object* obj, Bool mounted)
{
	if (mounted)
	{
		obj->setWeaponSetFlag(WEAPONSET_BFME_MOUNTED);
		obj->setArmorSetFlag(ARMORSET_BFME_MOUNTED);
		obj->setModelConditionState(MODELCONDITION_MOUNTED);
	}
	else
	{
		obj->clearWeaponSetFlag(WEAPONSET_BFME_MOUNTED);
		obj->clearArmorSetFlag(ARMORSET_BFME_MOUNTED);
		obj->clearModelConditionState(MODELCONDITION_MOUNTED);
	}
	AIUpdateInterface* ai = obj->getAI();
	if (ai)
		ai->chooseLocomotorSet(mounted ? LOCOMOTORSET_MOUNTED : LOCOMOTORSET_NORMAL);
}

void ToggleMountedSpecialAbilityUpdate::onObjectCreated()
{
	SpecialAbilityUpdate::onObjectCreated();
	const ToggleMountedSpecialAbilityUpdateModuleData* d = getToggleMountedSpecialAbilityUpdateModuleData();
	if (d->m_triggerInstantlyOnCreate && d->m_mountedTemplate.isEmpty() && getObject()->getTemplate()->bfmeFindLocomotors(LOCOMOTORSET_MOUNTED))
		setMounted(getObject(), TRUE);
}

Bool ToggleMountedSpecialAbilityUpdate::bfmeTriggerOverride()
{
	const ToggleMountedSpecialAbilityUpdateModuleData* d = getToggleMountedSpecialAbilityUpdateModuleData();
	Object* me = getObject();
	if (d->m_mountedTemplate.isEmpty())
	{
		setMounted(me, !isMounted(me));
		return TRUE;
	}
	const ThingTemplate* t = TheThingFactory->findTemplate(d->m_mountedTemplate);
	if (t == nullptr)
		return TRUE;
	// replacing destroys us: do it once the update has unwound
	std::vector<AsciiString> sync = d->m_synchronizeTimers;
	if (d->m_specialPowerTemplate)
		sync.push_back(d->m_specialPowerTemplate->getName());
	bfmeReplaceObject(me, t, &sync);
	return TRUE;
}

Object* bfmeReplaceObject(Object* me, const ThingTemplate* tmpl, const std::vector<AsciiString>* syncPowers)
{
	if (me == nullptr || tmpl == nullptr || me->isEffectivelyDead())
		return nullptr;
	Object* n = TheThingFactory->newObject(tmpl, me->getTeam());
	if (n == nullptr)
		return nullptr;
	n->setPosition(me->getPosition());
	n->setOrientation(me->getOrientation());
	n->setProducer(TheGameLogic->findObjectByID(me->getProducerID()));

	// health as a fraction of the maximum
	BodyModuleInterface* ob = me->getBodyModule();
	BodyModuleInterface* nb = n->getBodyModule();
	if (ob && nb && ob->getMaxHealth() > 0.0f)
	{
		const Real want = nb->getMaxHealth() * (ob->getHealth() / ob->getMaxHealth());
		nb->internalChangeHealth(want - nb->getHealth());
	}

	// experience
	if (me->getExperienceTracker() && n->getExperienceTracker())
		n->getExperienceTracker()->setExperienceAndLevel(me->getExperienceTracker()->getCurrentExperience(), FALSE);

	// object upgrades
	const UpgradeMaskType& done = me->getObjectCompletedUpgradeMask();
	for (const UpgradeTemplate* ut = TheUpgradeCenter->firstUpgradeTemplate(); ut; ut = ut->friend_getNext())
		if (ut->getUpgradeType() == UPGRADE_TYPE_OBJECT && done.testForAny(ut->getUpgradeMask()) && !n->hasUpgrade(ut))
			n->giveUpgrade(ut);

	// keep special power recharges in step
	if (syncPowers)
	{
		for (size_t i = 0; i < syncPowers->size(); ++i)
		{
			const SpecialPowerTemplate* sp = TheSpecialPowerStore->findSpecialPowerTemplate((*syncPowers)[i]);
			if (sp == nullptr)
				continue;
			SpecialPowerModuleInterface* a = me->getSpecialPowerModule(sp);
			SpecialPowerModuleInterface* b = n->getSpecialPowerModule(sp);
			if (a && b)
				b->setReadyFrame(a->getReadyFrame());
		}
	}

	// carry on with what we were doing
	AIUpdateInterface* oa = me->getAI();
	AIUpdateInterface* na = n->getAI();
	if (oa && na)
	{
		Object* victim = oa->getGoalObject();
		if (victim && !victim->isEffectivelyDead())
			na->aiAttackObject(victim, NO_MAX_SHOTS_LIMIT, CMD_FROM_AI);
	}

	TheGameLogic->destroyObject(me);
	return n;
}

void ToggleMountedSpecialAbilityUpdate::crc( Xfer *xfer )
{
	SpecialAbilityUpdate::crc( xfer );
}

void ToggleMountedSpecialAbilityUpdate::xfer( Xfer *xfer )
{
	XferVersion cv = 1;
	XferVersion v = cv;
	xfer->xferVersion( &v, cv );
	SpecialAbilityUpdate::xfer( xfer );
}

void ToggleMountedSpecialAbilityUpdate::loadPostProcess()
{
	SpecialAbilityUpdate::loadPostProcess();
}
