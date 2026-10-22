// bfme-native: RotWK castles and camps (CastleBehavior) and building on their plots
// (FOUNDATION_CONSTRUCT). See BfmeModules.h.
#include "PreRTS.h"

#include "Common/BuildAssistant.h"
#include "Common/DataChunk.h"
#include "Common/MapReaderWriterInfo.h"
#include "Common/Money.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/PlayerTemplate.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameClient/TerrainVisual.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/BfmeModules.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/CreateModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"

static Bool debugCastle() { static const Bool d = getenv("BFME_DEBUG_CASTLE") != nullptr; return d; }

//=================================================================================================
// base files
//=================================================================================================
struct BfmeBaseEntry
{
	AsciiString m_template;
	Coord3D m_offset;
	Real m_angle;
};

static std::vector<BfmeBaseEntry>* s_readBase = nullptr;

static Bool parseCastleTemplatesChunk(DataChunkInput &file, DataChunkInfo *info, void *)
{
	file.readInt();	// property key
	const Int count = file.readInt();
	for (Int i = 0; i < count && !file.atEndOfChunk(); ++i)
	{
		BfmeBaseEntry e;
		file.readAsciiString();	// entry name
		e.m_template = file.readAsciiString();
		e.m_offset.x = file.readReal();
		e.m_offset.y = file.readReal();
		e.m_offset.z = file.readReal();
		e.m_angle = file.readReal();
		file.readInt();	// priority
		file.readInt();	// phase
		s_readBase->push_back(e);
	}
	return true;
}

static const std::vector<BfmeBaseEntry>* loadBase(const AsciiString& name)
{
	static std::map<std::string, std::vector<BfmeBaseEntry> > cache;
	std::map<std::string, std::vector<BfmeBaseEntry> >::iterator it = cache.find(name.str());
	if (it != cache.end())
		return &it->second;
	std::vector<BfmeBaseEntry>& entries = cache[name.str()];
	AsciiString path;
	path.format("bases\\%s\\%s.bse", name.str(), name.str());
	CachedFileInputStream stream;
	if (stream.open(path))
	{
		DataChunkInput file(&stream);
		file.registerParser("CastleTemplates", AsciiString::TheEmptyString, parseCastleTemplatesChunk);
		s_readBase = &entries;
		file.parse(nullptr);
		s_readBase = nullptr;
	}
	if (debugCastle())
		fprintf(stderr, "[castle] base %s: %d entries\n", path.str(), (int)entries.size());
	return &entries;
}

//=================================================================================================
// CastleBehavior
//=================================================================================================
static void parseCastleIgnore(INI* ini, void*, void*, const void*)
{
	while (ini->getNextTokenOrNull() != nullptr) {}
}

void CastleBehaviorModuleData::parseCastleEntry(INI* ini, void* instance, void*, const void*)
{
	CastleBehaviorModuleData* self = (CastleBehaviorModuleData*)instance;
	BfmeCastleEntry e;
	e.m_faction = ini->getNextToken();
	e.m_base = ini->getNextToken();
	const char* cost = ini->getNextTokenOrNull();
	e.m_cost = cost ? INI::scanInt(cost) : 0;
	self->m_castles.push_back(e);
}

void CastleBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse fp[] =
	{
		{ "CastleToUnpackForFaction",		parseCastleEntry,	nullptr, 0 },
		{ "MaxCastleRadius",						INI::parseReal,		nullptr, offsetof( CastleBehaviorModuleData, m_maxCastleRadius ) },
		{ "InstantUnpack",							INI::parseBool,		nullptr, offsetof( CastleBehaviorModuleData, m_instantUnpack ) },
		{ "FilterValidOwnedEntries",		parseCastleIgnore,	nullptr, 0 },
		{ "ScanDistance",								parseCastleIgnore,	nullptr, 0 },
		{ "EvaEnemyCastleSightedEvent",	parseCastleIgnore,	nullptr, 0 },
		{ "FadeTime",										parseCastleIgnore,	nullptr, 0 },
		{ "KeepDeathKillsEverything",		parseCastleIgnore,	nullptr, 0 },
		{ "PreBuiltList",								parseCastleIgnore,	nullptr, 0 },
		{ "PreBuiltPlyr",								parseCastleIgnore,	nullptr, 0 },
		{ "UnpackDelayTime",						parseCastleIgnore,	nullptr, 0 },
		{ "FilterCrew",									parseCastleIgnore,	nullptr, 0 },
		{ "CrewReleaseFX",							parseCastleIgnore,	nullptr, 0 },
		{ "CrewPrepareFX",							parseCastleIgnore,	nullptr, 0 },
		{ "CrewPrepareInterval",				parseCastleIgnore,	nullptr, 0 },
		{ "Summoned",										parseCastleIgnore,	nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fp);
}

CastleBehavior::CastleBehavior( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData ), m_unpacked(FALSE), m_startSite(FALSE)
{
	setWakeFrame(getObject(), UPDATE_SLEEP(LOGICFRAMES_PER_SECOND / 2));
}

CastleBehavior::~CastleBehavior() {}
void CastleBehavior::crc( Xfer *xfer ) { UpdateModule::crc(xfer); }
void CastleBehavior::xfer( Xfer *xfer )
{
	XferVersion cv = 1, v = cv;
	xfer->xferVersion(&v, cv);
	UpdateModule::xfer(xfer);
	xfer->xferBool(&m_unpacked);
	xfer->xferBool(&m_startSite);
}
void CastleBehavior::loadPostProcess() { UpdateModule::loadPostProcess(); }

CastleBehavior* CastleBehavior::findOn(const Object* obj)
{
	if (obj == nullptr)
		return nullptr;
	for (BehaviorModule** m = obj->getBehaviorModules(); *m; ++m)
		if (CastleBehavior* c = dynamic_cast<CastleBehavior*>(*m))
			return c;
	return nullptr;
}

static Bool isRealPlayer(const Player* p)
{
	return p && p->isPlayableSide() && (p->getPlayerType() == PLAYER_HUMAN || p->getPlayerType() == PLAYER_COMPUTER) &&
		p->getPlayerTemplate() != nullptr;
}

const BfmeCastleEntry* CastleBehavior::entryFor(const Player* p) const
{
	if (p == nullptr)
		return nullptr;
	const CastleBehaviorModuleData* d = getCastleBehaviorModuleData();
	for (size_t i = 0; i < d->m_castles.size(); ++i)
		if (d->m_castles[i].m_faction.compareNoCase(p->getSide()) == 0)
			return &d->m_castles[i];
	return nullptr;
}

/// the player whose start position this site stands on
static Player* startOwnerOf(const Object* site, Real* distance = nullptr)
{
	if (ThePlayerList == nullptr || TheTerrainLogic == nullptr)
		return nullptr;
	Player* best = nullptr;
	Real bestD = 300.0f * 300.0f;
	for (Int i = 0; i < ThePlayerList->getPlayerCount(); ++i)
	{
		Player* p = ThePlayerList->getNthPlayer(i);
		if (!isRealPlayer(p) || p->getMpStartIndex() < 0)
			continue;
		AsciiString wp;
		wp.format("Player_%d_Start", p->getMpStartIndex() + 1);
		Waypoint* w = TheTerrainLogic->getWaypointByName(wp);
		if (w == nullptr)
			continue;
		const Real dx = w->getLocation()->x - site->getPosition()->x, dy = w->getLocation()->y - site->getPosition()->y;
		if (dx * dx + dy * dy < bestD)
		{
			bestD = dx * dx + dy * dy;
			best = p;
		}
	}
	if (distance)
		*distance = sqrtf(bestD);
	return best;
}

