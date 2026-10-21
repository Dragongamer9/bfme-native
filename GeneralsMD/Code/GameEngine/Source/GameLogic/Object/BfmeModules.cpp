// bfme-native: small RotWK modules, see BfmeModules.h
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "Common/SpecialPower.h"
#include "GameClient/ControlBar.h"
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
#include "GameLogic/Module/SpecialPowerUpdateModule.h"

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

//=================================================================================================
BFME_TRIVIAL_SNAPSHOT(BfmeInertBehavior, BehaviorModule)

//=================================================================================================
// TerrainResourceBehavior
//=================================================================================================
void TerrainResourceBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse fp[] =
	{
		{ "MaxIncome",						INI::parseInt,											nullptr, offsetof( TerrainResourceBehaviorModuleData, m_maxIncome ) },
		{ "IncomeInterval",				INI::parseDurationUnsignedInt,			nullptr, offsetof( TerrainResourceBehaviorModuleData, m_incomeInterval ) },
		{ "Radius",								INI::parseReal,											nullptr, offsetof( TerrainResourceBehaviorModuleData, m_radius ) },
		{ "Visible",							INI::parseBool,											nullptr, offsetof( TerrainResourceBehaviorModuleData, m_visible ) },
		{ "HighPriority",					INI::parseBool,											nullptr, offsetof( TerrainResourceBehaviorModuleData, m_highPriority ) },
		{ "Upgrade",							INI::parseAsciiStringVectorAppend,	nullptr, offsetof( TerrainResourceBehaviorModuleData, m_upgrades ) },
		{ "UpgradeBonusPercent",	INI::parsePercentToReal,						nullptr, offsetof( TerrainResourceBehaviorModuleData, m_upgradeBonusPercent ) },
		{ "UpgradeMustBePresent",	ObjectFilter::parseFromINI,					nullptr, offsetof( TerrainResourceBehaviorModuleData, m_upgradeMustBePresent ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fp);
}

TerrainResourceBehavior::TerrainResourceBehavior( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	setWakeFrame(getObject(), UPDATE_SLEEP(max(1u, getTerrainResourceBehaviorModuleData()->m_incomeInterval)));
}
BFME_TRIVIAL_SNAPSHOT(TerrainResourceBehavior, UpdateModule)

static TerrainResourceBehavior* findTerrainResource(const Object* o)
{
	for (BehaviorModule** m = const_cast<Object*>(o)->getBehaviorModules(); *m; ++m)
	{
		TerrainResourceBehavior* t = dynamic_cast<TerrainResourceBehavior*>(*m);
		if (t)
			return t;
	}
	return nullptr;
}

Real TerrainResourceBehavior::computeLandShare() const
{
	const Real radius = getTerrainResourceBehaviorModuleData()->m_radius;
	if (radius <= 0.0f)
		return 1.0f;
	const Object* me = getObject();
	const Real cell = 20.0f;	// GameData TerrainResourceCellSize

	// the other resource buildings that can overlap us
	std::vector<std::pair<Coord3D, Real> > others;
	SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(me, radius * 2.0f, FROM_CENTER_2D);
	MemoryPoolObjectHolder hold(iter);
	for (Object* o = iter->first(); o; o = iter->next())
	{
		if (o == me || o->isEffectivelyDead() || o->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
			continue;
		TerrainResourceBehavior* t = findTerrainResource(o);
		if (t && t->getRadius() > 0.0f)
			others.push_back(std::make_pair(*o->getPosition(), t->getRadius()));
	}

	Real share = 0.0f;
	Int cells = 0;
	const Coord3D* c = me->getPosition();
	for (Real dx = -radius; dx <= radius; dx += cell)
	{
		for (Real dy = -radius; dy <= radius; dy += cell)
		{
			if (dx * dx + dy * dy > radius * radius)
				continue;
			++cells;
			Int claimants = 1;
			for (size_t i = 0; i < others.size(); ++i)
			{
				const Real ox = c->x + dx - others[i].first.x, oy = c->y + dy - others[i].first.y;
				if (ox * ox + oy * oy <= others[i].second * others[i].second)
					++claimants;
			}
			share += 1.0f / (Real)claimants;
		}
	}
	return cells ? share / (Real)cells : 1.0f;
}

UpdateSleepTime TerrainResourceBehavior::update()
{
	const TerrainResourceBehaviorModuleData* d = getTerrainResourceBehaviorModuleData();
	const UnsignedInt interval = max(1u, d->m_incomeInterval);
	Object* me = getObject();
	if (me->isEffectivelyDead())
		return UPDATE_SLEEP_FOREVER;
	if (me->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION) || d->m_maxIncome <= 0)
		return UPDATE_SLEEP(interval);
	Player* p = me->getControllingPlayer();
	if (p == nullptr)
		return UPDATE_SLEEP(interval);

	Real amount = (Real)d->m_maxIncome * computeLandShare();
	Bool haveUpgrade = FALSE;
	for (size_t i = 0; i < d->m_upgrades.size(); ++i)
		if (objOrPlayerHasUpgrade(me, d->m_upgrades[i]))
			haveUpgrade = TRUE;
	if (haveUpgrade && !d->m_upgradeMustBePresent.isEmpty())
	{
		Bool found = FALSE;
		for (Object* o = TheGameLogic->getFirstObject(); o && !found; o = o->getNextObject())
			if (o->getControllingPlayer() == p && !o->isEffectivelyDead() && d->m_upgradeMustBePresent.matches(o, me))
				found = TRUE;
		haveUpgrade = found;
	}
	if (haveUpgrade)
		amount *= (1.0f + d->m_upgradeBonusPercent);

	const UnsignedInt cash = (UnsignedInt)floorf(amount + 0.5f);
	if (cash > 0)
		p->getMoney()->deposit(cash);
	return UPDATE_SLEEP(interval);
}

//=================================================================================================
// AutoAbilityBehavior
//=================================================================================================
void AutoAbilityBehaviorModuleData::parseQuery(INI* ini, void* /*instance*/, void* store, const void* /*userData*/)
{
	std::vector<AutoAbilityQuery>* v = (std::vector<AutoAbilityQuery>*)store;
	AutoAbilityQuery q;
	q.m_count = INI::scanInt(ini->getNextToken());
	ObjectFilter::parseFromINI(ini, nullptr, &q.m_filter, nullptr);
	v->push_back(q);
}

static void parseSecondsToFrames(INI* ini, void*, void* store, const void*)
{
	*(UnsignedInt*)store = (UnsignedInt)ceilf(INI::scanReal(ini->getNextToken()) * LOGICFRAMES_PER_SECOND);
}

void AutoAbilityBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse fp[] =
	{
		{ "SpecialAbility",						INI::parseAsciiString,						nullptr, offsetof( AutoAbilityBehaviorModuleData, m_specialAbility ) },
		{ "MaxScanRange",							INI::parseReal,										nullptr, offsetof( AutoAbilityBehaviorModuleData, m_maxScanRange ) },
		{ "MinScanRange",							INI::parseReal,										nullptr, offsetof( AutoAbilityBehaviorModuleData, m_minScanRange ) },
		{ "AllowSelf",								INI::parseBool,										nullptr, offsetof( AutoAbilityBehaviorModuleData, m_allowSelf ) },
		{ "StartsActive",							INI::parseBool,										nullptr, offsetof( AutoAbilityBehaviorModuleData, m_startsActive ) },
		{ "AdjustAttackMeleePosition",INI::parseBool,										nullptr, offsetof( AutoAbilityBehaviorModuleData, m_adjustAttackMeleePosition ) },
		{ "IdleTimeSeconds",					parseSecondsToFrames,			nullptr, offsetof( AutoAbilityBehaviorModuleData, m_idleTime ) },
		{ "BaseMaxRangeFromStartPos",	INI::parseBool,										nullptr, offsetof( AutoAbilityBehaviorModuleData, m_baseMaxRangeFromStartPos ) },
		{ "ForbiddenStatus",					ObjectStatusMaskType::parseFromINI,	nullptr, offsetof( AutoAbilityBehaviorModuleData, m_forbiddenStatus ) },
		{ "Query",										AutoAbilityBehaviorModuleData::parseQuery,	nullptr, offsetof( AutoAbilityBehaviorModuleData, m_queries ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fp);
}

AutoAbilityBehavior::AutoAbilityBehavior( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData ),
	m_active(getAutoAbilityBehaviorModuleData()->m_startsActive), m_idleSince(0)
{
	setWakeFrame(getObject(), UPDATE_SLEEP(LOGICFRAMES_PER_SECOND / 2 + GameLogicRandomValue(0, LOGICFRAMES_PER_SECOND / 2)));
}
BFME_TRIVIAL_SNAPSHOT(AutoAbilityBehavior, UpdateModule)

/// how the ability is aimed, from the first command button that uses it
static UnsignedInt autoAbilityTargeting(const SpecialPowerTemplate* sp)
{
	if (TheControlBar == nullptr)
		return NEED_TARGET_ENEMY_OBJECT;
	for (const CommandButton* b = TheControlBar->getCommandButtons(); b; b = b->getNext())
		if (b->getSpecialPowerTemplate() == sp)
			return b->getOptions() & (NEED_TARGET_ENEMY_OBJECT | NEED_TARGET_ALLY_OBJECT | NEED_TARGET_NEUTRAL_OBJECT | NEED_TARGET_POS);
	return 0;
}

UpdateSleepTime AutoAbilityBehavior::update()
{
	const AutoAbilityBehaviorModuleData* d = getAutoAbilityBehaviorModuleData();
	const UpdateSleepTime later = UPDATE_SLEEP(LOGICFRAMES_PER_SECOND / 2);
	Object* me = getObject();
	if (me->isEffectivelyDead())
		return UPDATE_SLEEP_FOREVER;
	Player* p = me->getControllingPlayer();
	const Bool computer = p && p->getPlayerType() == PLAYER_COMPUTER;
	if (!(m_active || computer) || me->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION) || me->getStatusBits().testForAny(d->m_forbiddenStatus))
		return later;

	const UnsignedInt now = TheGameLogic->getFrame();
	if (d->m_idleTime > 0)
	{
		AIUpdateInterface* ai = me->getAI();
		if (ai && !ai->isIdle())
		{
			m_idleSince = 0;
			return later;
		}
		if (m_idleSince == 0)
			m_idleSince = now;
		if (now - m_idleSince < d->m_idleTime)
			return later;
	}

	const SpecialPowerTemplate* sp = TheSpecialPowerStore->findSpecialPowerTemplate(d->m_specialAbility);
	SpecialPowerModuleInterface* spm = sp ? me->getSpecialPowerModule(sp) : nullptr;
	if (spm == nullptr)
		return UPDATE_SLEEP_FOREVER;
	if (!spm->isReady())
		return later;
	// don't restart (or interrupt) an ability that is still running
	for (BehaviorModule** m = me->getBehaviorModules(); *m; ++m)
	{
		SpecialPowerUpdateInterface* su = (*m)->getSpecialPowerUpdateInterface();
		if (su && su->isActive())
			return later;
	}

	const Real maxR = d->m_maxScanRange > 0.0f ? d->m_maxScanRange : 100.0f;
	for (size_t qi = 0; qi < d->m_queries.size(); ++qi)
	{
		const AutoAbilityQuery& q = d->m_queries[qi];
		Int found = 0;
		Object* closest = nullptr;
		Real closestD = 0.0f;
		SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(me, maxR, FROM_CENTER_2D);
		MemoryPoolObjectHolder hold(iter);
		for (Object* o = iter->first(); o; o = iter->next())
		{
			if (o->isEffectivelyDead() || o->isKindOf(KINDOF_HORDE) || (o == me && !d->m_allowSelf))
				continue;
			if (!q.m_filter.matches(o, me))
				continue;
			const Real dist = ThePartitionManager->getDistanceSquared(me, o, FROM_CENTER_2D);
			if (dist < d->m_minScanRange * d->m_minScanRange)
				continue;
			++found;
			if (closest == nullptr || dist < closestD)
			{
				closest = o;
				closestD = dist;
			}
		}
		if (d->m_allowSelf && q.m_filter.matches(me, me) && closest == nullptr)
		{
			closest = me;
			++found;
		}
		if (found < max(1, q.m_count) || closest == nullptr)
			continue;

		const UnsignedInt aim = autoAbilityTargeting(sp);
		if (aim & (NEED_TARGET_ENEMY_OBJECT | NEED_TARGET_ALLY_OBJECT | NEED_TARGET_NEUTRAL_OBJECT))
			spm->doSpecialPowerAtObject(closest, 0);
		else if (aim & NEED_TARGET_POS)
			spm->doSpecialPowerAtLocation(closest->getPosition(), 0.0f, 0);
		else
			spm->doSpecialPower(0);
		if (getenv("BFME_DEBUG_ABILITY"))
			printf("[ability] #%u %s auto-used %s on #%u (%d matches, query %d)\n", me->getID(), me->getTemplate()->getName().str(),
				d->m_specialAbility.str(), closest->getID(), found, (int)qi);
		return UPDATE_SLEEP(LOGICFRAMES_PER_SECOND);
	}
	return later;
}

//=================================================================================================
// DoCommandUpgrade
//=================================================================================================
void DoCommandUpgradeModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpgradeModuleData::buildFieldParse(p);
	static const FieldParse fp[] =
	{
		{ "GetUpgradeCommandButtonName",		INI::parseAsciiString,	nullptr, offsetof( DoCommandUpgradeModuleData, m_getUpgradeCommandButtonName ) },
		{ "RemoveUpgradeCommandButtonName",	INI::parseAsciiString,	nullptr, offsetof( DoCommandUpgradeModuleData, m_removeUpgradeCommandButtonName ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fp);
}

DoCommandUpgrade::DoCommandUpgrade( Thing *thing, const ModuleData* moduleData ) : UpgradeModule( thing, moduleData ) {}
BFME_TRIVIAL_SNAPSHOT(DoCommandUpgrade, UpgradeModule)

void DoCommandUpgrade::upgradeImplementation()
{
	const DoCommandUpgradeModuleData* d = getDoCommandUpgradeModuleData();
	Object* me = getObject();
	const CommandButton* b = TheControlBar ? TheControlBar->findCommandButton(d->m_getUpgradeCommandButtonName) : nullptr;
	if (b == nullptr || me->isEffectivelyDead())
		return;
	switch (b->getCommandType())
	{
		case GUI_COMMAND_OBJECT_UPGRADE:
		case GUI_COMMAND_PLAYER_UPGRADE:
		{
			const UpgradeTemplate* ut = b->getUpgradeTemplate();
			if (ut == nullptr)
				break;
			Player* p = me->getControllingPlayer();
			if (ut->getUpgradeType() == UPGRADE_TYPE_PLAYER)
			{
				if (p && !p->hasUpgradeComplete(ut))
					p->addUpgrade(ut, UPGRADE_STATUS_COMPLETE);
			}
			else if (!me->hasUpgrade(ut))
				me->giveUpgrade(ut);
			break;
		}
		case GUI_COMMAND_SPECIAL_POWER:
		case GUI_COMMAND_SPECIAL_POWER_FROM_SHORTCUT:
		case GUI_COMMAND_BFME_SPECIAL_POWER_TOGGLE:
		{
			const SpecialPowerTemplate* sp = b->getSpecialPowerTemplate();
			SpecialPowerModuleInterface* m = sp ? me->getSpecialPowerModule(sp) : nullptr;
			if (m)
			{
				m->setReadyFrame(TheGameLogic->getFrame());
				m->doSpecialPower(COMMAND_FIRED_BY_SCRIPT);
			}
			break;
		}
		default:
			break;
	}
}
