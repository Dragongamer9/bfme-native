// bfme-native: see BfmeSkirmishAI.h
#include "PreRTS.h"
#include <map>	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "Common/SpecialPower.h"
#include "Common/Science.h"
#include "Common/Upgrade.h"
#include "Common/BuildAssistant.h"
#include "Common/INI.h"
#include "Common/Money.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/RandomValue.h"
#include "GameLogic/LogicRandomValue.h"
#include "GameLogic/TerrainLogic.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/AI.h"
#include "GameLogic/BfmeSkirmishAI.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BfmeModules.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/Weapon.h"

BfmeSkirmishAIStore TheBfmeSkirmishAIStore;

//-------------------------------------------------------------------------------------------------
// INI: ArmyDefinition / AIDozerAssignment (SkirmishAIData.ini)
//-------------------------------------------------------------------------------------------------
static void parseSkipBlock(INI* ini, void*, void*, const void*)
{
	static const FieldParse none[] = { { nullptr, nullptr, nullptr, 0 } };
	char dummy[16];
	ini->initFromINI(dummy, none);
}

static void parseArmyMember(INI* ini, void* instance, void*, const void*)
{
	static const FieldParse fp[] =
	{
		{ "Unit",										INI::parseAsciiString,	nullptr, offsetof( BfmeArmyMember, m_unit ) },
		{ "PercentageOfArmyPhase1",	INI::parseReal,					nullptr, offsetof( BfmeArmyMember, m_percent[0] ) },
		{ "PercentageOfArmyPhase2",	INI::parseReal,					nullptr, offsetof( BfmeArmyMember, m_percent[1] ) },
		{ "PercentageOfArmyPhase3",	INI::parseReal,					nullptr, offsetof( BfmeArmyMember, m_percent[2] ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	BfmeArmyDefinition* army = (BfmeArmyDefinition*)instance;
	BfmeArmyMember m;
	ini->getNextTokenOrNull();	// the member's label
	ini->initFromINI(&m, fp);
	if (m.m_unit.isNotEmpty())
		army->m_members.push_back(m);
}

void BfmeSkirmishAIStore::parseArmyDefinition(INI* ini)
{
	static const FieldParse fp[] =
	{
		{ "Side",												INI::parseAsciiString,						nullptr, offsetof( BfmeArmyDefinition, m_side ) },
		{ "ArmyMemberDefinition",				parseArmyMember,									nullptr, 0 },
		{ "AIEconomyAssigment",					parseSkipBlock,										nullptr, 0 },
		{ "HeroBuildOrder",							INI::parseAsciiStringVector,			nullptr, offsetof( BfmeArmyDefinition, m_heroes ) },
		{ "PhaseDuration_Rush",					INI::parseReal,										nullptr, offsetof( BfmeArmyDefinition, m_phaseRush ) },
		{ "PhaseDuration_MidGame",			INI::parseReal,										nullptr, offsetof( BfmeArmyDefinition, m_phaseMidGame ) },
		{ "EconomyBuilderMinFarmsOwned",	INI::parseInt,									nullptr, offsetof( BfmeArmyDefinition, m_minFarms ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	BfmeArmyDefinition army;
	army.m_name = ini->getNextToken();
	ini->initFromINI(&army, fp);
	for (size_t i = 0; i < TheBfmeSkirmishAIStore.m_armies.size(); ++i)
		if (TheBfmeSkirmishAIStore.m_armies[i].m_name.compareNoCase(army.m_name) == 0)
		{
			TheBfmeSkirmishAIStore.m_armies[i] = army;
			return;
		}
	TheBfmeSkirmishAIStore.m_armies.push_back(army);
}

struct BfmeDozerAssignment
{
	AsciiString m_side;
	AsciiString m_unit;
};

void BfmeSkirmishAIStore::parseDozerAssignment(INI* ini)
{
	static const FieldParse fp[] =
	{
		{ "Side",	INI::parseAsciiString,	nullptr, offsetof( BfmeDozerAssignment, m_side ) },
		{ "Unit",	INI::parseAsciiString,	nullptr, offsetof( BfmeDozerAssignment, m_unit ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	BfmeDozerAssignment a;
	ini->getNextToken();
	ini->initFromINI(&a, fp);
	TheBfmeSkirmishAIStore.m_dozers.push_back(std::make_pair(a.m_side, a.m_unit));
}

void INI::parseBfmeArmyDefinition( INI *ini ) { BfmeSkirmishAIStore::parseArmyDefinition(ini); }
void INI::parseBfmeDozerAssignment( INI *ini ) { BfmeSkirmishAIStore::parseDozerAssignment(ini); }

const BfmeArmyDefinition* BfmeSkirmishAIStore::findArmy(const AsciiString& side) const
{
	for (size_t i = 0; i < m_armies.size(); ++i)
		if (m_armies[i].m_side.compareNoCase(side) == 0)
			return &m_armies[i];
	return nullptr;
}

AsciiString BfmeSkirmishAIStore::findDozer(const AsciiString& side) const
{
	for (size_t i = 0; i < m_dozers.size(); ++i)
		if (m_dozers[i].first.compareNoCase(side) == 0)
			return m_dozers[i].second;
	return AsciiString::TheEmptyString;
}

//-------------------------------------------------------------------------------------------------
// helpers
//-------------------------------------------------------------------------------------------------
static const UnsignedInt THINK_INTERVAL = LOGICFRAMES_PER_SECOND;
static const UnsignedInt FIRST_ATTACK = 180 * LOGICFRAMES_PER_SECOND;
static const UnsignedInt ATTACK_INTERVAL = 75 * LOGICFRAMES_PER_SECOND;

static Bool debugAI() { static const Bool d = getenv("BFME_DEBUG_SKIRMISH_AI") != nullptr; return d; }

static Bool isAlive(const Object* o)
{
	return o && !o->isEffectivelyDead() && !o->isDestroyed();
}

static Bool isHordeMember(const Object* o)
{
	if (o->getProducerID() == INVALID_ID)
		return FALSE;
	const Object* producer = TheGameLogic->findObjectByID(o->getProducerID());
	return producer && HordeContain::findOn(producer) != nullptr;
}

static Bool isFarm(const ThingTemplate* t)
{
	return t->isKindOf(KINDOF_ECONOMY_STRUCTURE) || t->isKindOf(KINDOF_FS_CASH_PRODUCER);
}

static const CommandSet* commandSetOf(const AsciiString& name)
{
	return TheControlBar ? TheControlBar->findCommandSet(name) : nullptr;
}

/// does this object count as a fighting unit of the army
static Bool isArmyUnit(const Object* o)
{
	if (o->getAI() == nullptr || o->isKindOf(KINDOF_STRUCTURE) || o->isKindOf(KINDOF_DOZER) || o->isKindOf(KINDOF_IMMOBILE))
		return FALSE;
	if (o->isKindOf(KINDOF_INERT) || o->isKindOf(KINDOF_PROJECTILE) || o->isKindOf(KINDOF_UNATTACKABLE))
		return FALSE;
	// camp decorations (civilians walking about the camp) are not soldiers
	const Player* p = o->getControllingPlayer();
	const AsciiString& side = o->getTemplate()->getDefaultOwningSide();
	if (p && side.isNotEmpty() && side.compareNoCase(p->getSide()) != 0 && side.compareNoCase("Civilian") == 0)
		return FALSE;
	return !isHordeMember(o);
}

//-------------------------------------------------------------------------------------------------
BfmeSkirmishAI::BfmeSkirmishAI(Player* player) :
	m_player(player), m_started(FALSE), m_nextThink(0), m_nextAttack(0), m_startFrame(0), m_placementTry(0), m_nextHero(0), m_nextCapture(0), m_fallbackBuilt(FALSE)
{
	m_base.zero();
}

/// the side's ArmyDefinition; sides without one (the Völker factions) get every unit their side can
/// train, in equal parts
const BfmeArmyDefinition* BfmeSkirmishAI::armyDefinition() const
{
	const BfmeArmyDefinition* army = TheBfmeSkirmishAIStore.findArmy(m_player->getSide());
	if (TheControlBar == nullptr)
		return army;
	if (!m_fallbackBuilt)
	{
		// the side's ArmyDefinition first, then every other unit the side can train at a lower weight:
		// Völker subfactions (Dol Amroth, Anórien...) train their own units, which the definition lacks
		m_fallbackBuilt = TRUE;
		m_fallback.m_side = m_player->getSide();
		if (army)
			m_fallback = *army;
		m_fallback.m_side = m_player->getSide();
		const size_t defined = m_fallback.m_members.size();
		for (const CommandButton* b = TheControlBar->getCommandButtons(); b; b = b->getNext())
		{
			const ThingTemplate* t = b->getThingTemplate();
			if (b->getCommandType() != GUI_COMMAND_UNIT_BUILD || t == nullptr || t->isKindOf(KINDOF_HERO) || t->isKindOf(KINDOF_DOZER) ||
					t->isKindOf(KINDOF_STRUCTURE) || t->getDefaultOwningSide().compareNoCase(m_player->getSide()) != 0)
				continue;
			Bool dup = FALSE;
			for (size_t i = 0; i < m_fallback.m_members.size() && !dup; ++i)
				dup = m_fallback.m_members[i].m_unit == t->getName();
			if (dup)
				continue;
			BfmeArmyMember m;
			m.m_unit = t->getName();
			m.m_percent[0] = m.m_percent[1] = m.m_percent[2] = defined ? 3.0f : 10.0f;
			m_fallback.m_members.push_back(m);
		}
		if (debugAI())
			fprintf(stderr, "[skirmishai] %ls (%s) army: %d defined + %d more trainable unit types\n", m_player->getPlayerDisplayName().str(),
				m_player->getSide().str(), (int)defined, (int)(m_fallback.m_members.size() - defined));
	}
	return &m_fallback;
}

Int BfmeSkirmishAI::phase() const
{
	const BfmeArmyDefinition* army = armyDefinition();
	const Real seconds = (Real)(TheGameLogic->getFrame() - m_startFrame) / LOGICFRAMES_PER_SECOND;
	if (army == nullptr || seconds < army->m_phaseRush)
		return 0;
	return seconds < army->m_phaseRush + army->m_phaseMidGame ? 1 : 2;
}

void BfmeSkirmishAI::update()
{
	const UnsignedInt now = TheGameLogic->getFrame();
	if (now < m_nextThink)
		return;
	m_nextThink = now + THINK_INTERVAL;
	think();
}

void BfmeSkirmishAI::think()
{
	std::vector<Object*> dozers;
	Coord3D sum;
	sum.zero();
	Int units = 0;
	for (Object* o = TheGameLogic->getFirstObject(); o; o = o->getNextObject())
	{
		if (o->getControllingPlayer() != m_player || !isAlive(o))
			continue;
		if (o->isKindOf(KINDOF_DOZER) && o->getAI())
			dozers.push_back(o);
		else if (debugAI() && o->isKindOf(KINDOF_BASE_FOUNDATION) && TheGameLogic->getFrame() / LOGICFRAMES_PER_SECOND == 90 && TheGameLogic->getFrame() % LOGICFRAMES_PER_SECOND < THINK_INTERVAL &&
				TRUE)
		{
			const CommandSet* dcs = commandSetOf(o->getCommandSetString());
			Int n = 0;
			for (Int i = 0; dcs && i < MAX_COMMANDS_PER_SET; ++i)
				if (const CommandButton* b = dcs->getCommandButton(i))
					if (b->getCommandType() == GUI_COMMAND_BFME_FOUNDATION_CONSTRUCT)
						++n;
			fprintf(stderr, "[skirmishai] plot %s #%u: unselectable %d occupied %d set %s found %d constructs %d\n", o->getTemplate()->getName().str(), o->getID(),
				(int)o->testStatus(OBJECT_STATUS_UNSELECTABLE), (int)bfmeFoundationOccupied(o), o->getCommandSetString().str(), dcs != nullptr, n);
		}
		if (o->isKindOf(KINDOF_DOZER) && o->getAI())
			;
		else if ((o->isKindOf(KINDOF_BASE_FOUNDATION) || o->isKindOf(KINDOF_CAPTUREFLAG)) && !o->testStatus(OBJECT_STATUS_UNSELECTABLE) && !bfmeFoundationOccupied(o))
		{
			const CommandSet* cs = commandSetOf(o->getCommandSetString());
			for (Int i = 0; cs && i < MAX_COMMANDS_PER_SET; ++i)
			{
				const CommandButton* b = cs->getCommandButton(i);
				if (b && (b->getCommandType() == GUI_COMMAND_BFME_FOUNDATION_CONSTRUCT || b->getCommandType() == GUI_COMMAND_BFME_CASTLE_UNPACK_EXPLICIT_OBJECT))
				{
					dozers.push_back(o);
					break;
				}
			}
		}
		if (!o->isKindOf(KINDOF_STRUCTURE) && o->getAI())
		{
			sum.x += o->getPosition()->x;
			sum.y += o->getPosition()->y;
			++units;
		}
	}
	if (!m_started)
	{
		// the base goes where the start units stand (porters hatch a few seconds in)
		if (dozers.empty())
		{
			if (debugAI() && TheGameLogic->getFrame() % (30 * LOGICFRAMES_PER_SECOND) < THINK_INTERVAL)
				fprintf(stderr, "[skirmishai] %ls (%s) waits for a builder: %d units\n", m_player->getPlayerDisplayName().str(), m_player->getSide().str(), units);
			return;
		}
		m_started = TRUE;
		m_startFrame = TheGameLogic->getFrame();
		m_nextAttack = m_startFrame + FIRST_ATTACK;
		m_base.set(sum.x / units, sum.y / units, 0.0f);
		m_base.z = TheTerrainLogic ? TheTerrainLogic->getGroundHeight(m_base.x, m_base.y) : 0.0f;
		if (debugAI())
			fprintf(stderr, "[skirmishai] %ls starts at %.0f,%.0f with %d builders\n", m_player->getPlayerDisplayName().str(), m_base.x, m_base.y, (int)dozers.size());
	}
	buildBase(dozers);
	captureSites();
	recruitHeroes();
	trainArmy();
	attack();
}

//-------------------------------------------------------------------------------------------------
Bool BfmeSkirmishAI::canTrain(const ThingTemplate* building, const ThingTemplate* unit) const
{
	const CommandSet* cs = commandSetOf(building->friend_getCommandSetString());
	for (Int i = 0; cs && i < MAX_COMMANDS_PER_SET; ++i)
	{
		const CommandButton* b = cs->getCommandButton(i);
		if (b && b->getCommandType() == GUI_COMMAND_UNIT_BUILD && b->getThingTemplate() &&
				b->getThingTemplate()->isEquivalentTo(unit))
			return TRUE;
	}
	return FALSE;
}

const ThingTemplate* BfmeSkirmishAI::producerFor(Object* dozer, const ThingTemplate* unit) const
{
	const CommandSet* cs = commandSetOf(dozer->getCommandSetString());
	for (Int i = 0; cs && i < MAX_COMMANDS_PER_SET; ++i)
	{
		const CommandButton* b = cs->getCommandButton(i);
		if (b && (b->getCommandType() == GUI_COMMAND_DOZER_CONSTRUCT || (b->getCommandType() == GUI_COMMAND_BFME_FOUNDATION_CONSTRUCT || b->getCommandType() == GUI_COMMAND_BFME_CASTLE_UNPACK_EXPLICIT_OBJECT)) &&
				b->getThingTemplate() && canTrain(b->getThingTemplate(), unit))
			return b->getThingTemplate();
	}
	return nullptr;
}

Bool BfmeSkirmishAI::placeBuilding(Object* dozer, const ThingTemplate* what)
{
	const Int cost = what->calcCostToBuild(m_player);
	if (m_player->getMoney()->countMoney() < (UnsignedInt)cost)
		return FALSE;
	// farms share the land around them (TerrainResourceBehavior): put each one as far from our
	// other farms as the land allows
	if (isFarm(what))
	{
		std::vector<Coord3D> farms;
		for (Object* o = TheGameLogic->getFirstObject(); o; o = o->getNextObject())
			if (o->getControllingPlayer() == m_player && isAlive(o) && isFarm(o->getTemplate()))
				farms.push_back(*o->getPosition());
		Coord3D best;
		Real bestScore = -1.0f;
		for (Int ring = 0; ring < 6; ++ring)
			for (Int k = 0; k < 12; ++k)
			{
				const Real radius = 220.0f + 90.0f * ring;
				const Real angle = k * (PI / 6.0f) + ring * 0.29f;
				Coord3D pos;
				pos.x = m_base.x + radius * Cos(angle);
				pos.y = m_base.y + radius * Sin(angle);
				pos.z = TheTerrainLogic->getGroundHeight(pos.x, pos.y);
				Real nearest = 1.0e9f;
				for (size_t f = 0; f < farms.size(); ++f)
				{
					const Real dx = farms[f].x - pos.x, dy = farms[f].y - pos.y;
					nearest = min(nearest, sqrtf(dx * dx + dy * dy));
				}
				// spread out, but stay near home
				const Real score = min(nearest, 400.0f) - radius * 0.25f;
				if (score <= bestScore)
					continue;
				if (TheBuildAssistant->isLocationLegalToBuild(&pos, what, angle,
						BuildAssistant::TERRAIN_RESTRICTIONS | BuildAssistant::CLEAR_PATH | BuildAssistant::NO_OBJECT_OVERLAP, dozer, m_player) != LBC_OK)
					continue;
				best = pos;
				bestScore = score;
			}
		if (bestScore > -1.0f)
		{
			// the porter's construct() takes the cost
			if (TheBuildAssistant->buildObjectNow(dozer, what, &best, 0.0f, m_player))
			{
				if (debugAI())
					fprintf(stderr, "[skirmishai] %ls builds %s at %.0f,%.0f\n", m_player->getPlayerDisplayName().str(), what->getName().str(), best.x, best.y);
				return TRUE;
			}
		}
	}
	// a spiral of sites around the base, the building facing away from its centre
	for (Int tries = 0; tries < 48; ++tries, ++m_placementTry)
	{
		const Int ring = (m_placementTry / 8) % 8;
		const Real radius = 140.0f + 70.0f * ring;
		const Real angle = (m_placementTry % 8) * (PI / 4.0f) + ring * 0.37f;
		Coord3D pos;
		pos.x = m_base.x + radius * Cos(angle);
		pos.y = m_base.y + radius * Sin(angle);
		pos.z = TheTerrainLogic->getGroundHeight(pos.x, pos.y);
		const Real facing = angle;
		const LegalBuildCode lbc = TheBuildAssistant->isLocationLegalToBuild(&pos, what, facing,
			BuildAssistant::TERRAIN_RESTRICTIONS | BuildAssistant::CLEAR_PATH | BuildAssistant::NO_OBJECT_OVERLAP, dozer, m_player);
		if (lbc != LBC_OK)
			continue;
		Object* site = TheBuildAssistant->buildObjectNow(dozer, what, &pos, facing, m_player);	// takes the cost
		if (site == nullptr)
			continue;
		if (debugAI())
			fprintf(stderr, "[skirmishai] %ls builds %s at %.0f,%.0f\n", m_player->getPlayerDisplayName().str(), what->getName().str(), pos.x, pos.y);
		++m_placementTry;
		return TRUE;
	}
	return FALSE;
}

void BfmeSkirmishAI::buildBase(std::vector<Object*>& dozers)
{
	const BfmeArmyDefinition* army = armyDefinition();
	// camps: free build plots build instead of porters (one building at a time)
	Bool constructing = FALSE;
	for (Object* o = TheGameLogic->getFirstObject(); o && !constructing; o = o->getNextObject())
		if (o->getControllingPlayer() == m_player && isAlive(o) && o->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
			constructing = TRUE;
	// try every builder until one has something useful to build (a defence plot only raises towers)
	Int tried = 0;
	for (size_t di = 0; di < dozers.size() && tried < 4; ++di)
	{
	Object* dozer = dozers[di];
	const Bool plot = dozer->isKindOf(KINDOF_BASE_FOUNDATION) || dozer->isKindOf(KINDOF_CAPTUREFLAG);
	if (!plot && !(dozer->getAI() && dozer->getAI()->isIdle()))
		continue;
	if (plot && constructing)
		continue;

	// what we own (sites under construction included)
	Int farms = 0;
	std::vector<const ThingTemplate*> owned;
	for (Object* o = TheGameLogic->getFirstObject(); o; o = o->getNextObject())
	{
		if (o->getControllingPlayer() != m_player || !isAlive(o) || !o->isKindOf(KINDOF_STRUCTURE))
			continue;
		if (isFarm(o->getTemplate()))
			++farms;
		owned.push_back(o->getTemplate());
	}
	const ThingTemplate* farm = nullptr;
	const CommandSet* cs = commandSetOf(dozer->getCommandSetString());
	for (Int i = 0; cs && i < MAX_COMMANDS_PER_SET && farm == nullptr; ++i)
	{
		const CommandButton* b = cs->getCommandButton(i);
		if (b && (b->getCommandType() == GUI_COMMAND_DOZER_CONSTRUCT || (b->getCommandType() == GUI_COMMAND_BFME_FOUNDATION_CONSTRUCT || b->getCommandType() == GUI_COMMAND_BFME_CASTLE_UNPACK_EXPLICIT_OBJECT)) &&
				b->getThingTemplate() && isFarm(b->getThingTemplate()))
			farm = b->getThingTemplate();
	}

	// the army's units by importance in this phase, and whether something we own trains them
	std::vector<const BfmeArmyMember*> members;
	if (army)
		for (size_t i = 0; i < army->m_members.size(); ++i)
			if (army->m_members[i].m_percent[phase()] > 0.0f)
				members.push_back(&army->m_members[i]);
	const Int ph = phase();
	std::stable_sort(members.begin(), members.end(), [ph](const BfmeArmyMember* a, const BfmeArmyMember* b) { return a->m_percent[ph] > b->m_percent[ph]; });

	std::vector<const ThingTemplate*> wanted;
	if (farm && farms < 2)
		wanted.push_back(farm);
	for (size_t i = 0; i < members.size(); ++i)
	{
		const ThingTemplate* unit = TheThingFactory->findTemplate(members[i]->m_unit);
		if (unit == nullptr)
			continue;
		Bool have = FALSE;
		for (size_t k = 0; k < owned.size() && !have; ++k)
			have = canTrain(owned[k], unit);
		if (have)
			continue;
		const ThingTemplate* producer = producerFor(dozer, unit);
		if (producer && std::find(wanted.begin(), wanted.end(), producer) == wanted.end())
			wanted.push_back(producer);
		if (wanted.size() >= 3)
			break;
	}
	if (farm && farms < (army ? army->m_minFarms : 4))
		wanted.push_back(farm);
	if (wanted.empty())
		continue;
	++tried;	// only builders with something to build count against the per-think limit
	if (debugAI() && TheGameLogic->getFrame() % (30 * LOGICFRAMES_PER_SECOND) < THINK_INTERVAL)
		fprintf(stderr, "[skirmishai] %ls builder %s (%s): farm %s, wants %d (first %s), money %d\n", m_player->getPlayerDisplayName().str(),
			dozer->getTemplate()->getName().str(), dozer->getCommandSetString().str(), farm ? farm->getName().str() : "-", (int)wanted.size(),
			wanted.empty() ? "-" : wanted[0]->getName().str(), (int)m_player->getMoney()->countMoney());

	for (size_t i = 0; i < wanted.size(); ++i)
	{
		if (plot)
		{
			if (wanted[i]->calcCostToBuild(m_player) <= (Int)m_player->getMoney()->countMoney() && bfmeFoundationConstruct(dozer, wanted[i]))
			{
				if (debugAI())
					fprintf(stderr, "[skirmishai] %ls builds %s on a plot\n", m_player->getPlayerDisplayName().str(), wanted[i]->getName().str());
				return;
			}
			continue;
		}
		if (placeBuilding(dozer, wanted[i]))
			return;
	}
	}
}

//-------------------------------------------------------------------------------------------------
/// economy plots and outposts (capture flags with a castle site): send a unit to the nearest free one
void BfmeSkirmishAI::captureSites()
{
	const UnsignedInt now = TheGameLogic->getFrame();
	if (now < m_nextCapture)
		return;
	m_nextCapture = now + 30 * LOGICFRAMES_PER_SECOND;
	const Object* site = nullptr;
	Real best = 1600.0f * 1600.0f;
	for (Object* o = TheGameLogic->getFirstObject(); o; o = o->getNextObject())
	{
		if (!o->isKindOf(KINDOF_CAPTUREFLAG) || !isAlive(o) || o->getControllingPlayer() == m_player)
			continue;
		CastleBehavior* cb = CastleBehavior::findOn(o);
		if (cb == nullptr || cb->isUnpacked())
			continue;
		const Player* owner = o->getControllingPlayer();
		if (owner && owner->isPlayableSide() && owner->getPlayerTemplate())
			continue;	// someone else's: that is war, not expansion
		const Real dx = o->getPosition()->x - m_base.x, dy = o->getPosition()->y - m_base.y;
		if (dx * dx + dy * dy < best)
		{
			best = dx * dx + dy * dy;
			site = o;
		}
	}
	if (site == nullptr)
		return;
	for (Object* o = TheGameLogic->getFirstObject(); o; o = o->getNextObject())
		if (o->getControllingPlayer() == m_player && isAlive(o) && isArmyUnit(o) && o->getAI() && o->getAI()->isIdle())
		{
			o->getAI()->aiMoveToPosition(site->getPosition(), CMD_FROM_AI);
			if (debugAI())
				fprintf(stderr, "[skirmishai] %ls sends %s to take %s\n", m_player->getPlayerDisplayName().str(),
					o->getTemplate()->getName().str(), site->getTemplate()->getName().str());
			return;
		}
}

//-------------------------------------------------------------------------------------------------
/// heroes from the hero slots of our keep and barracks: one more per game phase, when there is
/// money left for the army
//-------------------------------------------------------------------------------------------------
/** Edain raises the army limit with "economy units" (CPObject, +50 each) bought at the keep. Buy one
	when the army is close to the limit and money is left over. */
static void raiseCommandPoints(Player* player)
{
	const Int limit = player->bfmeCommandPointLimit();
	if (limit <= 0 || player->bfmeCommandPointsUsed() + 60 < limit)
		return;
	for (Object* o = TheGameLogic->getFirstObject(); o; o = o->getNextObject())
	{
		if (o->getControllingPlayer() != player || !isAlive(o) || !o->isKindOf(KINDOF_STRUCTURE) ||
				o->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
			continue;
		ProductionUpdateInterface* pu = o->getProductionUpdateInterface();
		const CommandSet* cs = pu ? commandSetOf(o->getCommandSetString()) : nullptr;
		for (Int i = 0; cs && i < MAX_COMMANDS_PER_SET; ++i)
		{
			const CommandButton* b = cs->getCommandButton(i);
			const ThingTemplate* t = b && b->getCommandType() == GUI_COMMAND_UNIT_BUILD ? b->getThingTemplate() : nullptr;
			if (t == nullptr || t->isKindOf(KINDOF_STRUCTURE) || t->bfmeGetCommandPointBonus() <= 0)
				continue;
			if (t->calcCostToBuild(player) + 600 > (Int)player->getMoney()->countMoney() ||
					TheBuildAssistant->canMakeUnit(o, t) != CANMAKE_OK)
				continue;
			if (pu->queueCreateUnit(t, pu->requestUniqueUnitID()))
			{
				if (debugAI())
					fprintf(stderr, "[skirmishai] %ls buys %s at %s (army %d/%d)\n", player->getPlayerDisplayName().str(),
						t->getName().str(), o->getTemplate()->getName().str(), player->bfmeCommandPointsUsed(), limit);
				return;
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
/** Spell book: buy a power with the points earned (the cheapest first) and cast what is ready:
	heals on the most hurt unit, untargeted powers at once, the rest on the enemy unit closest to
	our army (only while there is fighting). */
static void useSpellBook(Player* player)
{
	if (TheScienceStore && player->getSciencePurchasePoints() > 0)
	{
		ScienceVec purchasable, potential;
		TheScienceStore->getPurchasableSciences(player, purchasable, potential);
		ScienceType best = SCIENCE_INVALID;
		Int bestCost = 1 << 30;
		for (size_t i = 0; i < purchasable.size(); ++i)
		{
			const Int cost = TheScienceStore->getSciencePurchaseCost(purchasable[i]);
			if (cost > 0 && (cost < bestCost || (cost == bestCost && GameLogicRandomValue(0, 1))) && player->isCapableOfPurchasingScience(purchasable[i]))
			{
				best = purchasable[i];
				bestCost = cost;
			}
		}
		if (best != SCIENCE_INVALID && player->attemptToPurchaseScience(best) && debugAI())
			fprintf(stderr, "[skirmishai] %ls learns %s\n", player->getPlayerDisplayName().str(), TheScienceStore->getInternalNameForScience(best).str());
	}
	Object* book = player->bfmeGetSpellBook();
	const CommandSet* cs = book ? commandSetOf(book->getCommandSetString()) : nullptr;
	if (cs == nullptr)
		return;
	// the fight: the enemy unit nearest to one of ours, and our most hurt unit
	Object* foe = nullptr;
	Object* hurt = nullptr;
	Real foeDist = 500.0f * 500.0f, hurtRatio = 0.6f;
	for (Object* o = TheGameLogic->getFirstObject(); o; o = o->getNextObject())
	{
		if (o->getControllingPlayer() != player || !isArmyUnit(o))
			continue;
		if (BodyModuleInterface* body = o->getBodyModule())
			if (body->getMaxHealth() > 0.0f && body->getHealth() / body->getMaxHealth() < hurtRatio)
			{
				hurt = o;
				hurtRatio = body->getHealth() / body->getMaxHealth();
			}
		AIUpdateInterface* ai = o->getAI();
		Object* victim = ai ? ai->getCurrentVictim() : nullptr;
		if (victim && !victim->isEffectivelyDead() && !victim->isKindOf(KINDOF_STRUCTURE))
		{
			const Real dx = victim->getPosition()->x - o->getPosition()->x, dy = victim->getPosition()->y - o->getPosition()->y;
			if (dx * dx + dy * dy < foeDist)
			{
				foe = victim;
				foeDist = dx * dx + dy * dy;
			}
		}
	}
	for (Int i = 0; i < MAX_COMMANDS_PER_SET; ++i)
	{
		const CommandButton* b = cs->getCommandButton(i);
		if (b == nullptr || b->getSpecialPowerTemplate() == nullptr ||
				TheControlBar->getCommandAvailability(b, book, nullptr) != COMMAND_AVAILABLE)
			continue;
		SpecialPowerModuleInterface* sp = book->getSpecialPowerModule(b->getSpecialPowerTemplate());
		if (sp == nullptr || !sp->isReady())
			continue;
		// powers without a recharge (map view...) would be cast every turn: give each 90 s of rest
		static std::map<std::pair<const Player*, const SpecialPowerTemplate*>, UnsignedInt> s_lastCast;
		UnsignedInt& last = s_lastCast[std::make_pair((const Player*)player, b->getSpecialPowerTemplate())];
		if (last && TheGameLogic->getFrame() < last + 90 * LOGICFRAMES_PER_SECOND && TheGameLogic->getFrame() >= last)
			continue;
		const UnsignedInt opt = b->getOptions();
		const AsciiString& name = b->getSpecialPowerTemplate()->getName();
		const Bool heal = strstr(name.str(), "Heal") != nullptr || strstr(name.str(), "heal") != nullptr;
		Object* target = heal ? hurt : foe;
		if (opt & (NEED_TARGET_ENEMY_OBJECT | NEED_TARGET_ALLY_OBJECT | NEED_TARGET_NEUTRAL_OBJECT))
		{
			if (opt & NEED_TARGET_ALLY_OBJECT)
				target = hurt;
			if (target == nullptr)
				continue;
			sp->doSpecialPowerAtObject(target, 0);
		}
		else if (opt & NEED_TARGET_POS)
		{
			if (target == nullptr)
				continue;
			sp->doSpecialPowerAtLocation(target->getPosition(), INVALID_ANGLE, 0);
		}
		else
			sp->doSpecialPower(0);
		last = TheGameLogic->getFrame();
		if (debugAI())
			fprintf(stderr, "[skirmishai] %ls casts %s%s\n", player->getPlayerDisplayName().str(), name.str(),
				target && (opt & (NEED_TARGET_POS | NEED_TARGET_ENEMY_OBJECT | NEED_TARGET_ALLY_OBJECT)) ? " on the battle" : "");
		return;	// one spell per turn
	}
}

//-------------------------------------------------------------------------------------------------
/** Research: once in a while buy one upgrade a building offers (forge upgrades, keep upgrades, the
	Völker subfaction choice at the keep first), keeping some money for the army. */
static void buyUpgrades(Player* player)
{
	std::vector<std::pair<Object*, const UpgradeTemplate*> > options;
	Bool keepFirst = FALSE;
	const Int money = (Int)player->getMoney()->countMoney();
	for (Object* o = TheGameLogic->getFirstObject(); o; o = o->getNextObject())
	{
		if (o->getControllingPlayer() != player || !isAlive(o) || !o->isKindOf(KINDOF_STRUCTURE) ||
				o->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
			continue;
		ProductionUpdateInterface* pu = o->getProductionUpdateInterface();
		const CommandSet* cs = pu && pu->getProductionCount() == 0 ? commandSetOf(o->getCommandSetString()) : nullptr;
		for (Int i = 0; cs && i < MAX_COMMANDS_PER_SET; ++i)
		{
			const CommandButton* b = cs->getCommandButton(i);
			if (b == nullptr || (b->getCommandType() != GUI_COMMAND_PLAYER_UPGRADE && b->getCommandType() != GUI_COMMAND_OBJECT_UPGRADE))
				continue;
			const UpgradeTemplate* ut = b->getUpgradeTemplate();
			if (ut == nullptr || TheControlBar->getCommandAvailability(b, o, nullptr) != COMMAND_AVAILABLE)
				continue;
			if (ut->getUpgradeType() == UPGRADE_TYPE_PLAYER ? player->hasUpgradeComplete(ut) || player->hasUpgradeInProduction(ut)
					: o->hasUpgrade(ut) || !o->affectedByUpgrade(ut))
				continue;
			if (!TheUpgradeCenter->canAffordUpgrade(player, ut, FALSE) || ut->calcCostToBuild(player) + 400 > money ||
					!o->canProduceUpgrade(ut) || pu->canQueueUpgrade(ut) == CANMAKE_QUEUE_FULL)
				continue;
			const Bool keep = o->isKindOf(KINDOF_COMMANDCENTER);
			if (keep && !keepFirst)
			{
				options.clear();
				keepFirst = TRUE;
			}
			if (keep || !keepFirst)
				options.push_back(std::make_pair(o, ut));
		}
	}
	if (options.empty())
		return;
	const size_t pick = (size_t)GameLogicRandomValue(0, (Int)options.size() - 1);
	ProductionUpdateInterface* pu = options[pick].first->getProductionUpdateInterface();
	if (pu && pu->queueUpgrade(options[pick].second) && debugAI())
		fprintf(stderr, "[skirmishai] %ls researches %s at %s\n", player->getPlayerDisplayName().str(),
			options[pick].second->getUpgradeName().str(), options[pick].first->getTemplate()->getName().str());
}

void BfmeSkirmishAI::recruitHeroes()
{
	const UnsignedInt now = TheGameLogic->getFrame();
	if (now < m_nextHero)
		return;
	m_nextHero = now + 20 * LOGICFRAMES_PER_SECOND;
	raiseCommandPoints(m_player);
	buyUpgrades(m_player);
	useSpellBook(m_player);
	Int heroes = 0;
	for (Object* o = TheGameLogic->getFirstObject(); o; o = o->getNextObject())
		if (o->getControllingPlayer() == m_player && isAlive(o) && o->isKindOf(KINDOF_HERO) && !o->isKindOf(KINDOF_STRUCTURE))
			++heroes;
	if (heroes >= phase() + 1)
		return;
	std::vector<std::pair<Object*, const ThingTemplate*> > options;
	for (Object* o = TheGameLogic->getFirstObject(); o; o = o->getNextObject())
	{
		if (o->getControllingPlayer() != m_player || !isAlive(o) || !o->isKindOf(KINDOF_STRUCTURE) ||
				o->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
			continue;
		const CommandSet* cs = commandSetOf(o->getCommandSetString());
		for (Int i = 0; cs && i < MAX_COMMANDS_PER_SET; ++i)
		{
			const CommandButton* b = cs->getCommandButton(i);
			if (b == nullptr || b->getCommandType() != GUI_COMMAND_BFME_REVIVE)
				continue;
			if (TheControlBar->getCommandAvailability(b, o, nullptr) != COMMAND_AVAILABLE)
				continue;
			const ThingTemplate* hero = bfmeReviveSlotHero(o, b);
			if (hero && hero->calcCostToBuild(m_player) + 300 <= (Int)m_player->getMoney()->countMoney())
				options.push_back(std::make_pair(o, hero));
		}
	}
	if (options.empty())
		return;
	const size_t pick = (size_t)GameLogicRandomValue(0, (Int)options.size() - 1);
	if (bfmeRecruitHero(options[pick].first, options[pick].second) && debugAI())
		fprintf(stderr, "[skirmishai] %ls recruits hero %s\n", m_player->getPlayerDisplayName().str(), options[pick].second->getName().str());
}

//-------------------------------------------------------------------------------------------------
void BfmeSkirmishAI::trainArmy()
{
	const BfmeArmyDefinition* army = armyDefinition();
	if (army == nullptr)
		return;
	const Int ph = phase();
	for (Object* o = TheGameLogic->getFirstObject(); o; o = o->getNextObject())
	{
		if (o->getControllingPlayer() != m_player || !isAlive(o) || !o->isKindOf(KINDOF_STRUCTURE))
			continue;
		if (o->getStatusBits().test(OBJECT_STATUS_UNDER_CONSTRUCTION) || o->getStatusBits().test(OBJECT_STATUS_SOLD))
			continue;
		ProductionUpdateInterface* pu = o->getProductionUpdateInterface();
		if (pu == nullptr || pu->getProductionCount() > 0)
			continue;
		// weighted pick among the units this building trains
		std::vector<const ThingTemplate*> choice;
		std::vector<Real> weight;
		Real total = 0.0f;
		for (size_t i = 0; i < army->m_members.size(); ++i)
		{
			const Real pct = army->m_members[i].m_percent[ph];
			const ThingTemplate* unit = pct > 0.0f ? TheThingFactory->findTemplate(army->m_members[i].m_unit) : nullptr;
			if (unit == nullptr || !canTrain(o->getTemplate(), unit))
				continue;
			const CanMakeType cmt = TheBuildAssistant->canMakeUnit(o, unit);
			if (cmt != CANMAKE_OK)
			{
				if (debugAI() && cmt != CANMAKE_NO_MONEY)
				{
					static Int s_reported = 0;
					if (s_reported++ < 20)
						fprintf(stderr, "[skirmishai] %ls can't train %s at %s: %d\n", m_player->getPlayerDisplayName().str(),
							unit->getName().str(), o->getTemplate()->getName().str(), (int)cmt);
				}
				continue;
			}
			choice.push_back(unit);
			weight.push_back(pct);
			total += pct;
		}
		if (choice.empty())
			continue;
		Real roll = GameLogicRandomValueReal(0.0f, total);
		size_t pick = 0;
		for (; pick + 1 < choice.size() && roll > weight[pick]; ++pick)
			roll -= weight[pick];
		const ThingTemplate* unit = choice[pick];
		// queueCreateUnit takes the cost
		if (pu->queueCreateUnit(unit, pu->requestUniqueUnitID()) && debugAI())
			fprintf(stderr, "[skirmishai] %ls trains %s at %s\n", m_player->getPlayerDisplayName().str(), unit->getName().str(), o->getTemplate()->getName().str());
	}
}

//-------------------------------------------------------------------------------------------------
void BfmeSkirmishAI::attack()
{
	const UnsignedInt now = TheGameLogic->getFrame();
	if (now < m_nextAttack)
		return;
	std::vector<Object*> army;
	for (Object* o = TheGameLogic->getFirstObject(); o; o = o->getNextObject())
		if (o->getControllingPlayer() == m_player && isAlive(o) && isArmyUnit(o))
			army.push_back(o);
	if (army.size() < 3)
	{
		m_nextAttack = now + 20 * LOGICFRAMES_PER_SECOND;
		return;
	}
	// the nearest enemy building to our base, else the nearest enemy unit
	const Object* target = nullptr;
	Real best = 0.0f;
	for (Int pass = 0; pass < 2 && target == nullptr; ++pass)
		for (Object* t = TheGameLogic->getFirstObject(); t; t = t->getNextObject())
		{
			if (!isAlive(t) || t->getControllingPlayer() == nullptr || t->getTeam() == nullptr)
				continue;
			if (m_player->getRelationship(t->getTeam()) != ENEMIES || t->isKindOf(KINDOF_UNATTACKABLE) || t->isKindOf(KINDOF_INERT))
				continue;
			if (pass == 0 ? !t->isKindOf(KINDOF_STRUCTURE) : !isArmyUnit(t))
				continue;
			const Real dx = t->getPosition()->x - m_base.x, dy = t->getPosition()->y - m_base.y;
			const Real d = dx * dx + dy * dy;
			if (target == nullptr || d < best)
			{
				target = t;
				best = d;
			}
		}
	m_nextAttack = now + ATTACK_INTERVAL;
	if (target == nullptr)
		return;
	for (size_t i = 0; i < army.size(); ++i)
		army[i]->getAI()->aiAttackMoveToPosition(target->getPosition(), NO_MAX_SHOTS_LIMIT, CMD_FROM_AI);
	if (debugAI())
		fprintf(stderr, "[skirmishai] %ls attacks %s at %.0f,%.0f with %d units\n", m_player->getPlayerDisplayName().str(),
			target->getTemplate()->getName().str(), target->getPosition()->x, target->getPosition()->y, (int)army.size());
}