UpdateSleepTime CastleBehavior::update()
{
	Object* me = getObject();
	if (me->isEffectivelyDead() || m_unpacked)
		return UPDATE_SLEEP_FOREVER;
	Player* owner = me->getControllingPlayer();
	if (!isRealPlayer(owner))
	{
		// a site on a start position belongs to that player
		Real dist = 0.0f;
		Player* p = startOwnerOf(me, &dist);
		if (p && dist < 120.0f)
			m_startSite = TRUE;
		else
			p = nullptr;	// other sites are captured (bfmeCastleUpdate)
		if (p && p->getDefaultTeam())
		{
			me->setTeam(p->getDefaultTeam());
			owner = p;
			if (debugCastle())
				fprintf(stderr, "[castle] #%u %s goes to %s\n", me->getID(), me->getTemplate()->getName().str(),
					TheNameKeyGenerator->keyToName(p->getPlayerNameKey()).str());
		}
		else
			return UPDATE_SLEEP(LOGICFRAMES_PER_SECOND);
	}
	// castles, the start camp (a player starts with the base on his start position) and the
	// computer's camps unpack by themselves
	if (getCastleBehaviorModuleData()->m_instantUnpack || (m_startSite && TheGameLogic->getFrame() < 10 * LOGICFRAMES_PER_SECOND))
		unpack(FALSE);
	else if (owner->getPlayerType() == PLAYER_COMPUTER)
		unpack(TRUE);
	return m_unpacked ? UPDATE_SLEEP_FOREVER : UPDATE_SLEEP(LOGICFRAMES_PER_SECOND);
}

static void completeConstruction(Object* obj, Object* builder)
{
	obj->clearStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_UNDER_CONSTRUCTION));
	obj->clearStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_RECONSTRUCTING));
	obj->clearModelConditionFlags(MAKE_MODELCONDITION_MASK3(MODELCONDITION_AWAITING_CONSTRUCTION,
		MODELCONDITION_PARTIALLY_CONSTRUCTED, MODELCONDITION_ACTIVELY_BEING_CONSTRUCTED));
	obj->setConstructionPercent(CONSTRUCTION_COMPLETE);
	if (BodyModuleInterface* body = obj->getBodyModule())
		body->evaluateVisualCondition();
	if (Player* player = obj->getControllingPlayer())
	{
		player->onStructureConstructionComplete(builder, obj, FALSE);
		for (BehaviorModule** m = obj->getBehaviorModules(); *m; ++m)
			if (CreateModuleInterface* create = (*m)->getCreate())
				create->onBuildComplete();
	}
	obj->handlePartitionCellMaintenance();
	obj->updateUpgradeModules();
}

