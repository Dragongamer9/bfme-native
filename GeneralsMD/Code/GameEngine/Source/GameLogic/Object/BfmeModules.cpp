// bfme-native: small RotWK modules, see BfmeModules.h
#include "PreRTS.h"
#include "Common/Money.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "Common/SpecialPower.h"
#include "GameClient/ControlBar.h"

#include "Common/BuildAssistant.h"
#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "GameClient/FXList.h"
#include "GameClient/Drawable.h"
#include "GameLogic/AttributeModifier.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/ExperienceLevel.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/BfmeModules.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/CreateModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"

// standard snapshot boilerplate for modules without own state
#include "GameLogic/Module/SpecialPowerUpdateModule.h"
#include "GameLogic/Module/ToggleMountedSpecialAbilityUpdate.h"

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
	// bfme-native: RotWK ranks
	if (xp->getBfmeRank() > 0)
	{
		Int target = xp->getBfmeRank() + d->m_levelsToGain;
		if (d->m_levelCap > 0)
			target = min(target, d->m_levelCap);
		xp->bfmeSetRank(target);
		return;
	}
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

//=================================================================================================
// RespawnUpdate
//=================================================================================================
/// "AutoSpawn:No Cost:2500 Time:135000 Health:100%" / "Level:3 Cost:200 Time:30000" (the value may be a separate token)
static void parseRespawnTokens(INI* ini, RespawnRule& r)
{
	for (const char* tok = ini->getNextTokenOrNull(); tok; tok = ini->getNextTokenOrNull())
	{
		AsciiString key = tok, val;
		const char* colon = strchr(tok, ':');
		if (colon)
		{
			key.set(tok, colon - tok);
			val = colon + 1;
		}
		if (val.isEmpty())
		{
			const char* v = ini->getNextTokenOrNull();
			if (v == nullptr)
				break;
			val = v;
		}
		if (key.compareNoCase("AutoSpawn") == 0)
			r.m_autoSpawn = INI::scanBool(val.str());
		else if (key.compareNoCase("Cost") == 0)
			r.m_cost = INI::scanInt(val.str());
		else if (key.compareNoCase("Time") == 0)
			r.m_time = (UnsignedInt)ceilf(INI::scanReal(val.str()) * LOGICFRAMES_PER_SECOND / 1000.0f);
		else if (key.compareNoCase("Health") == 0)
			r.m_health = INI::scanPercentToReal(val.str());
		else if (key.compareNoCase("Level") == 0)
			r.m_level = INI::scanInt(val.str());
	}
}

void RespawnUpdateModuleData::parseRule(INI* ini, void*, void* store, const void*)
{
	parseRespawnTokens(ini, *(RespawnRule*)store);
}

void RespawnUpdateModuleData::parseEntry(INI* ini, void*, void* store, const void*)
{
	RespawnRule r;
	r.m_cost = -1;
	r.m_time = 0;
	parseRespawnTokens(ini, r);
	((std::vector<RespawnRule>*)store)->push_back(r);
}

void RespawnUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse fp[] =
	{
		{ "DeathAnim",									ModelConditionFlags::parseFromINI,	nullptr, offsetof( RespawnUpdateModuleData, m_deathAnim ) },
		{ "DeathAnimationTime",					INI::parseDurationUnsignedInt,			nullptr, offsetof( RespawnUpdateModuleData, m_deathAnimationTime ) },
		{ "RespawnAnim",								ModelConditionFlags::parseFromINI,	nullptr, offsetof( RespawnUpdateModuleData, m_respawnAnim ) },
		{ "RespawnAnimationTime",				INI::parseDurationUnsignedInt,			nullptr, offsetof( RespawnUpdateModuleData, m_respawnAnimationTime ) },
		{ "DeathFX",										INI::parseFXList,										nullptr, offsetof( RespawnUpdateModuleData, m_deathFX ) },
		{ "RespawnFX",									INI::parseFXList,										nullptr, offsetof( RespawnUpdateModuleData, m_respawnFX ) },
		{ "InitialSpawnFX",							INI::parseFXList,										nullptr, offsetof( RespawnUpdateModuleData, m_initialSpawnFX ) },
		{ "AutoRespawnAtObjectFilter",	ObjectFilter::parseFromINI,					nullptr, offsetof( RespawnUpdateModuleData, m_autoRespawnAt ) },
		{ "ButtonImage",								INI::parseAsciiString,							nullptr, offsetof( RespawnUpdateModuleData, m_buttonImage ) },
		{ "RespawnAsTemplate",					INI::parseAsciiString,							nullptr, offsetof( RespawnUpdateModuleData, m_respawnAsTemplate ) },
		{ "RespawnRules",								RespawnUpdateModuleData::parseRule,	nullptr, offsetof( RespawnUpdateModuleData, m_rules ) },
		{ "RespawnEntry",								RespawnUpdateModuleData::parseEntry,	nullptr, offsetof( RespawnUpdateModuleData, m_entries ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fp);
}

RespawnUpdate::RespawnUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData ),
	m_state(STATE_ALIVE), m_stateFrame(0)
{
	setWakeFrame(getObject(), UPDATE_SLEEP_FOREVER);
}

