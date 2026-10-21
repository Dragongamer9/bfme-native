// bfme-native: RotWK AttributeModifierAuraUpdate
#include "PreRTS.h"

#include "Common/Player.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameLogic/AttributeModifier.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AttributeModifierAuraUpdate.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"

AttributeModifierAuraUpdateModuleData::AttributeModifierAuraUpdateModuleData() :
	m_startsActive(FALSE),
	m_refreshDelay(LOGICFRAMES_PER_SECOND),
	m_range(0.0f),
	m_requiresAllTriggers(FALSE),
	m_targetEnemy(FALSE),
	m_allowSelf(FALSE),
	m_runWhileDead(FALSE),
	m_affectContainedOnly(FALSE),
	m_maxActiveRank(0)
{
}

void AttributeModifierAuraUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse dataFieldParse[] =
	{
		{ "BonusName",						INI::parseAsciiString,						nullptr, offsetof( AttributeModifierAuraUpdateModuleData, m_bonusName ) },
		{ "StartsActive",					INI::parseBool,										nullptr, offsetof( AttributeModifierAuraUpdateModuleData, m_startsActive ) },
		{ "RefreshDelay",					INI::parseDurationUnsignedInt,		nullptr, offsetof( AttributeModifierAuraUpdateModuleData, m_refreshDelay ) },
		{ "Range",								INI::parseReal,										nullptr, offsetof( AttributeModifierAuraUpdateModuleData, m_range ) },
		{ "ObjectFilter",					ObjectFilter::parseFromINI,				nullptr, offsetof( AttributeModifierAuraUpdateModuleData, m_objectFilter ) },
		{ "TriggeredBy",					INI::parseAsciiStringVectorAppend,	nullptr, offsetof( AttributeModifierAuraUpdateModuleData, m_triggeredBy ) },
		{ "ConflictsWith",				INI::parseAsciiStringVectorAppend,	nullptr, offsetof( AttributeModifierAuraUpdateModuleData, m_conflictsWith ) },
		{ "RequiresAllTriggers",	INI::parseBool,										nullptr, offsetof( AttributeModifierAuraUpdateModuleData, m_requiresAllTriggers ) },
		{ "TargetEnemy",					INI::parseBool,										nullptr, offsetof( AttributeModifierAuraUpdateModuleData, m_targetEnemy ) },
		{ "AllowSelf",						INI::parseBool,										nullptr, offsetof( AttributeModifierAuraUpdateModuleData, m_allowSelf ) },
		{ "RunWhileDead",					INI::parseBool,										nullptr, offsetof( AttributeModifierAuraUpdateModuleData, m_runWhileDead ) },
		{ "AffectContainedOnly",	INI::parseBool,										nullptr, offsetof( AttributeModifierAuraUpdateModuleData, m_affectContainedOnly ) },
		{ "MaxActiveRank",				INI::parseInt,										nullptr, offsetof( AttributeModifierAuraUpdateModuleData, m_maxActiveRank ) },
		{ "RequiredConditions",		ModelConditionFlags::parseFromINI,	nullptr, offsetof( AttributeModifierAuraUpdateModuleData, m_requiredConditions ) },
		{ "AntiCategory",					INI::parseAsciiString,						nullptr, offsetof( AttributeModifierAuraUpdateModuleData, m_antiCategory ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(dataFieldParse);
}

AttributeModifierAuraUpdate::AttributeModifierAuraUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	// spread the first refresh so a freshly placed army doesn't all tick on one frame
	setWakeFrame(getObject(), UPDATE_SLEEP(1 + (getObject()->getID() % max(1u, getAttributeModifierAuraUpdateModuleData()->m_refreshDelay))));
}

AttributeModifierAuraUpdate::~AttributeModifierAuraUpdate()
{
}

static Bool objHasUpgradeNamed(const Object* obj, const AsciiString& name)
{
	const UpgradeTemplate* ut = TheUpgradeCenter ? TheUpgradeCenter->findUpgrade(name) : nullptr;
	if (ut == nullptr)
		return FALSE;
	if (obj->hasUpgrade(ut))
		return TRUE;
	const Player* p = obj->getControllingPlayer();
	return p && p->hasUpgradeComplete(ut);
}

Bool AttributeModifierAuraUpdate::isActive() const
{
	const AttributeModifierAuraUpdateModuleData* d = getAttributeModifierAuraUpdateModuleData();
	const Object* me = getObject();
	for (size_t i = 0; i < d->m_conflictsWith.size(); ++i)
		if (objHasUpgradeNamed(me, d->m_conflictsWith[i]))
			return FALSE;
	if (d->m_triggeredBy.empty())
		return d->m_startsActive;
	Bool any = FALSE, all = TRUE;
	for (size_t i = 0; i < d->m_triggeredBy.size(); ++i)
	{
		if (objHasUpgradeNamed(me, d->m_triggeredBy[i]))
			any = TRUE;
		else
			all = FALSE;
	}
	return d->m_startsActive || (d->m_requiresAllTriggers ? all : any);
}

UpdateSleepTime AttributeModifierAuraUpdate::update()
{
	const AttributeModifierAuraUpdateModuleData* d = getAttributeModifierAuraUpdateModuleData();
	const UnsignedInt delay = max(1u, d->m_refreshDelay);
	Object* me = getObject();

	if ((me->isEffectivelyDead() && !d->m_runWhileDead) || !isActive())
		return UPDATE_SLEEP(delay);
	if (d->m_requiredConditions.any())
	{
		const Drawable* dr = me->getDrawable();
		if (dr == nullptr || !dr->getModelConditionFlags().testForAll(d->m_requiredConditions))
			return UPDATE_SLEEP(delay);
	}

	const AttributeModifierList* list = TheAttributeModifierStore->findList(d->m_bonusName);
	if (list == nullptr)
		return UPDATE_SLEEP_FOREVER;

	// keep the bonus alive until shortly after the next refresh
	const UnsignedInt duration = list->m_duration ? list->m_duration : delay + delay / 2 + 1;

	if (d->m_affectContainedOnly)
	{
		ContainModuleInterface* contain = me->getContain();
		if (contain)
		{
			const ContainedItemsList* items = contain->getContainedItemsList();
			if (items)
				for (ContainedItemsList::const_iterator it = items->begin(); it != items->end(); ++it)
					if (d->m_objectFilter.isEmpty() || d->m_objectFilter.matches(*it, me))
						TheAttributeModifierStore->apply(*it, list, me->getID(), duration);
		}
		return UPDATE_SLEEP(delay);
	}

	SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(me, d->m_range, FROM_BOUNDINGSPHERE_2D);
	MemoryPoolObjectHolder hold(iter);
	for (Object* o = iter->first(); o; o = iter->next())
	{
		if (o == me && !d->m_allowSelf)
			continue;
		if (o->isEffectivelyDead())
			continue;
		if (!d->m_objectFilter.isEmpty() && !d->m_objectFilter.matches(o, me))
			continue;
		TheAttributeModifierStore->apply(o, list, me->getID(), duration);
	}
	if (d->m_allowSelf && d->m_range <= 0.0f)
		TheAttributeModifierStore->apply(me, list, me->getID(), duration);
	return UPDATE_SLEEP(delay);
}

void AttributeModifierAuraUpdate::crc( Xfer *xfer )
{
	UpdateModule::crc( xfer );
}

void AttributeModifierAuraUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	UpdateModule::xfer( xfer );
}

void AttributeModifierAuraUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}