Bool CastleBehavior::unpack(Bool pay)
{
	Object* me = getObject();
	Player* owner = me->getControllingPlayer();
	if (m_unpacked || !isRealPlayer(owner))
		return FALSE;
	const BfmeCastleEntry* e = entryFor(owner);
	if (e == nullptr)
		return FALSE;
	const std::vector<BfmeBaseEntry>* base = loadBase(e->m_base);
	if (base->empty())
	{
		// a castle base missing from the data (Rohan in this Völker build): the faction's camp instead
		// (and a missing camp the castle: Isengard and Angmar only have castles)
		std::string alt = e->m_base.str();
		size_t k = alt.find("_castle");
		if (k != std::string::npos)
			base = loadBase(AsciiString((alt.substr(0, k) + "_camp" + alt.substr(k + 7)).c_str()));
		else if ((k = alt.find("_camp")) != std::string::npos)
			base = loadBase(AsciiString((alt.substr(0, k) + "_castle" + alt.substr(k + 5)).c_str()));
	}
	if (base->empty())
	{
		// no base for this faction in the data: a start site falls back to the free-build start
		// (the FreeBuildPorterEgg hatches porters once the player has the upgrade)
		if (m_startSite && !pay)
		{
			const UpgradeTemplate* fb = TheUpgradeCenter->findUpgrade("Upgrade_EvilDualEconomyChoice");
			if (fb && !owner->hasUpgradeComplete(fb))
				owner->addUpgrade(fb, UPGRADE_STATUS_COMPLETE);
			m_unpacked = TRUE;
			fprintf(stderr, "[bfme] no base %s for %s: free build instead\n", e->m_base.str(), owner->getSide().str());
		}
		return FALSE;
	}
	if (pay)
	{
		if ((Int)owner->getMoney()->countMoney() < e->m_cost)
			return FALSE;
		owner->getMoney()->withdraw(e->m_cost);
	}
	m_unpacked = TRUE;
	const Real a = me->getOrientation();
	const Real c = Cos(a), s = Sin(a);
	Int made = 0;
	for (size_t i = 0; i < base->size(); ++i)
	{
		const BfmeBaseEntry& be = (*base)[i];
		const ThingTemplate* t = TheThingFactory->findTemplate(be.m_template);
		if (t == nullptr)
			continue;
		Coord3D pos;
		pos.x = me->getPosition()->x + be.m_offset.x * c - be.m_offset.y * s;
		pos.y = me->getPosition()->y + be.m_offset.x * s + be.m_offset.y * c;
		pos.z = TheTerrainLogic->getGroundHeight(pos.x, pos.y);
		const Real angle = normalizeAngle(be.m_angle + a);
		if (t->isKindOf(KINDOF_PROP))
		{
			if (TheTerrainVisual)
				TheTerrainVisual->addProp(t, &pos, angle);
			continue;
		}
		Object* obj = TheThingFactory->newObject(t, owner->getDefaultTeam());
		if (obj == nullptr)
			continue;
		obj->setPosition(&pos);
		obj->setOrientation(angle);
		obj->setProducer(me);
		if (obj->isKindOf(KINDOF_STRUCTURE))
		{
			TheAI->pathfinder()->addObjectToPathfindMap(obj);
			owner->onStructureCreated(me, obj);
			completeConstruction(obj, me);
		}
		++made;
	}
	// the flag itself steps aside for the keep
	me->setModelConditionState(MODELCONDITION_USER_3);
	me->setStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_UNSELECTABLE));
	if (debugCastle())
		fprintf(stderr, "[castle] #%u %s unpacked %s for %s: %d objects\n", me->getID(), me->getTemplate()->getName().str(),
			e->m_base.str(), TheNameKeyGenerator->keyToName(owner->getPlayerNameKey()).str(), made);
	return TRUE;
}

//=================================================================================================
// build plots
//=================================================================================================
struct BfmePlotBuild
{
	ObjectID m_plot;
	ObjectID m_building;
};
static std::vector<BfmePlotBuild> s_plotBuilds;

Bool bfmeFoundationOccupied(const Object* foundation)
{
	for (size_t i = 0; i < s_plotBuilds.size(); ++i)
		if (s_plotBuilds[i].m_plot == foundation->getID())
		{
			const Object* b = TheGameLogic->findObjectByID(s_plotBuilds[i].m_building);
			if (b && !b->isEffectivelyDead())
				return TRUE;
		}
	return FALSE;
}

