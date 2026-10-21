// bfme-native: small RotWK modules, see BfmeModules.h
#include "PreRTS.h"

#include "Common/BuildAssistant.h"
#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "GameClient/FXList.h"
#include "GameLogic/AttributeModifier.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/BfmeModules.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/CreateModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"

// standard snapshot boilerplate for modules without own state
#define BFME_TRIVIAL_SNAPSHOT(Cls, Base) \
	void Cls::crc( Xfer *xfer ) { Base::crc( xfer ); } \
	void Cls::xfer( Xfer *xfer ) { XferVersion cv = 1; XferVersion v = cv; xfer->xferVersion( &v, cv ); Base::xfer( xfer ); } \
	void Cls::loadPostProcess() { Base::loadPostProcess(); } \
	Cls::~Cls() {}

static Bool objOrPlayerHasUpgrade(const Object* obj, const AsciiString& name)
{
	const UpgradeTemplate* ut = TheUpgradeCenter ? TheUpgradeCenter->findUpgrade(name) : nullptr;
	if (ut == nullptr)
		return FALSE;
	if (obj->hasUpgrade(ut))
		return TRUE;
	const Player* p = obj->getControllingPlayer();
	return p && p->hasUpgradeComplete(ut);
}

//=================================================================================================
// LevelUpUpgrade: gain LevelsToGain levels, never past LevelCap
//=================================================================================================
void LevelUpUpgradeModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpgradeModuleData::buildFieldParse(p);
	static const FieldParse fp[] =
	{
		{ "LevelsToGain",	INI::parseInt,	nullptr, offsetof( LevelUpUpgradeModuleData, m_levelsToGain ) },
		{ "LevelCap",			INI::parseInt,	nullptr, offsetof( LevelUpUpgradeModuleData, m_levelCap ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fp);
}

LevelUpUpgrade::LevelUpUpgrade( Thing *thing, const ModuleData* moduleData ) : UpgradeModule( thing, moduleData ) {}
BFME_TRIVIAL_SNAPSHOT(LevelUpUpgrade, UpgradeModule)

void LevelUpUpgrade::upgradeImplementation()
{
	const LevelUpUpgradeModuleData* d = getLevelUpUpgradeModuleData();
	ExperienceTracker* xp = getObject()->getExperienceTracker();
	if (xp == nullptr || !xp->isTrainable())
		return;
	for (Int i = 0; i < d->m_levelsToGain; ++i)
	{
		if (d->m_levelCap > 0 && (Int)xp->getVeterancyLevel() >= d->m_levelCap)
			break;
		if (!xp->gainExpForLevel(1, FALSE))
			break;
	}
}

//=================================================================================================
// RemoveUpgradeUpgrade: take an upgrade away again (from this object or the whole player)
//=================================================================================================
void RemoveUpgradeUpgradeModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpgradeModuleData::buildFieldParse(p);
	static const FieldParse fp[] =
	{
		{ "UpgradeToRemove",						INI::parseAsciiStringVectorAppend,	nullptr, offsetof( RemoveUpgradeUpgradeModuleData, m_upgradesToRemove ) },
		{ "RemoveFromAllPlayerObjects",	INI::parseBool,											nullptr, offsetof( RemoveUpgradeUpgradeModuleData, m_removeFromAllPlayerObjects ) },
		{ "SuppressEvaEventForRemoval",	INI::parseBool,											nullptr, offsetof( RemoveUpgradeUpgradeModuleData, m_suppressEva ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fp);
}

RemoveUpgradeUpgrade::RemoveUpgradeUpgrade( Thing *thing, const ModuleData* moduleData ) : UpgradeModule( thing, moduleData ) {}
BFME_TRIVIAL_SNAPSHOT(RemoveUpgradeUpgrade, UpgradeModule)

struct RemoveUpgradeCtx { const UpgradeTemplate* ut; };
static void removeUpgradeFromObject(Object* obj, void* userData)
{
	const UpgradeTemplate* ut = ((RemoveUpgradeCtx*)userData)->ut;
	if (obj && obj->hasUpgrade(ut))
		obj->removeUpgrade(ut);
}

void RemoveUpgradeUpgrade::upgradeImplementation()
{
	const RemoveUpgradeUpgradeModuleData* d = getRemoveUpgradeUpgradeModuleData();
	Object* me = getObject();
	for (size_t i = 0; i < d->m_upgradesToRemove.size(); ++i)
	{
		const UpgradeTemplate* ut = TheUpgradeCenter->findUpgrade(d->m_upgradesToRemove[i]);
		if (ut == nullptr)
			continue;
		Player* p = me->getControllingPlayer();
		if (ut->getUpgradeType() == UPGRADE_TYPE_PLAYER && p)
		{
			p->removeUpgrade(ut);
		}
		else if (d->m_removeFromAllPlayerObjects && p)
		{
			RemoveUpgradeCtx ctx = { ut };
			p->iterateObjects(removeUpgradeFromObject, &ctx);
		}
		else if (me->hasUpgrade(ut))
		{
			me->removeUpgrade(ut);
		}
	}
}

//=================================================================================================
// ReplaceSelfUpgrade: swap this object for another template in place
//=================================================================================================
void ReplaceSelfUpgradeModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpgradeModuleData::buildFieldParse(p);
	static const FieldParse fp[] =
	{
		{ "ReplaceWith",	INI::parseAsciiString,	nullptr, offsetof( ReplaceSelfUpgradeModuleData, m_replaceWith ) },
		{ "AndThenAddA",	INI::parseAsciiString,	nullptr, offsetof( ReplaceSelfUpgradeModuleData, m_andThenAddA ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fp);
}

ReplaceSelfUpgrade::ReplaceSelfUpgrade( Thing *thing, const ModuleData* moduleData ) : UpgradeModule( thing, moduleData ) {}
BFME_TRIVIAL_SNAPSHOT(ReplaceSelfUpgrade, UpgradeModule)

void ReplaceSelfUpgrade::upgradeImplementation()
{
	const ReplaceSelfUpgradeModuleData* d = getReplaceSelfUpgradeModuleData();
	Object* me = getObject();
	const ThingTemplate* tmpl = TheThingFactory->findTemplate(d->m_replaceWith);
	if (tmpl == nullptr || me->isEffectivelyDead())
		return;
	Object* n = TheThingFactory->newObject(tmpl, me->getTeam());
	if (n == nullptr)
		return;
	n->setPosition(me->getPosition());
	n->setOrientation(me->getOrientation());
	if (d->m_andThenAddA.isNotEmpty())
	{
		const UpgradeTemplate* ut = TheUpgradeCenter->findUpgrade(d->m_andThenAddA);
		if (ut)
			n->giveUpgrade(ut);
	}
	TheGameLogic->destroyObject(me);
}

//=================================================================================================
// PassiveAreaEffectBehavior: wells, inns, banners -- a periodic buff/heal pulse
//=================================================================================================
PassiveAreaEffectBehaviorModuleData::PassiveAreaEffectBehaviorModuleData() :
	m_effectRadius(0.0f), m_pingDelay(LOGICFRAMES_PER_SECOND), m_healPercentPerSecond(0.0f), m_healFX(nullptr), m_nonStackable(FALSE)
{
}

void PassiveAreaEffectBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse fp[] =
	{
		{ "EffectRadius",					INI::parseReal,											nullptr, offsetof( PassiveAreaEffectBehaviorModuleData, m_effectRadius ) },
		{ "PingDelay",						INI::parseDurationUnsignedInt,			nullptr, offsetof( PassiveAreaEffectBehaviorModuleData, m_pingDelay ) },
		{ "AllowFilter",					ObjectFilter::parseFromINI,					nullptr, offsetof( PassiveAreaEffectBehaviorModuleData, m_allowFilter ) },
		{ "ModifierName",					INI::parseAsciiStringVectorAppend,	nullptr, offsetof( PassiveAreaEffectBehaviorModuleData, m_modifierNames ) },
		{ "UpgradeRequired",			INI::parseAsciiStringVectorAppend,	nullptr, offsetof( PassiveAreaEffectBehaviorModuleData, m_upgradeRequired ) },
		{ "HealPercentPerSecond",	INI::parsePercentToReal,						nullptr, offsetof( PassiveAreaEffectBehaviorModuleData, m_healPercentPerSecond ) },
		{ "HealFX",								INI::parseFXList,										nullptr, offsetof( PassiveAreaEffectBehaviorModuleData, m_healFX ) },
		{ "NonStackable",					INI::parseBool,											nullptr, offsetof( PassiveAreaEffectBehaviorModuleData, m_nonStackable ) },
		{ "AntiCategories",				INI::parseAsciiString,							nullptr, offsetof( PassiveAreaEffectBehaviorModuleData, m_antiCategories ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fp);
}

PassiveAreaEffectBehavior::PassiveAreaEffectBehavior( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	setWakeFrame(getObject(), UPDATE_SLEEP(1 + getObject()->getID() % max(1u, getPassiveAreaEffectBehaviorModuleData()->m_pingDelay)));
}
BFME_TRIVIAL_SNAPSHOT(PassiveAreaEffectBehavior, UpdateModule)

UpdateSleepTime PassiveAreaEffectBehavior::update()
{
	const PassiveAreaEffectBehaviorModuleData* d = getPassiveAreaEffectBehaviorModuleData();
	const UnsignedInt delay = max(1u, d->m_pingDelay);
	Object* me = getObject();
	if (me->isEffectivelyDead())
		return UPDATE_SLEEP_FOREVER;
	for (size_t i = 0; i < d->m_upgradeRequired.size(); ++i)
		if (!objOrPlayerHasUpgrade(me, d->m_upgradeRequired[i]))
			return UPDATE_SLEEP(delay);

	std::vector<const AttributeModifierList*> lists;
	for (size_t i = 0; i < d->m_modifierNames.size(); ++i)
	{
		const AttributeModifierList* l = TheAttributeModifierStore->findList(d->m_modifierNames[i]);
		if (l)
			lists.push_back(l);
	}
	const Real healFrac = d->m_healPercentPerSecond * ((Real)delay / LOGICFRAMES_PER_SECOND);

	SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(me, d->m_effectRadius, FROM_BOUNDINGSPHERE_2D);
	MemoryPoolObjectHolder hold(iter);
	for (Object* o = iter->first(); o; o = iter->next())
	{
		if (o->isEffectivelyDead())
			continue;
		if (!d->m_allowFilter.isEmpty() && !d->m_allowFilter.matches(o, me))
			continue;
		for (size_t l = 0; l < lists.size(); ++l)
			TheAttributeModifierStore->apply(o, lists[l], me->getID(), lists[l]->m_duration ? 0 : delay + delay / 2 + 1);
		if (healFrac > 0.0f)
		{
			BodyModuleInterface* body = o->getBodyModule();
			if (body && body->getHealth() < body->getMaxHealth())
			{
				o->attemptHealing(body->getMaxHealth() * healFrac, me);
				FXList::doFXObj(d->m_healFX, o);
			}
		}
	}
	return UPDATE_SLEEP(delay);
}

//=================================================================================================
// LargeGroupBonusUpdate: a bonus while enough friends stand close by
//=================================================================================================
LargeGroupBonusUpdateModuleData::LargeGroupBonusUpdateModuleData() :
	m_updateRate(LOGICFRAMES_PER_SECOND), m_count(0), m_radius(0.0f), m_rubOffRadius(0.0f), m_alliesOnly(TRUE)
{
}

void LargeGroupBonusUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse fp[] =
	{
		{ "UpdateRate",					INI::parseDurationUnsignedInt,			nullptr, offsetof( LargeGroupBonusUpdateModuleData, m_updateRate ) },
		{ "HordeMemberFilter",	ObjectFilter::parseFromINI,					nullptr, offsetof( LargeGroupBonusUpdateModuleData, m_hordeMemberFilter ) },
		{ "Count",							INI::parseInt,											nullptr, offsetof( LargeGroupBonusUpdateModuleData, m_count ) },
		{ "Radius",							INI::parseReal,											nullptr, offsetof( LargeGroupBonusUpdateModuleData, m_radius ) },
		{ "RubOffRadius",				INI::parseReal,											nullptr, offsetof( LargeGroupBonusUpdateModuleData, m_rubOffRadius ) },
		{ "AlliesOnly",					INI::parseBool,											nullptr, offsetof( LargeGroupBonusUpdateModuleData, m_alliesOnly ) },
		{ "AttributeModifier",	INI::parseAsciiString,							nullptr, offsetof( LargeGroupBonusUpdateModuleData, m_attributeModifier ) },
		{ "FlagSubObjectNames",	INI::parseAsciiStringVectorAppend,	nullptr, offsetof( LargeGroupBonusUpdateModuleData, m_flagSubObjectNames ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fp);
}

LargeGroupBonusUpdate::LargeGroupBonusUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	setWakeFrame(getObject(), UPDATE_SLEEP(1 + getObject()->getID() % max(1u, getLargeGroupBonusUpdateModuleData()->m_updateRate)));
}
BFME_TRIVIAL_SNAPSHOT(LargeGroupBonusUpdate, UpdateModule)

UpdateSleepTime LargeGroupBonusUpdate::update()
{
	const LargeGroupBonusUpdateModuleData* d = getLargeGroupBonusUpdateModuleData();
	const UnsignedInt delay = max(1u, d->m_updateRate);
	Object* me = getObject();
	if (me->isEffectivelyDead())
		return UPDATE_SLEEP_FOREVER;
	const AttributeModifierList* list = TheAttributeModifierStore->findList(d->m_attributeModifier);
	if (list == nullptr)
		return UPDATE_SLEEP_FOREVER;

	Int n = 0;
	SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(me, d->m_radius, FROM_BOUNDINGSPHERE_2D);
	MemoryPoolObjectHolder hold(iter);
	for (Object* o = iter->first(); o && n < d->m_count; o = iter->next())
	{
		if (o == me || o->isEffectivelyDead())
			continue;
		if (d->m_alliesOnly && me->getRelationship(o) != ALLIES)
			continue;
		if (!d->m_hordeMemberFilter.isEmpty() && !d->m_hordeMemberFilter.matches(o, me))
			continue;
		++n;
	}
	if (n >= d->m_count)
		TheAttributeModifierStore->apply(me, list, me->getID(), delay + delay / 2 + 1);
	return UPDATE_SLEEP(delay);
}

//=================================================================================================
HordeMemberCollide::HordeMemberCollide( Thing *thing, const ModuleData* moduleData ) : CollideModule( thing, moduleData ) {}
BFME_TRIVIAL_SNAPSHOT(HordeMemberCollide, CollideModule)

//=================================================================================================
// GettingBuiltBehavior
//=================================================================================================
void GettingBuiltBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse fp[] =
	{
		{ "WorkerName",									INI::parseAsciiString,					nullptr, offsetof( GettingBuiltBehaviorModuleData, m_workerName ) },
		{ "EvilWorkerName",							INI::parseAsciiString,					nullptr, offsetof( GettingBuiltBehaviorModuleData, m_evilWorkerName ) },
		{ "SpawnTimer",									INI::parseDurationUnsignedInt,	nullptr, offsetof( GettingBuiltBehaviorModuleData, m_spawnTimer ) },
		{ "RebuildTimeSeconds",					INI::parseReal,									nullptr, offsetof( GettingBuiltBehaviorModuleData, m_rebuildTimeSeconds ) },
		{ "UseSpawnTimerWithoutWorker",	INI::parseBool,									nullptr, offsetof( GettingBuiltBehaviorModuleData, m_useSpawnTimerWithoutWorker ) },
		{ "RebuildWhenDead",						INI::parseBool,									nullptr, offsetof( GettingBuiltBehaviorModuleData, m_rebuildWhenDead ) },
		{ "SelfRepairFromDamageLoop",		INI::parseAsciiString,					nullptr, offsetof( GettingBuiltBehaviorModuleData, m_unusedString ) },
		{ "SelfRepairFromRubbleLoop",		INI::parseAsciiString,					nullptr, offsetof( GettingBuiltBehaviorModuleData, m_unusedString ) },
		{ "SelfBuildingLoop",						INI::parseAsciiString,					nullptr, offsetof( GettingBuiltBehaviorModuleData, m_unusedString ) },
		{ "DisallowRebuildRange",				INI::parseReal,									nullptr, offsetof( GettingBuiltBehaviorModuleData, m_unusedReal ) },
		{ "DisallowRebuildFilter",			INI::parseAsciiString,					nullptr, offsetof( GettingBuiltBehaviorModuleData, m_unusedString ) },
		{ "TestFaction",								INI::parseAsciiString,					nullptr, offsetof( GettingBuiltBehaviorModuleData, m_unusedString ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fp);
}

GettingBuiltBehavior::GettingBuiltBehavior( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	setWakeFrame(getObject(), UPDATE_SLEEP(1));
}
BFME_TRIVIAL_SNAPSHOT(GettingBuiltBehavior, UpdateModule)

UpdateSleepTime GettingBuiltBehavior::update()
{
	Object* me = getObject();
	if (me->isEffectivelyDead())
		return UPDATE_SLEEP_FOREVER;
	if (!me->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
		return UPDATE_SLEEP(LOGICFRAMES_PER_SECOND);	// built; keep an eye out (rebuilds come back under construction)

	// same bookkeeping as Zero Hour's dozer construction (DozerAIUpdate), with the structure as its own builder
	const Int framesToBuild = max(1, me->getTemplate()->calcTimeToBuild(me->getControllingPlayer()));
	const Real step = 100.0f / (Real)framesToBuild;
	me->setConstructionPercent(me->getConstructionPercent() + step);
	BodyModuleInterface* body = me->getBodyModule();
	if (body)
		body->internalChangeHealth(body->getMaxHealth() / (Real)framesToBuild);

	if (me->getConstructionPercent() >= 100.0f)
	{
		me->clearStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_UNDER_CONSTRUCTION));
		me->clearStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_RECONSTRUCTING));
		me->clearModelConditionFlags(MAKE_MODELCONDITION_MASK3(MODELCONDITION_AWAITING_CONSTRUCTION, MODELCONDITION_PARTIALLY_CONSTRUCTED, MODELCONDITION_ACTIVELY_BEING_CONSTRUCTED));
		me->setConstructionPercent(CONSTRUCTION_COMPLETE);
		if (body)
			body->evaluateVisualCondition();
		Player* player = me->getControllingPlayer();
		if (player)
		{
			player->onStructureConstructionComplete(me, me, FALSE);
			for (BehaviorModule** m = me->getBehaviorModules(); *m; ++m)
			{
				CreateModuleInterface* create = (*m)->getCreate();
				if (create)
					create->onBuildComplete();
			}
		}
		me->handlePartitionCellMaintenance();
		me->updateUpgradeModules();
		return UPDATE_SLEEP(LOGICFRAMES_PER_SECOND);
	}
	me->setModelConditionState(MODELCONDITION_ACTIVELY_BEING_CONSTRUCTED);
	return UPDATE_SLEEP_NONE;
}
