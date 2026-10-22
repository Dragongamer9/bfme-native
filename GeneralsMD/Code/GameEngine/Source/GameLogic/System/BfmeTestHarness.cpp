// bfme-native: scripted headless logic tests.
//
// BFME_TEST=<script> runs a script against the running game (normally started with
// "-headless -file <test map>"). One command per line, '#' starts a comment:
//
//   <frame> spawn  TEMPLATE PLAYER X Y [ANGLE] [COUNT]
//   <frame> attack PLAYER_A PLAYER_B      every unit of A attacks the closest enemy object of B
//   <frame> move   PLAYER X Y              every unit of PLAYER moves to X Y
//   <frame> upgrade PLAYER UPGRADE         give a player upgrade
//   <frame> dump                           print every object (template, owner, health, position)
//   <frame> summary                        print living units / total health per player
//   <frame> quit                           stop the engine
//
// Output lines start with "[BFMETEST]".
#include "PreRTS.h"

#include "Common/GameEngine.h"
#include "Common/Money.h"
#include "GameClient/View.h"
#include "GameClient/Display.h"
#include "GameLogic/Stances.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "Common/BuildAssistant.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/ExperienceLevel.h"
#include "GameLogic/Module/BfmeModules.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/PlayerTemplate.h"
#include "Common/SpecialPower.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Weapon.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{
struct TestCmd
{
	UnsignedInt frame;
	std::vector<std::string> args;
};

std::vector<TestCmd> s_cmds;
size_t s_next = 0;
Bool s_loaded = FALSE;

void loadScript()
{
	s_loaded = TRUE;
	const char* path = getenv("BFME_TEST");
	if (path == nullptr || *path == 0)
		return;
	FILE* f = fopen(path, "r");
	if (f == nullptr)
	{
		printf("[BFMETEST] cannot open %s\n", path);
		return;
	}
	char line[1024];
	while (fgets(line, sizeof(line), f))
	{
		std::string s(line);
		size_t hash = s.find('#');
		if (hash != std::string::npos)
			s.resize(hash);
		std::istringstream is(s);
		TestCmd c;
		if (!(is >> c.frame))
			continue;
		std::string a;
		while (is >> a)
			c.args.push_back(a);
		if (!c.args.empty())
			s_cmds.push_back(c);
	}
	fclose(f);
	std::stable_sort(s_cmds.begin(), s_cmds.end(), [](const TestCmd& x, const TestCmd& y) { return x.frame < y.frame; });
	printf("[BFMETEST] loaded %d commands from %s\n", (int)s_cmds.size(), path);
	fflush(stdout);
}

Player* findPlayer(const std::string& name)
{
	return ThePlayerList ? ThePlayerList->findPlayerWithNameKey(NAMEKEY(name.c_str())) : nullptr;
}

Bool isUnitOf(const Object* o, const Player* p)
{
	return o->getControllingPlayer() == p && !o->isEffectivelyDead() && o->getAI() != nullptr;
}

const char* ownerName(const Object* o)
{
	const Player* p = o->getControllingPlayer();
	static char buf[64];
	if (p == nullptr)
		return "-";
	AsciiString n = TheNameKeyGenerator->keyToName(p->getPlayerNameKey());
	snprintf(buf, sizeof(buf), "%s", n.isEmpty() ? "neutral" : n.str());
	return buf;
}

