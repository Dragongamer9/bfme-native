// bfme-native: RotWK hordes, see HordeContain.h
#include "PreRTS.h"

#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameLogic/AI.h"
#include "GameLogic/AttributeModifier.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Weapon.h"

static const UnsignedInt HORDE_THINK_FRAMES = 5;

//-------------------------------------------------------------------------------------------------
HordeContainModuleData::HordeContainModuleData() :
	m_slots(0),
	m_showPips(FALSE),
	m_ranksToReleaseWhenAttacking(0),
	m_ranksToJustFreeWhenAttacking(0),
	m_meleeAttackLeashDistance(0.0f),
	m_thisFormationIsTheMainFormation(FALSE),
	m_minimumHordeSize(0),
	m_unusedReal(0.0f),
	m_unusedBool(FALSE),
	m_unusedInt(0)
{
	m_randomOffset.x = m_randomOffset.y = 0.0f;
	m_unusedCoord.zero();
}

void HordeContainModuleData::parseInitialPayload(INI* ini, void* /*instance*/, void* store, const void* /*userData*/)
{
	// "InitialPayload = LAGondorFighter 10"
	std::vector<HordePayload>* v = (std::vector<HordePayload>*)store;
	HordePayload p;
	p.m_templateName = ini->getNextToken();
	const char* n = ini->getNextTokenOrNull();
	p.m_count = n ? INI::scanInt(n) : 1;
	v->push_back(p);
}

static Real scanTaggedReal(const char* tok, const char* tag)
{
	// "X:15" -> 15
	const size_t n = strlen(tag);
	if (strnicmp(tok, tag, n) == 0)
		return INI::scanReal(tok + n);
	return INI::scanReal(tok);
}

void HordeContainModuleData::parseRankInfo(INI* ini, void* /*instance*/, void* store, const void* /*userData*/)
{
	// "RankInfo = RankNumber:1 UnitType:LAGondorFighter Position:X:15 Y:0 Position:X:15 Y:25 ..."
	// (also seen: "Position:X: 0 Y:0" with the number split off, and "Leader 1 0" annotations)
	std::vector<AsciiString> toks;
	for (const char* tok = ini->getNextTokenOrNull(); tok; tok = ini->getNextTokenOrNull())
	{
		AsciiString t = tok;
		if (!toks.empty() && toks.back().getLength() > 0 && toks.back().getCharAt(toks.back().getLength() - 1) == ':')
			toks.back().concat(t);	// glue "X:" + "0"
		else
			toks.push_back(t);
	}
	std::vector<HordeRankInfo>* v = (std::vector<HordeRankInfo>*)store;
	HordeRankInfo r;
	r.m_rankNumber = 0;
	for (size_t i = 0; i < toks.size(); ++i)
	{
		const char* tok = toks[i].str();
		if (strnicmp(tok, "RankNumber:", 11) == 0)
			r.m_rankNumber = atoi(tok + 11);
		else if (strnicmp(tok, "UnitType:", 9) == 0)
			r.m_unitType = tok + 9;
		else if (strnicmp(tok, "Position:", 9) == 0)
		{
			Coord2D c;
			const char* x = tok + 9;
			c.x = (Real)atof(strnicmp(x, "X:", 2) == 0 ? x + 2 : x);
			c.y = 0.0f;
			if (i + 1 < toks.size() && strnicmp(toks[i + 1].str(), "Y:", 2) == 0)
				c.y = (Real)atof(toks[++i].str() + 2);
			r.m_positions.push_back(c);
		}
	}
	v->push_back(r);
}

void HordeContainModuleData::parseRandomOffset(INI* ini, void* /*instance*/, void* store, const void* /*userData*/)
{
	// "RandomOffset = X:4 Y:4"
	Coord2D* c = (Coord2D*)store;
	const char* x = ini->getNextToken();
	c->x = scanTaggedReal(x, "X:");
	const char* y = ini->getNextTokenOrNull();
	c->y = y ? scanTaggedReal(y, "Y:") : c->x;
}