void RespawnUpdate::crc( Xfer *xfer ) { UpdateModule::crc( xfer ); }
void RespawnUpdate::xfer( Xfer *xfer )
{
	XferVersion cv = 1;
	XferVersion v = cv;
	xfer->xferVersion( &v, cv );
	UpdateModule::xfer( xfer );
	Int st = (Int)m_state;
	xfer->xferInt(&st);
	m_state = (State)st;
	xfer->xferUnsignedInt(&m_stateFrame);
}
void RespawnUpdate::loadPostProcess() { UpdateModule::loadPostProcess(); }
RespawnUpdate::~RespawnUpdate() {}

RespawnUpdate* RespawnUpdate::findOn(const Object* obj)
{
	for (BehaviorModule** m = const_cast<Object*>(obj)->getBehaviorModules(); *m; ++m)
	{
		RespawnUpdate* r = dynamic_cast<RespawnUpdate*>(*m);
		if (r)
			return r;
	}
	return nullptr;
}

Bool bfmeRespawnInterceptDeath(Object* obj, DamageInfo* /*damageInfo*/)
{
	RespawnUpdate* r = RespawnUpdate::findOn(obj);
	if (r == nullptr || r->isFallen())
		return FALSE;
	r->onFallen();
	return TRUE;
}

RespawnRule RespawnUpdate::currentRule() const
{
	const RespawnUpdateModuleData* d = getRespawnUpdateModuleData();
	RespawnRule r = d->m_rules;
	const ExperienceTracker* xp = getObject()->getExperienceTracker();
	const Int level = xp ? (xp->getBfmeRank() > 0 ? xp->getBfmeRank() : (Int)xp->getVeterancyLevel() + 1) : 1;
	for (size_t i = 0; i < d->m_entries.size(); ++i)
	{
		if (d->m_entries[i].m_level != level)
			continue;
		if (d->m_entries[i].m_cost >= 0)
			r.m_cost = d->m_entries[i].m_cost;
		if (d->m_entries[i].m_time > 0)
			r.m_time = d->m_entries[i].m_time;
	}
	return r;
}

void RespawnUpdate::onFallen()
{
	const RespawnUpdateModuleData* d = getRespawnUpdateModuleData();
	Object* me = getObject();
	m_state = STATE_DYING;
	m_stateFrame = TheGameLogic->getFrame();
	AIUpdateInterface* ai = me->getAI();
	if (ai)
		ai->aiIdle(CMD_FROM_AI);
	if (d->m_deathAnim.any())
		me->setModelConditionFlags(d->m_deathAnim);
	if (d->m_deathFX)
		FXList::doFXObj(d->m_deathFX, me);
	if (getenv("BFME_DEBUG_RESPAWN"))
		printf("[respawn] #%u %s has fallen (revive: cost %d, %u frames, auto=%d)\n", me->getID(), me->getTemplate()->getName().str(),
			currentRule().m_cost, currentRule().m_time, (int)currentRule().m_autoSpawn);
	setWakeFrame(me, UPDATE_SLEEP(max(1u, d->m_deathAnimationTime)));
}

UpdateSleepTime RespawnUpdate::update()
{
	const RespawnUpdateModuleData* d = getRespawnUpdateModuleData();
	Object* me = getObject();
	const UnsignedInt now = TheGameLogic->getFrame();
	if (m_state == STATE_DYING)
	{
		// gone from the battlefield until revived
		if (d->m_deathAnim.any())
			me->clearModelConditionFlags(d->m_deathAnim);
		me->setStatus(MAKE_OBJECT_STATUS_MASK4(OBJECT_STATUS_UNSELECTABLE, OBJECT_STATUS_MASKED, OBJECT_STATUS_NO_COLLISIONS, OBJECT_STATUS_BFME_NOT_IN_WORLD));
		me->setModelConditionState(MODELCONDITION_HIDDEN);
		m_state = STATE_DEAD;
		m_stateFrame = now;
		const RespawnRule r = currentRule();
		Player* p = me->getControllingPlayer();
		// computer players revive their heroes as soon as they can pay (until fortress revive is wired to their AI)
		if (r.m_autoSpawn || (p && p->getPlayerType() == PLAYER_COMPUTER))
			return UPDATE_SLEEP(max(1u, r.m_time));
		return UPDATE_SLEEP_FOREVER;
	}
	if (m_state == STATE_DEAD)
	{
		const RespawnRule r = currentRule();
		Player* p = me->getControllingPlayer();
		if (!r.m_autoSpawn && p)
		{
			if (p->getMoney()->countMoney() < (UnsignedInt)max(0, r.m_cost))
				return UPDATE_SLEEP(LOGICFRAMES_PER_SECOND * 5);
			p->getMoney()->withdraw(max(0, r.m_cost));
		}
		// AutoRespawnAtObjectFilter: come back at the nearest matching building of ours
		const Coord3D* where = nullptr;
		if (!d->m_autoRespawnAt.isEmpty())
		{
			Real best = 0.0f;
			for (Object* o = TheGameLogic->getFirstObject(); o; o = o->getNextObject())
			{
				if (o == me || o->getControllingPlayer() != p || o->isEffectivelyDead() || !d->m_autoRespawnAt.matches(o, me))
					continue;
				const Real dx = o->getPosition()->x - me->getPosition()->x, dy = o->getPosition()->y - me->getPosition()->y;
				if (where == nullptr || dx * dx + dy * dy < best)
				{
					where = o->getPosition();
					best = dx * dx + dy * dy;
				}
			}
		}
		revive(where);
		return UPDATE_SLEEP_FOREVER;
	}
	return UPDATE_SLEEP_FOREVER;
}