Bool bfmeFoundationConstruct(Object* foundation, const ThingTemplate* what)
{
	if (foundation == nullptr || what == nullptr || foundation->isEffectivelyDead() || bfmeFoundationOccupied(foundation))
		return FALSE;
	Player* owner = foundation->getControllingPlayer();
	if (owner == nullptr || owner->getDefaultTeam() == nullptr)
		return FALSE;
	const Int cost = what->calcCostToBuild(owner);
	if ((Int)owner->getMoney()->countMoney() < cost)
		return FALSE;
	owner->getMoney()->withdraw(cost);

	Object* obj = TheThingFactory->newObject(what, owner->getDefaultTeam(), MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_UNDER_CONSTRUCTION));
	if (obj == nullptr)
		return FALSE;
	obj->setProducer(foundation);
	obj->setBuilder(foundation);
	Coord3D pos = *foundation->getPosition();
	pos.z = TheTerrainLogic->getGroundHeight(pos.x, pos.y);
	obj->setPosition(&pos);
	obj->setOrientation(foundation->getOrientation());
	TheAI->pathfinder()->addObjectToPathfindMap(obj);
	owner->onStructureCreated(foundation, obj);
	obj->setConstructionPercent(0.0f);
	if (BodyModuleInterface* body = obj->getBodyModule())
		body->internalChangeHealth(-body->getHealth() + 1.0f);
	obj->clearAndSetModelConditionFlags(
		MAKE_MODELCONDITION_MASK2(MODELCONDITION_PARTIALLY_CONSTRUCTED, MODELCONDITION_ACTIVELY_BEING_CONSTRUCTED),
		MAKE_MODELCONDITION_MASK(MODELCONDITION_AWAITING_CONSTRUCTION));

	// the plot hides under its building
	foundation->setStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_UNSELECTABLE));
	if (Drawable* d = foundation->getDrawable())
		d->setDrawableHidden(TRUE);
	BfmePlotBuild pb;
	pb.m_plot = foundation->getID();
	pb.m_building = obj->getID();
	s_plotBuilds.push_back(pb);
	if (debugCastle())
		fprintf(stderr, "[castle] plot #%u: %s builds %s (#%u)\n", foundation->getID(),
			TheNameKeyGenerator->keyToName(owner->getPlayerNameKey()).str(), what->getName().str(), obj->getID());
	return TRUE;
}

//=================================================================================================
// capture flags (CAPTUREFLAG): economy plots, outposts, map flags
//=================================================================================================
struct BfmeCapture
{
	ObjectID m_flag;
	Int m_player;	///< player index taking it
	UnsignedInt m_since;
};
static std::vector<BfmeCapture> s_captures;
static const UnsignedInt CAPTURE_FRAMES = 3 * LOGICFRAMES_PER_SECOND;
static const Real CAPTURE_RADIUS = 70.0f;

static void updateCaptures()
{
	const UnsignedInt now = TheGameLogic->getFrame();
	for (Object* flag = TheGameLogic->getFirstObject(); flag; flag = flag->getNextObject())
	{
		if (!flag->isKindOf(KINDOF_CAPTUREFLAG) || flag->isEffectivelyDead())
			continue;
		CastleBehavior* cb = CastleBehavior::findOn(flag);
		if ((cb && cb->isUnpacked()) || bfmeFoundationOccupied(flag))
			continue;
		// who stands at the flag
		Player* taker = nullptr;
		Bool contested = FALSE, ownerPresent = FALSE;
		Player* owner = flag->getControllingPlayer();
		SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(flag, CAPTURE_RADIUS, FROM_CENTER_2D);
		MemoryPoolObjectHolder hold(iter);
		for (Object* o = iter->first(); o; o = iter->next())
		{
			if (o->isEffectivelyDead() || o->isKindOf(KINDOF_STRUCTURE) || o->getAI() == nullptr || o->isKindOf(KINDOF_PROJECTILE))
				continue;
			if (!(o->isKindOf(KINDOF_INFANTRY) || o->isKindOf(KINDOF_CAVALRY) || o->isKindOf(KINDOF_HERO) || o->isKindOf(KINDOF_MONSTER)))
				continue;
			Player* p = o->getControllingPlayer();
			if (!isRealPlayer(p))
				continue;
			if (p == owner)
				ownerPresent = TRUE;
			else if (taker == nullptr || taker == p)
				taker = p;
			else if (taker->getRelationship(p->getDefaultTeam()) != ALLIES)
				contested = TRUE;
		}
		size_t k = 0;
		while (k < s_captures.size() && s_captures[k].m_flag != flag->getID())
			++k;
		if (taker == nullptr || contested || ownerPresent || (isRealPlayer(owner) && owner->getRelationship(taker->getDefaultTeam()) == ALLIES))
		{
			if (k < s_captures.size())
				s_captures.erase(s_captures.begin() + k);
			continue;
		}
		if (k == s_captures.size() || s_captures[k].m_player != taker->getPlayerIndex())
		{
			BfmeCapture c;
			c.m_flag = flag->getID();
			c.m_player = taker->getPlayerIndex();
			c.m_since = now;
			if (k < s_captures.size())
				s_captures[k] = c;
			else
				s_captures.push_back(c);
			continue;
		}
		if (now - s_captures[k].m_since < CAPTURE_FRAMES || taker->getDefaultTeam() == nullptr)
			continue;
		flag->setTeam(taker->getDefaultTeam());
		s_captures.erase(s_captures.begin() + k);
		if (debugCastle())
			fprintf(stderr, "[castle] #%u %s captured by %s\n", flag->getID(), flag->getTemplate()->getName().str(),
				TheNameKeyGenerator->keyToName(taker->getPlayerNameKey()).str());
	}
}