void HordeContainModuleData::parseLine(INI* ini, void* /*instance*/, void* /*store*/, const void* /*userData*/)
{
	// swallow the rest of the line (fields we parse but don't use yet)
	while (ini->getNextTokenOrNull()) {}
}

void HordeContainModuleData::parseSubBlock(INI* ini, void* /*instance*/, void* /*store*/, const void* /*userData*/)
{
	// "MeleeBehavior = Amoeba ... End": a nested block we don't use yet
	while (ini->getNextTokenOrNull()) {}
	static const FieldParse none[] = { { nullptr, nullptr, nullptr, 0 } };
	char dummy[16];
	ini->initFromINI(dummy, none);
}

void HordeContainModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse dataFieldParse[] =
	{
		{ "InitialPayload",									parseInitialPayload,	nullptr, offsetof( HordeContainModuleData, m_initialPayload ) },
		{ "Slots",													INI::parseInt,				nullptr, offsetof( HordeContainModuleData, m_slots ) },
		{ "PassengerFilter",								ObjectFilter::parseFromINI, nullptr, offsetof( HordeContainModuleData, m_passengerFilter ) },
		{ "ShowPips",												INI::parseBool,				nullptr, offsetof( HordeContainModuleData, m_showPips ) },
		{ "ObjectStatusOfContained",				parseLine,						nullptr, 0 },
		{ "RankInfo",												parseRankInfo,				nullptr, offsetof( HordeContainModuleData, m_rankInfo ) },
		{ "RandomOffset",										parseRandomOffset,		nullptr, offsetof( HordeContainModuleData, m_randomOffset ) },
		{ "RanksToReleaseWhenAttacking",		INI::parseInt,				nullptr, offsetof( HordeContainModuleData, m_ranksToReleaseWhenAttacking ) },
		{ "RanksToJustFreeWhenAttacking",		INI::parseInt,				nullptr, offsetof( HordeContainModuleData, m_ranksToJustFreeWhenAttacking ) },
		{ "MeleeAttackLeashDistance",				INI::parseReal,				nullptr, offsetof( HordeContainModuleData, m_meleeAttackLeashDistance ) },
		{ "ThisFormationIsTheMainFormation",	INI::parseBool,				nullptr, offsetof( HordeContainModuleData, m_thisFormationIsTheMainFormation ) },
		{ "AttributeModifiers",							INI::parseAsciiStringVectorAppend, nullptr, offsetof( HordeContainModuleData, m_attributeModifiers ) },
		{ "BannerCarriersAllowed",					INI::parseAsciiStringVectorAppend, nullptr, offsetof( HordeContainModuleData, m_bannerCarriersAllowed ) },
		{ "MinimumHordeSize",								INI::parseInt,				nullptr, offsetof( HordeContainModuleData, m_minimumHordeSize ) },
		{ "BannerCarrierPosition",					parseLine,						nullptr, 0 },
		{ "BannerCarrierDestroyHordeOnDeath", INI::parseBool,			nullptr, offsetof( HordeContainModuleData, m_unusedBool ) },
		{ "BannerCarrierHordeDeathType",		parseLine,						nullptr, 0 },
		{ "BannerCarrierMinLevel",					parseLine,						nullptr, 0 },
		{ "RanksThatStopAdvance",						parseLine,						nullptr, 0 },
		{ "ComboHorde",											parseLine,						nullptr, 0 },
		{ "AlternateFormation",							INI::parseAsciiString,	nullptr, offsetof( HordeContainModuleData, m_unusedString ) },
		{ "LeaderPosition",									parseLine,						nullptr, 0 },
		{ "LeadersAllowed",									parseLine,						nullptr, 0 },
		{ "LeaderRank",											parseLine,						nullptr, 0 },
		{ "BackUpMinDelayTime",							parseLine,						nullptr, 0 },
		{ "BackUpMaxDelayTime",							parseLine,						nullptr, 0 },
		{ "BackUpMinDistance",							parseLine,						nullptr, 0 },
		{ "BackUpMaxDistance",							parseLine,						nullptr, 0 },
		{ "BackupPercentage",								parseLine,						nullptr, 0 },
		{ "CowerRadius",										parseLine,						nullptr, 0 },
		{ "IsPorcupineFormation",						parseLine,						nullptr, 0 },
		{ "ForcedLocomotorSet",							parseLine,						nullptr, 0 },
		{ "MachineAllowed",									parseLine,						nullptr, 0 },
		{ "MachineType",										parseLine,						nullptr, 0 },
		{ "UseSlowHordeMovement",						parseLine,						nullptr, 0 },
		{ "SplitHorde",											parseLine,						nullptr, 0 },
		{ "EvaEventLastMemberDeath",				parseLine,						nullptr, 0 },
		{ "RankSplit",											parseLine,						nullptr, 0 },
		{ "SplitHordeNumber",								parseLine,						nullptr, 0 },
		{ "NotComboFormation",							parseLine,						nullptr, 0 },
		{ "UseMarchingAnims",								parseLine,						nullptr, 0 },
		{ "FrontAngle",											parseLine,						nullptr, 0 },
		{ "FlankedDelay",										parseLine,						nullptr, 0 },
		{ "FlankedDuration",								parseLine,						nullptr, 0 },
		{ "MeleeBehavior",									parseSubBlock,				nullptr, 0 },
		{ "VisionRearOverride",							parseLine,						nullptr, 0 },
		{ "VisionSideOverride",							parseLine,						nullptr, 0 },
		{ "LivingWorldOverloadTemplate",		parseLine,						nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(dataFieldParse);
}

//-------------------------------------------------------------------------------------------------
HordeContain::HordeContain( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData ),
	m_spawned(FALSE)
{
	setWakeFrame(getObject(), UPDATE_SLEEP(1));
}

HordeContain::~HordeContain()
{
}

HordeContain* HordeContain::findOn(const Object* obj)
{
	if (obj == nullptr)
		return nullptr;
	for (BehaviorModule** m = obj->getBehaviorModules(); *m; ++m)
	{
		HordeContain* hc = dynamic_cast<HordeContain*>(*m);
		if (hc)
			return hc;
	}
	return nullptr;
}

const ThingTemplate* bfmeHordeMemberTemplate(const ThingTemplate* hordeTemplate)
{
	if (hordeTemplate == nullptr)
		return nullptr;
	const ModuleInfo& mi = hordeTemplate->getBehaviorModuleInfo();
	for (Int i = 0; i < mi.getCount(); ++i)
	{
		const HordeContainModuleData* d = dynamic_cast<const HordeContainModuleData*>(mi.getNthData(i));
		if (d && !d->m_initialPayload.empty())
			return TheThingFactory->findTemplate(d->m_initialPayload[0].m_templateName);
	}
	return nullptr;
}

void HordeContain::slotPosition(Int index, Coord3D* out) const
{
	const HordeContainModuleData* d = getHordeContainModuleData();
	const Object* me = getObject();

	// RankInfo positions in rank order; anything beyond them goes into extra rows behind
	Coord2D local;
	local.x = local.y = 0.0f;
	Int n = index;
	Bool found = FALSE;
	for (size_t r = 0; r < d->m_rankInfo.size() && !found; ++r)
	{
		const std::vector<Coord2D>& pos = d->m_rankInfo[r].m_positions;
		if (n < (Int)pos.size())
		{
			local = pos[n];
			found = TRUE;
		}
		else
		{
			n -= (Int)pos.size();
		}
	}
	if (!found)
	{
		const Int perRow = 5;
		local.x = -15.0f - 15.0f * (Real)(n / perRow);
		local.y = 25.0f * (Real)((n % perRow) - perRow / 2);
	}

	const Real a = me->getOrientation();
	const Real c = Cos(a), s = Sin(a);
	out->x = me->getPosition()->x + local.x * c - local.y * s;
	out->y = me->getPosition()->y + local.x * s + local.y * c;
	out->z = TheTerrainLogic ? TheTerrainLogic->getGroundHeight(out->x, out->y) : me->getPosition()->z;
}

void HordeContain::spawnMembers()
{
	const HordeContainModuleData* d = getHordeContainModuleData();
	Object* me = getObject();
	Int index = 0;
	for (size_t p = 0; p < d->m_initialPayload.size(); ++p)
	{
		const ThingTemplate* tmpl = TheThingFactory->findTemplate(d->m_initialPayload[p].m_templateName);
		if (tmpl == nullptr)
			continue;
		for (Int i = 0; i < d->m_initialPayload[p].m_count; ++i, ++index)
		{
			Object* m = TheThingFactory->newObject(tmpl, me->getTeam());
			if (m == nullptr)
				continue;
			Coord3D pos;
			slotPosition(index, &pos);
			m->setPosition(&pos);
			m->setOrientation(me->getOrientation());
			m->setProducer(me);
			m_members.push_back(m->getID());
			m_syncedUpgrades.clear();	// hand the horde's upgrades to the newcomer on the next update
			m_lastOrder.push_back(pos);

			for (size_t am = 0; am < d->m_attributeModifiers.size(); ++am)
				TheAttributeModifierStore->apply(m, TheAttributeModifierStore->findList(d->m_attributeModifiers[am]), me->getID(), 0, TRUE);
		}
	}
	m_spawned = TRUE;

	// the horde is a leader object standing among its own soldiers: it must not wait for them to
	// get out of its way
	if (me->getAI())
		me->getAI()->setCanPathThroughUnits(TRUE);
}

Int HordeContain::getMemberCount() const
{
	Int n = 0;
	for (size_t i = 0; i < m_members.size(); ++i)
	{
		const Object* m = TheGameLogic->findObjectByID(m_members[i]);
		if (m && !m->isEffectivelyDead())
			++n;
	}
	return n;
}

Object* HordeContain::getMemberClosestTo(const Coord3D* pos) const
{
	Object* best = nullptr;
	Real bestD = 0.0f;
	for (size_t i = 0; i < m_members.size(); ++i)
	{
		Object* m = TheGameLogic->findObjectByID(m_members[i]);
		if (m == nullptr || m->isEffectivelyDead())
			continue;
		Real dx = m->getPosition()->x - pos->x, dy = m->getPosition()->y - pos->y;
		Real dd = dx * dx + dy * dy;
		if (best == nullptr || dd < bestD)
		{
			best = m;
			bestD = dd;
		}
	}
	return best;
}

UpdateSleepTime HordeContain::update()
{
	Object* me = getObject();
	if (!m_spawned)
		spawnMembers();
	// a container that is not a horde (citadel/ring "slaughter" containers on heroes and buildings)
	if (getHordeContainModuleData()->m_initialPayload.empty() && !me->isKindOf(KINDOF_HORDE))
		return UPDATE_SLEEP_FOREVER;

	// drop the dead; the horde goes when its last member does
	for (size_t i = 0; i < m_members.size(); )
	{
		Object* m = TheGameLogic->findObjectByID(m_members[i]);
		if (m == nullptr || m->isEffectivelyDead())
		{
			m_members.erase(m_members.begin() + i);
			m_lastOrder.erase(m_lastOrder.begin() + i);
		}
		else
		{
			++i;
		}
	}
	if (m_members.empty())
	{
		TheGameLogic->destroyObject(me);
		return UPDATE_SLEEP_FOREVER;
	}
	// upgrades bought for the horde (armor, blades...) are object upgrades on its members
	if (me->getObjectCompletedUpgradeMask() != m_syncedUpgrades)
		syncUpgrades();

	AIUpdateInterface* ai = me->getAI();
	Object* victim = ai ? ai->getCurrentVictim() : nullptr;
	const Bool hordeMoving = ai && ai->isMoving();

	for (size_t i = 0; i < m_members.size(); ++i)
	{
		Object* m = TheGameLogic->findObjectByID(m_members[i]);
		AIUpdateInterface* mai = m ? m->getAI() : nullptr;
		if (mai == nullptr)
			continue;

		if (victim && !victim->isEffectivelyDead())
		{
			// the horde is attacking: each member picks the closest enemy soldier of the target
			if (!mai->isAttacking())
			{
				Object* target = victim;
				HordeContain* enemyHorde = victim->isKindOf(KINDOF_HORDE) ? findOn(victim) : nullptr;
				if (enemyHorde)
					target = enemyHorde->getMemberClosestTo(m->getPosition());
				if (target)
					mai->aiAttackObject(target, NO_MAX_SHOTS_LIMIT, CMD_FROM_AI);
				if (getenv("BFME_DEBUG_HORDE"))
					fprintf(stderr, "[horde] #%u member #%u -> target #%u (victim #%u, enemyHorde=%d) attacking=%d idle=%d\n", me->getID(), m->getID(),
						target ? target->getID() : 0, victim->getID(), enemyHorde != nullptr, (int)mai->isAttacking(), (int)mai->isIdle());
			}
			if (mai->isAttacking())
				continue;
			// the target isn't attackable yet (out of sight): keep formation while the horde closes in
		}

		if (mai->isAttacking() && !hordeMoving)
			continue;	// let them finish fights they picked up on their own while the horde stands

		Coord3D slot;
		slotPosition((Int)i, &slot);
		const Real dx = m->getPosition()->x - slot.x, dy = m->getPosition()->y - slot.y;
		const Real ox = m_lastOrder[i].x - slot.x, oy = m_lastOrder[i].y - slot.y;
		const Bool farFromSlot = dx * dx + dy * dy > 3.0f * 3.0f;
		const Bool slotMoved = ox * ox + oy * oy > 2.0f * 2.0f;
		if (farFromSlot && (slotMoved || mai->isIdle() || mai->isAttacking()))
		{
			mai->aiMoveToPosition(&slot, CMD_FROM_AI);
			m_lastOrder[i] = slot;
		}
		else if (!farFromSlot && mai->isIdle() && !hordeMoving)
		{
			m->setOrientation(me->getOrientation());
		}
	}
	return UPDATE_SLEEP(HORDE_THINK_FRAMES);
}

void HordeContain::crc( Xfer *xfer )
{
	UpdateModule::crc( xfer );
}

void HordeContain::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	UpdateModule::xfer( xfer );
	xfer->xferBool(&m_spawned);
	UnsignedShort count = (UnsignedShort)m_members.size();
	xfer->xferUnsignedShort(&count);
	if (xfer->getXferMode() == XFER_LOAD)
	{
		m_members.resize(count);
		m_lastOrder.resize(count);
	}
	for (UnsignedShort i = 0; i < count; ++i)
	{
		xfer->xferObjectID(&m_members[i]);
		xfer->xferCoord3D(&m_lastOrder[i]);
	}
}

void HordeContain::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}

void HordeContain::syncUpgrades()
{
	Object* me = getObject();
	const UpgradeMaskType& now = me->getObjectCompletedUpgradeMask();
	for (const UpgradeTemplate* ut = TheUpgradeCenter->firstUpgradeTemplate(); ut; ut = ut->friend_getNext())
	{
		if (ut->getUpgradeType() != UPGRADE_TYPE_OBJECT)
			continue;
		const Bool has = now.testForAny(ut->getUpgradeMask());
		const Bool had = m_syncedUpgrades.testForAny(ut->getUpgradeMask());
		if (has == had)
			continue;
		for (size_t i = 0; i < m_members.size(); ++i)
		{
			Object* m = TheGameLogic->findObjectByID(m_members[i]);
			if (m == nullptr)
				continue;
			if (has && !m->hasUpgrade(ut))
				m->giveUpgrade(ut);
			else if (!has && m->hasUpgrade(ut))
				m->removeUpgrade(ut);
		}
	}
	m_syncedUpgrades = now;
}