Object* RespawnUpdate::revive(const Coord3D* pos)
{
	const RespawnUpdateModuleData* d = getRespawnUpdateModuleData();
	Object* me = getObject();
	if (m_state == STATE_ALIVE)
		return me;
	const RespawnRule r = currentRule();
	Coord3D at = pos ? *pos : *me->getPosition();
	at.z = TheTerrainLogic->getGroundHeight(at.x, at.y);

	me->clearStatus(MAKE_OBJECT_STATUS_MASK4(OBJECT_STATUS_UNSELECTABLE, OBJECT_STATUS_MASKED, OBJECT_STATUS_NO_COLLISIONS, OBJECT_STATUS_BFME_NOT_IN_WORLD));
	me->clearModelConditionState(MODELCONDITION_HIDDEN);
	if (d->m_deathAnim.any())
		me->clearModelConditionFlags(d->m_deathAnim);
	me->setPosition(&at);
	BodyModuleInterface* body = me->getBodyModule();
	if (body)
		body->internalChangeHealth(body->getMaxHealth() * max(0.01f, r.m_health) - body->getHealth());
	m_state = STATE_ALIVE;
	m_stateFrame = TheGameLogic->getFrame();
	if (d->m_respawnAnim.any())
		TheAttributeModifierStore->applyTimedModelConditions(me, d->m_respawnAnim, max(1u, d->m_respawnAnimationTime));
	if (d->m_respawnFX)
		FXList::doFXObj(d->m_respawnFX, me);
	if (getenv("BFME_DEBUG_RESPAWN"))
		printf("[respawn] #%u %s revived at %.0f,%.0f\n", me->getID(), me->getTemplate()->getName().str(), at.x, at.y);

	if (d->m_respawnAsTemplate.isNotEmpty())
	{
		const ThingTemplate* t = TheThingFactory->findTemplate(d->m_respawnAsTemplate);
		if (t)
			return bfmeReplaceObject(me, t, nullptr);
	}
	return me;
}