/// per logic frame: plot buildings rise by themselves; a plot comes back when its building falls
void bfmeCastleUpdate()
{
	if (TheGameLogic->getFrame() == 0)
	{
		s_plotBuilds.clear();
		s_captures.clear();
	}
	if (TheGameLogic->getFrame() % 10 == 0)
		updateCaptures();
	for (size_t i = 0; i < s_plotBuilds.size(); )
	{
		Object* plot = TheGameLogic->findObjectByID(s_plotBuilds[i].m_plot);
		Object* b = TheGameLogic->findObjectByID(s_plotBuilds[i].m_building);
		if (b == nullptr || b->isEffectivelyDead())
		{
			if (plot && !plot->isEffectivelyDead())
			{
				plot->clearStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_UNSELECTABLE));
				if (Drawable* d = plot->getDrawable())
					d->setDrawableHidden(FALSE);
			}
			s_plotBuilds.erase(s_plotBuilds.begin() + i);
			continue;
		}
		if (b->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
		{
			const Int frames = max(1, b->getTemplate()->calcTimeToBuild(b->getControllingPlayer()));
			b->setConstructionPercent(b->getConstructionPercent() + 100.0f / (Real)frames);
			if (BodyModuleInterface* body = b->getBodyModule())
				body->internalChangeHealth(body->getMaxHealth() / (Real)frames);
			if (b->getConstructionPercent() >= 100.0f)
				completeConstruction(b, plot);
		}
		++i;
	}
}

// the control bar (Core) reaches these through hooks
extern Bool (*g_bfmeCastleUnpackHook)(Object*);
extern Bool (*g_bfmeFoundationConstructHook)(Object*, const ThingTemplate*);
static Bool castleUnpackHook(Object* o)
{
	CastleBehavior* c = CastleBehavior::findOn(o);
	return c && c->unpack(TRUE);
}
static struct BfmeCastleHooks
{
	BfmeCastleHooks()
	{
		g_bfmeCastleUnpackHook = castleUnpackHook;
		g_bfmeFoundationConstructHook = bfmeFoundationConstruct;
	}
} s_bfmeCastleHooks;

//=================================================================================================
// hero slots (REVIVE buttons on keeps and fortresses)
//=================================================================================================
#include "Common/PlayerTemplate.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Image.h"
#include "GameLogic/Module/ProductionUpdate.h"