void run(const TestCmd& c)
{
	const std::string& op = c.args[0];
	if (op == "spawn" && c.args.size() >= 5)
	{
		const ThingTemplate* t = TheThingFactory->findTemplate(c.args[1].c_str());
		Player* p = findPlayer(c.args[2]);
		if (t == nullptr || p == nullptr)
		{
			printf("[BFMETEST] spawn: unknown %s %s\n", t ? "" : c.args[1].c_str(), p ? "" : c.args[2].c_str());
			return;
		}
		Real angle = c.args.size() >= 6 ? (Real)atof(c.args[5].c_str()) * PI / 180.0f : 0.0f;
		Int count = c.args.size() >= 7 ? atoi(c.args[6].c_str()) : 1;
		for (Int i = 0; i < count; ++i)
		{
			Coord3D pos;
			pos.x = (Real)atof(c.args[3].c_str()) + 30.0f * i;
			pos.y = (Real)atof(c.args[4].c_str());
			pos.z = TheTerrainLogic->getGroundHeight(pos.x, pos.y);
			Object* o = TheThingFactory->newObject(t, p->getDefaultTeam());
			if (o)
			{
				o->setPosition(&pos);
				o->setOrientation(angle);
				printf("[BFMETEST] spawned %s #%u for %s at %.0f %.0f\n", t->getName().str(), o->getID(), c.args[2].c_str(), pos.x, pos.y);
			}
		}
	}
	else if (op == "build" && c.args.size() >= 5)
	{
		// build TEMPLATE PLAYER X Y : place a structure as a fresh construction site
		const ThingTemplate* t = TheThingFactory->findTemplate(c.args[1].c_str());
		Player* p = findPlayer(c.args[2]);
		if (t == nullptr || p == nullptr)
			return;
		Object* o = TheThingFactory->newObject(t, p->getDefaultTeam(), MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_UNDER_CONSTRUCTION));
		Coord3D pos;
		pos.x = (Real)atof(c.args[3].c_str());
		pos.y = (Real)atof(c.args[4].c_str());
		pos.z = TheTerrainLogic->getGroundHeight(pos.x, pos.y);
		o->setPosition(&pos);
		TheAI->pathfinder()->addObjectToPathfindMap(o);
		p->onStructureCreated(o, o);
		o->setConstructionPercent(0.0f);
		BodyModuleInterface* body = o->getBodyModule();
		if (body)
			body->internalChangeHealth(-body->getHealth() + 1.0f);
		printf("[BFMETEST] build %s #%u for %s (build time %d frames)\n", t->getName().str(), o->getID(), c.args[2].c_str(), t->calcTimeToBuild(p));
	}
	else if (op == "construction")
	{
		for (Object* o = TheGameLogic->getFirstObject(); o; o = o->getNextObject())
			if (o->isKindOf(KINDOF_STRUCTURE))
				printf("[BFMETEST] construction #%u %s %.0f%% underConstruction=%d hp=%.0f\n", o->getID(), o->getTemplate()->getName().str(),
					o->getConstructionPercent(), (int)o->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION), o->getBodyModule() ? o->getBodyModule()->getHealth() : 0.0f);
	}
	else if (op == "attack" && c.args.size() >= 3)
	{
		Player* a = findPlayer(c.args[1]);
		Player* b = findPlayer(c.args[2]);
		if (!a || !b)
			return;
		Int n = 0;
		for (Object* o = TheGameLogic->getFirstObject(); o; o = o->getNextObject())
		{
			if (!isUnitOf(o, a) || o->getProducerID() != INVALID_ID)
				continue;	// horde members follow their horde
			Object* best = nullptr;
			Real bestD = 0.0f;
			for (Object* t = TheGameLogic->getFirstObject(); t; t = t->getNextObject())
			{
				if (t->getControllingPlayer() != b || t->isEffectivelyDead() || t->getProducerID() != INVALID_ID)
					continue;
				Real dx = t->getPosition()->x - o->getPosition()->x, dy = t->getPosition()->y - o->getPosition()->y;
				Real d = dx * dx + dy * dy;
				if (best == nullptr || d < bestD)
				{
					best = t;
					bestD = d;
				}
			}
			if (best)
			{
				o->getAI()->aiAttackObject(best, NO_MAX_SHOTS_LIMIT, CMD_FROM_PLAYER);
				++n;
			}
		}
		printf("[BFMETEST] attack: %d units of %s ordered\n", n, c.args[1].c_str());
	}
	else if (op == "move" && c.args.size() >= 4)
	{
		Player* a = findPlayer(c.args[1]);
		if (!a)
			return;
		Coord3D pos;
		pos.x = (Real)atof(c.args[2].c_str());
		pos.y = (Real)atof(c.args[3].c_str());
		pos.z = TheTerrainLogic->getGroundHeight(pos.x, pos.y);
		for (Object* o = TheGameLogic->getFirstObject(); o; o = o->getNextObject())
			if (isUnitOf(o, a) && o->getProducerID() == INVALID_ID)
				o->getAI()->aiMoveToPosition(&pos, CMD_FROM_PLAYER);
	}
	else if (op == "upgrade" && c.args.size() >= 3)
	{
		Player* a = findPlayer(c.args[1]);
		const UpgradeTemplate* ut = TheUpgradeCenter->findUpgrade(c.args[2].c_str());
		if (a && ut && ut->getUpgradeType() == UPGRADE_TYPE_PLAYER)
			a->addUpgrade(ut, UPGRADE_STATUS_COMPLETE);
		else if (a && ut)
		{
			// object upgrade: give it to the player's objects (of template args[3] if given)
			std::vector<Object*> v;
			for (Object* o = TheGameLogic->getFirstObject(); o; o = o->getNextObject())
				if (o->getControllingPlayer() == a && !o->isEffectivelyDead() && !o->hasUpgrade(ut) &&
						(c.args.size() < 4 || o->getTemplate()->getName().compareNoCase(c.args[3].c_str()) == 0))
					v.push_back(o);
			for (size_t i = 0; i < v.size(); ++i)
				v[i]->giveUpgrade(ut);
			printf("[BFMETEST] upgrade %s given to %d objects\n", c.args[2].c_str(), (int)v.size());
		}
	}
	else if (op == "dump")
	{
		for (Object* o = TheGameLogic->getFirstObject(); o; o = o->getNextObject())
		{
			BodyModuleInterface* body = o->getBodyModule();
			printf("[BFMETEST] obj #%u %s owner=%s pos=%.0f,%.0f hp=%.0f/%.0f%s\n", o->getID(), o->getTemplate()->getName().str(),
				ownerName(o), o->getPosition()->x, o->getPosition()->y, body ? body->getHealth() : 0.0f, body ? body->getMaxHealth() : 0.0f,
				o->isEffectivelyDead() ? " DEAD" : "");
		}
	}
	else if (op == "summary")
	{
		std::map<std::string, std::pair<int, float> > per;
		for (Object* o = TheGameLogic->getFirstObject(); o; o = o->getNextObject())
		{
			if (o->isEffectivelyDead() || o->getAI() == nullptr)
				continue;
			BodyModuleInterface* body = o->getBodyModule();
			std::pair<int, float>& e = per[ownerName(o)];
			e.first += 1;
			e.second += body ? body->getHealth() : 0.0f;
		}
		for (std::map<std::string, std::pair<int, float> >::iterator it = per.begin(); it != per.end(); ++it)
			printf("[BFMETEST] summary frame=%u %s units=%d health=%.0f\n", TheGameLogic->getFrame(), it->first.c_str(), it->second.first, it->second.second);
	}
	else if (op == "state")
	{
		for (Object* o = TheGameLogic->getFirstObject(); o; o = o->getNextObject())
		{
			AIUpdateInterface* ai = o->getAI();
			if (ai == nullptr)
				continue;
			const Locomotor* loco = ai->getCurLocomotor();
			Object* v = ai->getCurrentVictim();
			const Weapon* w = o->getCurrentWeapon();
			printf("[BFMETEST] state #%u %s loco=%s maxspeed=%.2f moving=%d attacking=%d idle=%d victim=%u weapon=%s range=%.0f pos=%.0f,%.0f waitPath=%d path=%d blocked=%d stuck=%d disabled=%d phys=%d vel=%.2f mass=%.1f\n", o->getID(),
				o->getTemplate()->getName().str(), loco ? loco->getTemplateName().str() : "-",
				loco ? loco->getMaxSpeedForCondition(BODY_PRISTINE) : 0.0f, (int)ai->isMoving(), (int)ai->isAttacking(), (int)ai->isIdle(),
				v ? v->getID() : 0, w ? w->getName().str() : "-", w ? w->getAttackRange(o) : 0.0f, o->getPosition()->x, o->getPosition()->y,
				(int)ai->isWaitingForPath(), ai->getPath() != nullptr, ai->getNumFramesBlocked(), (int)ai->isBlockedAndStuck(),
				(int)o->isDisabled(), o->getPhysics() != nullptr, o->getPhysics() ? o->getPhysics()->getVelocityMagnitude() : 0.0f,
				o->getPhysics() ? o->getPhysics()->getMass() : 0.0f);
		}
	}
	else if (op == "power" && c.args.size() >= 5)
	{
		// power PLAYER TEMPLATE SPECIALPOWER ENEMYPLAYER : use a special power on the closest enemy
		Player* a = findPlayer(c.args[1]);
		const Bool noTarget = c.args[4] == "none";
		Player* b = noTarget ? nullptr : findPlayer(c.args[4]);
		const SpecialPowerTemplate* sp = TheSpecialPowerStore->findSpecialPowerTemplate(c.args[3].c_str());
		if (!a || (!b && !noTarget) || !sp)
		{
			printf("[BFMETEST] power: unknown player or power %s\n", c.args[3].c_str());
			return;
		}
		for (Object* o = TheGameLogic->getFirstObject(); o; o = o->getNextObject())
		{
			if (o->getControllingPlayer() != a || o->isEffectivelyDead() || o->getTemplate()->getName().compareNoCase(c.args[2].c_str()) != 0)
				continue;
			SpecialPowerModuleInterface* m = o->getSpecialPowerModule(sp);
			if (m == nullptr)
			{
				printf("[BFMETEST] power: #%u has no module for %s\n", o->getID(), c.args[3].c_str());
				continue;
			}
			m->pauseCountdown(FALSE);
			m->setReadyFrame(TheGameLogic->getFrame());
			Object* best = nullptr;
			Real bestD = 0.0f;
			for (Object* t = TheGameLogic->getFirstObject(); t; t = t->getNextObject())
			{
				if (b == nullptr || t->getControllingPlayer() != b || t->isEffectivelyDead() || t->isKindOf(KINDOF_HORDE))
					continue;
				Real dx = t->getPosition()->x - o->getPosition()->x, dy = t->getPosition()->y - o->getPosition()->y;
				if (best == nullptr || dx * dx + dy * dy < bestD)
				{
					best = t;
					bestD = dx * dx + dy * dy;
				}
			}
			if (best)
				m->doSpecialPowerAtObject(best, 0);
			else
				m->doSpecialPower(0);
			printf("[BFMETEST] power: #%u used %s on #%u\n", o->getID(), c.args[3].c_str(), best ? best->getID() : 0);
		}
	}
	else if (op == "kill" && c.args.size() >= 3)
	{
		// kill PLAYER TEMPLATE : lethal damage to the player's objects of that template
		Player* a = findPlayer(c.args[1]);
		for (Object* o = TheGameLogic->getFirstObject(); o; o = o->getNextObject())
			if (o->getControllingPlayer() == a && !o->isEffectivelyDead() && o->getTemplate()->getName().compareNoCase(c.args[2].c_str()) == 0)
			{
				o->kill();
				printf("[BFMETEST] killed #%u %s dead=%d\n", o->getID(), c.args[2].c_str(), (int)o->isEffectivelyDead());
			}
	}
	else if (op == "revive" && c.args.size() >= 3)
	{
		// revive PLAYER TEMPLATE [X Y] : bring a fallen hero back (as a fortress would)
		Player* a = findPlayer(c.args[1]);
		for (Object* o = TheGameLogic->getFirstObject(); o; o = o->getNextObject())
		{
			RespawnUpdate* r = RespawnUpdate::findOn(o);
			if (o->getControllingPlayer() != a || r == nullptr || !r->isFallen() || o->getTemplate()->getName().compareNoCase(c.args[2].c_str()) != 0)
				continue;
			Coord3D pos;
			if (c.args.size() >= 5)
			{
				pos.x = (Real)atof(c.args[3].c_str());
				pos.y = (Real)atof(c.args[4].c_str());
				pos.z = 0.0f;
			}
			Object* n = r->revive(c.args.size() >= 5 ? &pos : nullptr);
			printf("[BFMETEST] revived #%u as %s\n", n ? n->getID() : 0, n ? n->getTemplate()->getName().str() : "-");
			break;
		}
	}
	else if (op == "xp")
	{
		// xp : rank and experience of every trainable unit; xp PLAYER TEMPLATE N : give experience
		if (c.args.size() >= 4)
		{
			Player* a = findPlayer(c.args[1]);
			for (Object* o = TheGameLogic->getFirstObject(); o; o = o->getNextObject())
				if (o->getControllingPlayer() == a && !o->isEffectivelyDead() && o->getExperienceTracker() &&
						o->getTemplate()->getName().compareNoCase(c.args[2].c_str()) == 0)
					o->getExperienceTracker()->addExperiencePoints(atoi(c.args[3].c_str()), FALSE);
		}
		for (Object* o = TheGameLogic->getFirstObject(); o; o = o->getNextObject())
		{
			const ExperienceTracker* x = o->getExperienceTracker();
			if (x && o->getAI() && !o->isEffectivelyDead())
				printf("[BFMETEST] xp #%u %s rank=%d exp=%d award=%d hasLevel2=%d\n", o->getID(), o->getTemplate()->getName().str(), x->getBfmeRank(),
					x->getCurrentExperience(), (TheExperienceLevelStore && TheExperienceLevelStore->getLevel(o->getTemplate(), x->getBfmeRank())) ? TheExperienceLevelStore->getLevel(o->getTemplate(), x->getBfmeRank())->m_experienceAward : -1, (int)(TheUpgradeCenter->findUpgrade("Upgrade_Level_2") && o->hasUpgrade(TheUpgradeCenter->findUpgrade("Upgrade_Level_2"))));
		}
	}
	else if (op == "produce" && c.args.size() >= 4)
	{
		// produce PLAYER FACTORYTEMPLATE UNITTEMPLATE : queue a unit as the player's UI would (cost, limits checked)
		Player* a = findPlayer(c.args[1]);
		const ThingTemplate* t = TheThingFactory->findTemplate(c.args[3].c_str());
		for (Object* o = TheGameLogic->getFirstObject(); o && a && t; o = o->getNextObject())
		{
			if (o->getControllingPlayer() != a || o->isEffectivelyDead() || o->getTemplate()->getName().compareNoCase(c.args[2].c_str()) != 0)
				continue;
			ProductionUpdateInterface* pu = o->getProductionUpdateInterface();
			const CanMakeType can = TheBuildAssistant->canMakeUnit(o, t);
			Bool queued = FALSE;
			if (pu && can == CANMAKE_OK)
			{
				queued = pu->queueCreateUnit(t, pu->requestUniqueUnitID());	// charges the cost
			}
			printf("[BFMETEST] produce %s at #%u: canMake=%d queued=%d (build time %d frames)\n", c.args[3].c_str(), o->getID(), (int)can, (int)queued, t->calcTimeToBuild(a));
			break;
		}
	}
	else if (op == "stance" && c.args.size() >= 4)
	{
		// stance PLAYER TEMPLATE STANCE|none
		Player* a = findPlayer(c.args[1]);
		for (Object* o = TheGameLogic->getFirstObject(); o; o = o->getNextObject())
		{
			StancesBehavior* sb = StancesBehavior::findOn(o);
			if (sb == nullptr || o->getControllingPlayer() != a || o->getTemplate()->getName().compareNoCase(c.args[2].c_str()) != 0)
				continue;
			const Bool ok = sb->setStance(c.args[3] == "none" ? AsciiString::TheEmptyString : AsciiString(c.args[3].c_str()));
			printf("[BFMETEST] stance #%u %s -> %s ok=%d\n", o->getID(), c.args[2].c_str(), c.args[3].c_str(), (int)ok);
		}
	}
	else if (op == "screenshot")
	{
		// screenshot : PNG of the current frame into the user data dir (rendering builds only)
		if (TheDisplay)
		{
			TheDisplay->takeScreenShot(SCREENSHOT_PNG);
			printf("[BFMETEST] screenshot frame=%u\n", TheGameLogic->getFrame());
		}
	}
	else if (op == "camera" && c.args.size() >= 3)
	{
		// camera X Y [ZOOM] : look at a point
		if (TheTacticalView)
		{
			Coord3D pos;
			pos.x = (Real)atof(c.args[1].c_str());
			pos.y = (Real)atof(c.args[2].c_str());
			pos.z = 0.0f;
			TheTacticalView->lookAt(&pos);
			if (c.args.size() >= 4)
				TheTacticalView->setZoom((Real)atof(c.args[3].c_str()));
		}
	}
	else if (op == "money")
	{
		for (Int i = 0; i < ThePlayerList->getPlayerCount(); ++i)
		{
			Player* pl = ThePlayerList->getNthPlayer(i);
			AsciiString n = TheNameKeyGenerator->keyToName(pl->getPlayerNameKey());
			if (n.isNotEmpty())
				printf("[BFMETEST] money frame=%u %s %u commandpoints=%d/%d\n", TheGameLogic->getFrame(), n.str(), pl->getMoney()->countMoney(),
					pl->bfmeCommandPointsUsed(), pl->bfmeCommandPointLimit());
		}
	}
	else if (op == "players")
	{
		for (Int i = 0; i < ThePlayerList->getPlayerCount(); ++i)
		{
			Player* p = ThePlayerList->getNthPlayer(i);
			AsciiString n = TheNameKeyGenerator->keyToName(p->getPlayerNameKey());
			printf("[BFMETEST] player %d name='%s' template=%s\n", i, n.str(),
				p->getPlayerTemplate() ? p->getPlayerTemplate()->getName().str() : "-");
		}
	}
	else if (op == "quit")
	{
		printf("[BFMETEST] quit at frame %u\n", TheGameLogic->getFrame());
		TheGameEngine->setQuitting(TRUE);
	}
	else
	{
		printf("[BFMETEST] unknown command '%s'\n", op.c_str());
	}
	fflush(stdout);
}
} // namespace

void bfmeTestHarnessUpdate()
{
	if (!s_loaded)
		loadScript();
	if (s_next >= s_cmds.size())
		return;
	const UnsignedInt now = TheGameLogic->getFrame();
	while (s_next < s_cmds.size() && s_cmds[s_next].frame <= now)
		run(s_cmds[s_next++]);
}