//=================================================================================================
// ExperienceLevelCreate
//=================================================================================================
void ExperienceLevelCreateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	BehaviorModuleData::buildFieldParse(p);
	static const FieldParse fp[] =
	{
		{ "LevelToGrant",	INI::parseInt,	nullptr, offsetof( ExperienceLevelCreateModuleData, m_levelToGrant ) },
		{ "MPOnly",				INI::parseBool,	nullptr, offsetof( ExperienceLevelCreateModuleData, m_mpOnly ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fp);
}

ExperienceLevelCreate::ExperienceLevelCreate( Thing *thing, const ModuleData* moduleData ) : BehaviorModule( thing, moduleData ) {}
BFME_TRIVIAL_SNAPSHOT(ExperienceLevelCreate, BehaviorModule)

void ExperienceLevelCreate::onCreate()
{
	// onCreate runs before initObject gives rank 1, so set the experience and let the rank follow
	ExperienceTracker* xp = getObject()->getExperienceTracker();
	const Int want = getExperienceLevelCreateModuleData()->m_levelToGrant;
	const ExperienceLevel* l = TheExperienceLevelStore ? TheExperienceLevelStore->getLevel(getObject()->getTemplate(), want) : nullptr;
	if (xp && l && xp->getCurrentExperience() < l->m_requiredExperience)
		xp->setExperienceAndLevel(l->m_requiredExperience, FALSE);
}

//=================================================================================================
// InheritUpgradeCreate
//=================================================================================================
void InheritUpgradeCreateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	BehaviorModuleData::buildFieldParse(p);
	static const FieldParse fp[] =
	{
		{ "Radius",				INI::parseReal,											nullptr, offsetof( InheritUpgradeCreateModuleData, m_radius ) },
		{ "Upgrade",			INI::parseAsciiStringVectorAppend,	nullptr, offsetof( InheritUpgradeCreateModuleData, m_upgrades ) },
		{ "ObjectFilter",	ObjectFilter::parseFromINI,					nullptr, offsetof( InheritUpgradeCreateModuleData, m_filter ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fp);
}

InheritUpgradeCreate::InheritUpgradeCreate( Thing *thing, const ModuleData* moduleData ) : BehaviorModule( thing, moduleData ) {}
BFME_TRIVIAL_SNAPSHOT(InheritUpgradeCreate, BehaviorModule)

void InheritUpgradeCreate::onBuildComplete()
{
	const InheritUpgradeCreateModuleData* d = getInheritUpgradeCreateModuleData();
	Object* me = getObject();
	for (size_t u = 0; u < d->m_upgrades.size(); ++u)
	{
		const UpgradeTemplate* ut = TheUpgradeCenter->findUpgrade(d->m_upgrades[u]);
		if (ut == nullptr || me->hasUpgrade(ut))
			continue;
		SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(me, d->m_radius, FROM_CENTER_2D);
		MemoryPoolObjectHolder hold(iter);
		for (Object* o = iter->first(); o; o = iter->next())
		{
			if (o != me && o->getControllingPlayer() == me->getControllingPlayer() && o->hasUpgrade(ut) && d->m_filter.matches(o, me))
			{
				me->giveUpgrade(ut);
				break;
			}
		}
	}
}

//=================================================================================================
// RefundDie
//=================================================================================================
void RefundDieModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	DieModuleData::buildFieldParse(p);
	static const FieldParse fp[] =
	{
		{ "RefundPercent",		INI::parsePercentToReal,		nullptr, offsetof( RefundDieModuleData, m_refundPercent ) },
		{ "UpgradeRequired",	INI::parseAsciiString,			nullptr, offsetof( RefundDieModuleData, m_upgradeRequired ) },
		{ "BuildingRequired",	ObjectFilter::parseFromINI,	nullptr, offsetof( RefundDieModuleData, m_buildingRequired ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fp);
}

RefundDie::RefundDie( Thing *thing, const ModuleData* moduleData ) : DieModule( thing, moduleData ) {}
void RefundDie::crc( Xfer *xfer ) { DieModule::crc( xfer ); }
void RefundDie::xfer( Xfer *xfer ) { XferVersion cv = 1; XferVersion v = cv; xfer->xferVersion( &v, cv ); DieModule::xfer( xfer ); }
void RefundDie::loadPostProcess() { DieModule::loadPostProcess(); }
RefundDie::~RefundDie() {}

void RefundDie::onDie( const DamageInfo *damageInfo )
{
	if (!isDieApplicable(damageInfo))
		return;
	const RefundDieModuleData* d = getRefundDieModuleData();
	Object* me = getObject();
	Player* p = me->getControllingPlayer();
	if (p == nullptr || d->m_refundPercent <= 0.0f)
		return;
	if (d->m_upgradeRequired.isNotEmpty() && !objOrPlayerHasUpgrade(me, d->m_upgradeRequired))
		return;
	if (!d->m_buildingRequired.isEmpty())
	{
		Bool found = FALSE;
		for (Object* o = TheGameLogic->getFirstObject(); o && !found; o = o->getNextObject())
			if (o != me && o->getControllingPlayer() == p && !o->isEffectivelyDead() && d->m_buildingRequired.matches(o, me))
				found = TRUE;
		if (!found)
			return;
	}
	const Int cost = me->getTemplate()->calcCostToBuild(p);
	if (cost > 0)
		p->getMoney()->deposit((UnsignedInt)(cost * d->m_refundPercent));
}

//=================================================================================================
// AttachUpdate
//=================================================================================================
void AttachUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse fp[] =
	{
		{ "ObjectFilter",									ObjectFilter::parseFromINI,	nullptr, offsetof( AttachUpdateModuleData, m_filter ) },
		{ "ScanRange",										INI::parseReal,							nullptr, offsetof( AttachUpdateModuleData, m_scanRange ) },
		{ "AlwaysTeleport",								INI::parseBool,							nullptr, offsetof( AttachUpdateModuleData, m_alwaysTeleport ) },
		{ "AnchorToTopOfGeometry",				INI::parseBool,							nullptr, offsetof( AttachUpdateModuleData, m_anchorToTop ) },
		{ "ParentStatus",									INI::parseAsciiString,			nullptr, offsetof( AttachUpdateModuleData, m_unusedString ) },
		{ "ParentOwnerAttachmentEvaEvent",	INI::parseAsciiString,			nullptr, offsetof( AttachUpdateModuleData, m_unusedString ) },
		{ "ParentEnemyAttachmentEvaEvent",	INI::parseAsciiString,			nullptr, offsetof( AttachUpdateModuleData, m_unusedString ) },
		{ "ParentOwnerDiedEvaEvent",			INI::parseAsciiString,			nullptr, offsetof( AttachUpdateModuleData, m_unusedString ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fp);
}

AttachUpdate::AttachUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData ),
	m_parent(INVALID_ID), m_searched(FALSE)
{
	setWakeFrame(getObject(), UPDATE_SLEEP_NONE);
}

void AttachUpdate::crc( Xfer *xfer ) { UpdateModule::crc( xfer ); }
void AttachUpdate::xfer( Xfer *xfer )
{
	XferVersion cv = 1;
	XferVersion v = cv;
	xfer->xferVersion( &v, cv );
	UpdateModule::xfer( xfer );
	xfer->xferObjectID(&m_parent);
	xfer->xferBool(&m_searched);
}
void AttachUpdate::loadPostProcess() { UpdateModule::loadPostProcess(); }
AttachUpdate::~AttachUpdate() {}

UpdateSleepTime AttachUpdate::update()
{
	const AttachUpdateModuleData* d = getAttachUpdateModuleData();
	Object* me = getObject();
	if (!m_searched)
	{
		m_searched = TRUE;
		Object* best = nullptr;
		Real bestD = 0.0f;
		SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(me, d->m_scanRange, FROM_CENTER_2D);
		MemoryPoolObjectHolder hold(iter);
		for (Object* o = iter->first(); o; o = iter->next())
		{
			if (o == me || o->isEffectivelyDead() || o->isKindOf(KINDOF_HORDE) || !d->m_filter.matches(o, me))
				continue;
			const Real dist = ThePartitionManager->getDistanceSquared(me, o, FROM_CENTER_2D);
			if (best == nullptr || dist < bestD)
			{
				best = o;
				bestD = dist;
			}
		}
		if (best == nullptr)
		{
			TheGameLogic->destroyObject(me);
			return UPDATE_SLEEP_FOREVER;
		}
		m_parent = best->getID();
	}
	Object* parent = TheGameLogic->findObjectByID(m_parent);
	if (parent == nullptr || parent->isEffectivelyDead())
	{
		TheGameLogic->destroyObject(me);
		return UPDATE_SLEEP_FOREVER;
	}
	Coord3D pos = *parent->getPosition();
	if (d->m_anchorToTop)
		pos.z += parent->getGeometryInfo().getMaxHeightAbovePosition();
	me->setPosition(&pos);
	return UPDATE_SLEEP_NONE;
}

//=================================================================================================
// DualWeaponBehavior
//=================================================================================================
void DualWeaponBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse fp[] =
	{
		{ "SwitchWeaponOnCloseRangeDistance",	INI::parseReal,	nullptr, offsetof( DualWeaponBehaviorModuleData, m_closeRange ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fp);
}

DualWeaponBehavior::DualWeaponBehavior( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData ), m_close(FALSE)
{
	setWakeFrame(getObject(), UPDATE_SLEEP(LOGICFRAMES_PER_SECOND / 3));
}
BFME_TRIVIAL_SNAPSHOT(DualWeaponBehavior, UpdateModule)

UpdateSleepTime DualWeaponBehavior::update()
{
	Object* me = getObject();
	const UpdateSleepTime later = UPDATE_SLEEP(LOGICFRAMES_PER_SECOND / 3);
	if (me->isEffectivelyDead())
		return UPDATE_SLEEP_FOREVER;
	AIUpdateInterface* ai = me->getAI();
	Object* victim = ai ? ai->getCurrentVictim() : nullptr;
	if (victim == nullptr)
	{
		if (m_close)
		{
			me->clearWeaponSetFlag(WEAPONSET_BFME_CLOSE_RANGE);
			m_close = FALSE;
		}
		return later;
	}
	const Real r = getDualWeaponBehaviorModuleData()->m_closeRange;
	const Bool close = ThePartitionManager->getDistanceSquared(me, victim, FROM_BOUNDINGSPHERE_2D) <= r * r;
	if (close != m_close)
	{
		// the CLOSE_RANGE weapon set holds the melee weapon
		if (close)
			me->setWeaponSetFlag(WEAPONSET_BFME_CLOSE_RANGE);
		else
			me->clearWeaponSetFlag(WEAPONSET_BFME_CLOSE_RANGE);
		m_close = close;
	}
	return later;
}

//=================================================================================================
// MonitorConditionUpdate
//=================================================================================================
void MonitorConditionUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse fp[] =
	{
		{ "WeaponSetFlags",						WeaponSetFlags::parseFromINI,				nullptr, offsetof( MonitorConditionUpdateModuleData, m_weaponSetFlags ) },
		{ "WeaponToggleCommandSet",		INI::parseAsciiString,							nullptr, offsetof( MonitorConditionUpdateModuleData, m_weaponToggleCommandSet ) },
		{ "ModelConditionFlags",			ModelConditionFlags::parseFromINI,	nullptr, offsetof( MonitorConditionUpdateModuleData, m_modelConditionFlags ) },
		{ "ModelConditionCommandSet",	INI::parseAsciiString,							nullptr, offsetof( MonitorConditionUpdateModuleData, m_modelConditionCommandSet ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fp);
}

MonitorConditionUpdate::MonitorConditionUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData ), m_active(FALSE)
{
	setWakeFrame(getObject(), UPDATE_SLEEP(1 + getObject()->getID() % 5));
}

void MonitorConditionUpdate::crc( Xfer *xfer ) { UpdateModule::crc( xfer ); }
void MonitorConditionUpdate::xfer( Xfer *xfer )
{
	XferVersion cv = 1;
	XferVersion v = cv;
	xfer->xferVersion( &v, cv );
	UpdateModule::xfer( xfer );
	xfer->xferBool( &m_active );
	xfer->xferAsciiString( &m_previous );
	xfer->xferAsciiString( &m_applied );
}
void MonitorConditionUpdate::loadPostProcess() { UpdateModule::loadPostProcess(); }
MonitorConditionUpdate::~MonitorConditionUpdate() {}

UpdateSleepTime MonitorConditionUpdate::update()
{
	const MonitorConditionUpdateModuleData* d = getMonitorConditionUpdateModuleData();
	Object* me = getObject();
	const Drawable* draw = me->getDrawable();
	AsciiString wanted;
	if (d->m_weaponSetFlags.any() && me->getWeaponSetFlags().testForAll(d->m_weaponSetFlags))
		wanted = d->m_weaponToggleCommandSet;
	else if (d->m_modelConditionFlags.any() && draw && draw->getModelConditionFlags().testForAll(d->m_modelConditionFlags))
		wanted = d->m_modelConditionCommandSet;
	if (wanted.isNotEmpty() && !m_active)
	{
		m_previous = me->getCommandSetString();
		me->setCommandSetStringOverride(wanted);
		m_applied = wanted;
		m_active = TRUE;
	}
	else if (wanted.isEmpty() && m_active)
	{
		// undo only our own change (another monitor may have taken over meanwhile)
		if (me->getCommandSetString() == m_applied)
			me->setCommandSetStringOverride(m_previous);
		m_active = FALSE;
	}
	return UPDATE_SLEEP(5);
}

//=================================================================================================
// BannerCarrierUpdate: horde reinforcement
//=================================================================================================
static void parseBannerIgnore(INI* ini, void*, void*, const void*)
{
	while (ini->getNextTokenOrNull() != nullptr) {}
}

BannerCarrierUpdateModuleData::BannerCarrierUpdateModuleData() :
	m_idleSpawnRate(LOGICFRAMES_PER_SECOND * 10), m_meleeFreeUnitSpawnTime(0), m_diedRespawnTime(0),
	m_meleeFreeBannerRespawnTime(0), m_unitSpawnFX(nullptr), m_bannerMorphFX(nullptr),
	m_replenishNearbyHorde(FALSE), m_replenishAllNearbyHordes(FALSE), m_scanHordeDistance(0.0f)
{
}

void BannerCarrierUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse fp[] =
	{
		{ "IdleSpawnRate",							INI::parseDurationUnsignedInt,	nullptr, offsetof( BannerCarrierUpdateModuleData, m_idleSpawnRate ) },
		{ "MeleeFreeUnitSpawnTime",			INI::parseDurationUnsignedInt,	nullptr, offsetof( BannerCarrierUpdateModuleData, m_meleeFreeUnitSpawnTime ) },
		{ "DiedRespawnTime",						INI::parseDurationUnsignedInt,	nullptr, offsetof( BannerCarrierUpdateModuleData, m_diedRespawnTime ) },
		{ "MeleeFreeBannerReSpawnTime",	INI::parseDurationUnsignedInt,	nullptr, offsetof( BannerCarrierUpdateModuleData, m_meleeFreeBannerRespawnTime ) },
		{ "UnitSpawnFX",								INI::parseFXList,								nullptr, offsetof( BannerCarrierUpdateModuleData, m_unitSpawnFX ) },
		{ "BannerMorphFX",							INI::parseFXList,								nullptr, offsetof( BannerCarrierUpdateModuleData, m_bannerMorphFX ) },
		{ "ReplenishNearbyHorde",				INI::parseBool,									nullptr, offsetof( BannerCarrierUpdateModuleData, m_replenishNearbyHorde ) },
		{ "ReplenishAllNearbyHordes",		INI::parseBool,									nullptr, offsetof( BannerCarrierUpdateModuleData, m_replenishAllNearbyHordes ) },
		{ "ScanHordeDistance",					INI::parseReal,									nullptr, offsetof( BannerCarrierUpdateModuleData, m_scanHordeDistance ) },
		{ "UpgradeRequired",						INI::parseAsciiString,					nullptr, offsetof( BannerCarrierUpdateModuleData, m_upgradeRequired ) },
		{ "MorphCondition",							parseBannerIgnore,							nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fp);
}

BannerCarrierUpdate::BannerCarrierUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	const BannerCarrierUpdateModuleData* d = getBannerCarrierUpdateModuleData();
	if (d->m_replenishNearbyHorde || d->m_replenishAllNearbyHordes)
		setWakeFrame(getObject(), UPDATE_SLEEP(1 + getObject()->getID() % max(1u, d->m_idleSpawnRate)));
	else
		setWakeFrame(getObject(), UPDATE_SLEEP_FOREVER);
}
BFME_TRIVIAL_SNAPSHOT(BannerCarrierUpdate, UpdateModule)

const BannerCarrierUpdateModuleData* BannerCarrierUpdate::findOn(const Object* obj)
{
	if (obj == nullptr)
		return nullptr;
	for (BehaviorModule** m = obj->getBehaviorModules(); *m; ++m)
	{
		BannerCarrierUpdate* b = dynamic_cast<BannerCarrierUpdate*>(*m);
		if (b)
			return b->data();
	}
	return nullptr;
}

UpdateSleepTime BannerCarrierUpdate::update()
{
	const BannerCarrierUpdateModuleData* d = getBannerCarrierUpdateModuleData();
	const UnsignedInt delay = max(1u, d->m_idleSpawnRate);
	Object* me = getObject();
	if (me->isEffectivelyDead())
		return UPDATE_SLEEP_FOREVER;
	if (!(d->m_replenishNearbyHorde || d->m_replenishAllNearbyHordes))
		return UPDATE_SLEEP_FOREVER;
	if (me->getStatusBits().test(OBJECT_STATUS_UNDER_CONSTRUCTION))
		return UPDATE_SLEEP(delay);
	if (d->m_upgradeRequired.isNotEmpty() && !objOrPlayerHasUpgrade(me, d->m_upgradeRequired))
		return UPDATE_SLEEP(delay);

	SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(me, d->m_scanHordeDistance, FROM_BOUNDINGSPHERE_2D);
	MemoryPoolObjectHolder hold(iter);
	for (Object* o = iter->first(); o; o = iter->next())
	{
		if (o->isEffectivelyDead() || !o->isKindOf(KINDOF_HORDE))
			continue;
		if (d->m_replenishAllNearbyHordes ? me->getRelationship(o) != ALLIES : o->getControllingPlayer() != me->getControllingPlayer())
			continue;
		HordeContain* h = HordeContain::findOn(o);
		if (h && h->bfmeReplenishOne(d->m_unitSpawnFX, FALSE))
			if (!d->m_replenishAllNearbyHordes)
				break;
	}
	return UPDATE_SLEEP(delay);
}

//=================================================================================================
// AISpecialPowerUpdate: computer players use their units' abilities
//=================================================================================================
void AISpecialPowerUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse fp[] =
	{
		{ "CommandButtonName",				INI::parseAsciiString,	nullptr, offsetof( AISpecialPowerUpdateModuleData, m_commandButtonName ) },
		{ "SpecialPowerAIType",				INI::parseAsciiString,	nullptr, offsetof( AISpecialPowerUpdateModuleData, m_aiType ) },
		{ "SpecialPowerRadius",				INI::parseReal,					nullptr, offsetof( AISpecialPowerUpdateModuleData, m_radius ) },
		{ "SpecialPowerRange",				INI::parseReal,					nullptr, offsetof( AISpecialPowerUpdateModuleData, m_range ) },
		{ "RandomizeTargetLocation",	INI::parseBool,					nullptr, offsetof( AISpecialPowerUpdateModuleData, m_randomize ) },
		{ "SpellMakesAStructure",			parseBannerIgnore,			nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fp);
}

enum AIPowerKind
{
	AIPOWER_NONE,					///< stances, toggles, building capture, spell book: not handled here
	AIPOWER_SELF_BUFF,		///< use in a fight
	AIPOWER_HEAL,					///< use when we or friends nearby are hurt
	AIPOWER_KILL_UNIT,		///< an enemy unit as target
	AIPOWER_KILL_STRUCTURE,
	AIPOWER_AREA,					///< a spot among the enemies
};

static Int classifyAIPower(const AsciiString& t)
{
	const char* s = t.str();
	if (strstr(s, "STANCE") || strstr(s, "TOGGLE") || strstr(s, "CAPTURE") || strstr(s, "SPELLBOOK") || strstr(s, "TRAINARCHERS"))
		return AIPOWER_NONE;
	if (strstr(s, "HEAL"))
		return strstr(s, "ATTACK_HEAL") ? AIPOWER_AREA : AIPOWER_HEAL;
	if (strstr(s, "STRUCTURES"))
		return AIPOWER_KILL_STRUCTURE;
	if (strstr(s, "KILLER") || strstr(s, "DOMINATE") || strstr(s, "MORGUL_BLADE") || strstr(s, "SOUL_FREEZE") || strstr(s, "GOBLIN_POISON"))
		return AIPOWER_KILL_UNIT;
	if (strstr(s, "AOE_ATTACK") || strstr(s, "SUMMON") || strstr(s, "WIZARD_BLAST") || strstr(s, "ARROWWIND") || strstr(s, "CHARGE") || strstr(s, "CALLOFTHEDEEP"))
		return AIPOWER_AREA;
	if (strstr(s, "BUFF") || strstr(s, "GIVEXP") || strstr(s, "ELENDIL") || strstr(s, "MOUNTED") || strstr(s, "AOE_AND_BUFF") || strstr(s, "DEBUFF"))
		return AIPOWER_SELF_BUFF;
	return AIPOWER_SELF_BUFF;
}

AISpecialPowerUpdate::AISpecialPowerUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData ), m_kind(-1)
{
	setWakeFrame(getObject(), UPDATE_SLEEP(LOGICFRAMES_PER_SECOND + getObject()->getID() % LOGICFRAMES_PER_SECOND));
}
BFME_TRIVIAL_SNAPSHOT(AISpecialPowerUpdate, UpdateModule)

UpdateSleepTime AISpecialPowerUpdate::update()
{
	const AISpecialPowerUpdateModuleData* d = getAISpecialPowerUpdateModuleData();
	const UpdateSleepTime later = UPDATE_SLEEP(LOGICFRAMES_PER_SECOND);
	Object* me = getObject();
	if (me->isEffectivelyDead())
		return UPDATE_SLEEP_FOREVER;
	if (m_kind < 0)
		m_kind = classifyAIPower(d->m_aiType);
	if (m_kind == AIPOWER_NONE)
		return UPDATE_SLEEP_FOREVER;
	Player* p = me->getControllingPlayer();
	if (p == nullptr || p->getPlayerType() != PLAYER_COMPUTER)
		return UPDATE_SLEEP(LOGICFRAMES_PER_SECOND * 5);
	if (me->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION) || me->isDisabled())
		return later;

	const CommandButton* b = TheControlBar ? TheControlBar->findCommandButton(d->m_commandButtonName) : nullptr;
	const SpecialPowerTemplate* sp = b ? b->getSpecialPowerTemplate() : nullptr;
	SpecialPowerModuleInterface* spm = sp ? me->getSpecialPowerModule(sp) : nullptr;
	if (spm == nullptr)
		return UPDATE_SLEEP(LOGICFRAMES_PER_SECOND * 5);	// may come with a level or an upgrade
	if (!spm->isReady())
		return later;
	for (BehaviorModule** m = me->getBehaviorModules(); *m; ++m)
	{
		SpecialPowerUpdateInterface* su = (*m)->getSpecialPowerUpdateInterface();
		if (su && su->isActive())
			return later;
	}
	// level-gated buttons stay locked until the unit has the science/level
	if (b->getScienceVec().size() > 0 && !p->hasScience(b->getScienceVec()[0]))
		return later;

	const Real scan = d->m_range > 0.0f ? d->m_range + 50.0f : (d->m_radius > 0.0f ? d->m_radius * 1.5f : 150.0f);
	AIUpdateInterface* ai = me->getAI();
	Object* victim = ai ? ai->getCurrentVictim() : nullptr;
	Object* closest = nullptr;
	Real closestD = 0.0f;
	Int enemies = 0;
	Bool hurtFriend = FALSE;
	SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(me, scan, FROM_CENTER_2D);
	MemoryPoolObjectHolder hold(iter);
	for (Object* o = iter->first(); o; o = iter->next())
	{
		if (o->isEffectivelyDead() || o->isKindOf(KINDOF_HORDE) || o->isKindOf(KINDOF_UNATTACKABLE))
			continue;
		const Relationship r = me->getRelationship(o);
		if (r == ALLIES)
		{
			BodyModuleInterface* body = o->getBodyModule();
			if (body && o->isKindOf(KINDOF_CAN_ATTACK) && body->getHealth() < body->getMaxHealth() * 0.6f)
				hurtFriend = TRUE;
			continue;
		}
		if (r != ENEMIES || o->getControllingPlayer() == nullptr)
			continue;
		const Bool structure = o->isKindOf(KINDOF_STRUCTURE);
		if ((m_kind == AIPOWER_KILL_STRUCTURE) != structure)
			continue;
		if (!structure && !o->isKindOf(KINDOF_CAN_ATTACK))
			continue;
		++enemies;
		const Real dd = ThePartitionManager->getDistanceSquared(me, o, FROM_CENTER_2D);
		if (closest == nullptr || dd < closestD)
		{
			closest = o;
			closestD = dd;
		}
	}
	// prefer what we are already fighting
	Object* target = closest;
	if (victim && !victim->isEffectivelyDead() && me->getRelationship(victim) == ENEMIES &&
			(m_kind != AIPOWER_KILL_STRUCTURE) == !victim->isKindOf(KINDOF_STRUCTURE) &&
			ThePartitionManager->getDistanceSquared(me, victim, FROM_CENTER_2D) <= scan * scan)
	{
		HordeContain* h = victim->isKindOf(KINDOF_HORDE) ? HordeContain::findOn(victim) : nullptr;
		Object* t = h ? h->getMemberClosestTo(me->getPosition()) : victim;
		if (t)
			target = t;
	}

	Bool use = FALSE;
	switch (m_kind)
	{
		case AIPOWER_SELF_BUFF:			use = enemies >= 2 || (enemies >= 1 && ai && ai->isAttacking()); break;
		case AIPOWER_HEAL:					use = hurtFriend; break;
		case AIPOWER_KILL_UNIT:
		case AIPOWER_KILL_STRUCTURE:	use = target != nullptr; break;
		case AIPOWER_AREA:					use = target != nullptr && enemies >= 2; break;
	}
	if (!use)
		return later;

	const UnsignedInt aim = b->getOptions() & (NEED_TARGET_ENEMY_OBJECT | NEED_TARGET_ALLY_OBJECT | NEED_TARGET_NEUTRAL_OBJECT | NEED_TARGET_POS);
	if (aim & NEED_TARGET_ENEMY_OBJECT)
	{
		if (target == nullptr)
			return later;
		spm->doSpecialPowerAtObject(target, 0);
	}
	else if (aim & (NEED_TARGET_ALLY_OBJECT | NEED_TARGET_NEUTRAL_OBJECT))
		spm->doSpecialPowerAtObject(me, 0);
	else if (aim & NEED_TARGET_POS)
	{
		Coord3D pos = target ? *target->getPosition() : *me->getPosition();
		if (d->m_randomize && d->m_radius > 0.0f)
		{
			pos.x += GameLogicRandomValueReal(-d->m_radius, d->m_radius) * 0.3f;
			pos.y += GameLogicRandomValueReal(-d->m_radius, d->m_radius) * 0.3f;
		}
		spm->doSpecialPowerAtLocation(&pos, 0.0f, 0);
	}
	else
		spm->doSpecialPower(0);
	if (getenv("BFME_DEBUG_ABILITY"))
		printf("[ability] AI #%u %s used %s (%s) target #%u enemies %d\n", me->getID(), me->getTemplate()->getName().str(),
			d->m_commandButtonName.str(), d->m_aiType.str(), target ? target->getID() : 0, enemies);
	return UPDATE_SLEEP(LOGICFRAMES_PER_SECOND * 2);
}