/// the hero a REVIVE button stands for: the n-th REVIVE button of the building's command set is the
/// side's ring hero (n = 0) or its n-th BuildableHeroesMP entry
const ThingTemplate* bfmeReviveSlotHero(const Object* building, const CommandButton* button)
{
	const Player* p = building ? building->getControllingPlayer() : nullptr;
	const PlayerTemplate* pt = p ? p->getPlayerTemplate() : nullptr;
	const CommandSet* cs = TheControlBar ? TheControlBar->findCommandSet(building->getCommandSetString()) : nullptr;
	if (pt == nullptr || cs == nullptr)
		return nullptr;
	Int n = -1;
	for (Int i = 0, k = 0; i < MAX_COMMANDS_PER_SET; ++i)
	{
		const CommandButton* b = cs->getCommandButton(i);
		if (b == nullptr || b->getCommandType() != GUI_COMMAND_BFME_REVIVE)
			continue;
		if (b == button)
		{
			n = k;
			break;
		}
		++k;
	}
	if (debugCastle())
		fprintf(stderr, "[castle] revive slot %s: set %s index %d of %d heroes\n", button->getName().str(), building->getCommandSetString().str(),
			n, (int)pt->getBfmeBuildableHeroesMP().size());
	if (n <= 0)
		return nullptr;	// the ring hero slot: not modelled
	const std::vector<AsciiString>& heroes = pt->getBfmeBuildableHeroesMP();
	if (n - 1 >= (Int)heroes.size() || heroes[n - 1].compareNoCase("CreateAHero") == 0)
		return nullptr;	// Create-a-Hero has its own screen
	return TheThingFactory->findTemplate(heroes[n - 1]);
}

/// our hero of that type: alive, fallen (waiting to be revived) or none
static Object* findHero(const Player* p, const ThingTemplate* hero, Bool* fallen)
{
	*fallen = FALSE;
	for (Object* o = TheGameLogic->getFirstObject(); o; o = o->getNextObject())
	{
		if (o->getControllingPlayer() != p || o->isDestroyed() || !o->getTemplate()->isEquivalentTo(hero))
			continue;
		RespawnUpdate* r = RespawnUpdate::findOn(o);
		if (r && r->isFallen())
		{
			*fallen = TRUE;
			return o;
		}
		if (!o->isEffectivelyDead())
			return o;
	}
	return nullptr;
}

static Bool heroInProduction(const Player* p, const ThingTemplate* hero)
{
	for (Object* o = TheGameLogic->getFirstObject(); o; o = o->getNextObject())
	{
		if (o->getControllingPlayer() != p || o->isEffectivelyDead())
			continue;
		ProductionUpdateInterface* pu = o->getProductionUpdateInterface();
		if (pu && pu->getProductionCount() > 0)
			for (const ProductionEntry* e = pu->firstProduction(); e; e = pu->nextProduction(e))
				if (e->getProductionObject() && e->getProductionObject()->isEquivalentTo(hero))
					return TRUE;
	}
	return FALSE;
}

static Int reviveAvailability(const CommandButton* b, Object* building)
{
	const ThingTemplate* hero = bfmeReviveSlotHero(building, b);
	if (hero == nullptr)
		return COMMAND_HIDDEN;
	// "Fake" slots need an upgrade the side only gets in special cases
	if (BitIsSet(b->getOptions(), NEED_UPGRADE) && b->getUpgradeTemplate())
	{
		const UpgradeTemplate* ut = b->getUpgradeTemplate();
		const Player* owner = building->getControllingPlayer();
		if (!building->hasUpgrade(ut) && !(owner && owner->hasUpgradeComplete(ut)))
			return COMMAND_HIDDEN;
	}
	Player* p = building->getControllingPlayer();
	Bool fallen = FALSE;
	Object* h = findHero(p, hero, &fallen);
	if ((h && !fallen) || heroInProduction(p, hero))
		return COMMAND_RESTRICTED;
	if ((Int)p->getMoney()->countMoney() < hero->calcCostToBuild(p))
		return COMMAND_CANT_AFFORD;
	return COMMAND_AVAILABLE;
}

Bool bfmeRecruitHero(Object* building, const ThingTemplate* hero)
{
	Player* p = building ? building->getControllingPlayer() : nullptr;
	if (p == nullptr || hero == nullptr || heroInProduction(p, hero))
		return FALSE;
	Bool fallen = FALSE;
	Object* h = findHero(p, hero, &fallen);
	if (h && !fallen)
		return FALSE;
	const Int cost = hero->calcCostToBuild(p);
	if (debugCastle())
		fprintf(stderr, "[castle] recruit %s: cost %d money %u alive=%d fallen=%d production=%d\n", hero->getName().str(), cost,
			p->getMoney()->countMoney(), (int)(h != nullptr), (int)fallen, (int)(building->getProductionUpdateInterface() != nullptr));
	if ((Int)p->getMoney()->countMoney() < cost)
		return FALSE;
	if (h && fallen)
	{
		// a fallen hero comes back at the building
		Coord3D pos = *building->getPosition();
		pos.y -= building->getGeometryInfo().getBoundingSphereRadius() + 20.0f;
		RespawnUpdate* r = RespawnUpdate::findOn(h);
		if (r == nullptr || r->revive(&pos) == nullptr)
			return FALSE;
		p->getMoney()->withdraw(cost);
		return TRUE;
	}
	ProductionUpdateInterface* pu = building->getProductionUpdateInterface();
	if (debugCastle())
		fprintf(stderr, "[castle] canMakeUnit %s: %d\n", hero->getName().str(), (int)TheBuildAssistant->canMakeUnit(building, hero));
	if (pu == nullptr || !pu->queueCreateUnit(hero, pu->requestUniqueUnitID()))	// takes the cost
		return FALSE;
	if (debugCastle())
		fprintf(stderr, "[castle] %s recruits %s at %s\n", TheNameKeyGenerator->keyToName(p->getPlayerNameKey()).str(),
			hero->getName().str(), building->getTemplate()->getName().str());
	return TRUE;
}

static Bool reviveExecute(Object* building, const CommandButton* b)
{
	const ThingTemplate* hero = bfmeReviveSlotHero(building, b);
	const Bool ok = bfmeRecruitHero(building, hero);
	if (debugCastle())
		fprintf(stderr, "[castle] %s at %s: hero %s -> %d\n", b->getName().str(), building->getTemplate()->getName().str(),
			hero ? hero->getName().str() : "none", (int)ok);
	return ok;
}

/// the hero's portrait: the image of a button that builds it
static const Image* reviveImage(Object* building, const CommandButton* b)
{
	const ThingTemplate* hero = bfmeReviveSlotHero(building, b);
	if (hero == nullptr || TheControlBar == nullptr)
		return nullptr;
	static std::map<const ThingTemplate*, const Image*> cache;
	std::map<const ThingTemplate*, const Image*>::iterator it = cache.find(hero);
	if (it != cache.end())
		return it->second;
	const Image* img = nullptr;
	for (const CommandButton* c = TheControlBar->getCommandButtons(); c && img == nullptr; c = c->getNext())
		if (c->getThingTemplate() && c->getThingTemplate()->isEquivalentTo(hero) && c->getButtonImage() &&
				c->getCommandType() == GUI_COMMAND_UNIT_BUILD)
			img = c->getButtonImage();
	for (const CommandButton* c = TheControlBar->getCommandButtons(); c && img == nullptr; c = c->getNext())
		if (c->getThingTemplate() && c->getThingTemplate()->isEquivalentTo(hero) && c->getButtonImage())
			img = c->getButtonImage();
	cache[hero] = img;
	return img;
}

extern Int (*g_bfmeReviveAvailabilityHook)(const CommandButton*, Object*);
extern Bool (*g_bfmeReviveHook)(Object*, const CommandButton*);
extern const Image* (*g_bfmeReviveImageHook)(Object*, const CommandButton*);
extern const ThingTemplate* (*g_bfmeReviveHeroHook)(const Object*, const CommandButton*);
static struct BfmeReviveHooks
{
	BfmeReviveHooks()
	{
		g_bfmeReviveAvailabilityHook = reviveAvailability;
		g_bfmeReviveHook = reviveExecute;
		g_bfmeReviveImageHook = reviveImage;
		g_bfmeReviveHeroHook = bfmeReviveSlotHero;
	}
} s_bfmeReviveHooks;
